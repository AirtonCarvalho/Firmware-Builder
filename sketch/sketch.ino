#include <Wire.h>
#include <WiFi.h>
#include "BluetoothSerial.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Pinos internos do OLED 0.96" na placa ideaspark ML-ESP32-096OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

BluetoothSerial SerialBT;
WiFiServer tcpServer(8080);
WiFiClient tcpClient;

String mensagemAtual = "Ola Airton!";
String origemAtual = "PRONTO";
int tamanhoFonte = 2;       // 1 = Pequena, 2 = Média, 3 = Grande
bool telaInvertida = false;
bool modoLetreiro = false;
bool modoRadar = false;

int posScrollX = 128;
int anguloRadar = 0;
unsigned long ultimoFrame = 0;

void desenharTelaEstatica() {
  display.clearDisplay();
  display.invertDisplay(telaInvertida);
  display.setTextColor(SSD1306_WHITE);

  // Barra superior com origem da mensagem e tamanho da fonte
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("VIA: ");
  display.print(origemAtual);
  display.setCursor(96, 0);
  display.print("F:");
  display.print(tamanhoFonte);
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // Área principal para sua mensagem
  display.setTextSize(tamanhoFonte);
  display.setCursor(0, 18);
  display.println(mensagemAtual);

  display.display();
}

void animarLetreiro() {
  display.clearDisplay();
  display.invertDisplay(telaInvertida);
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("LETREIRO [");
  display.print(origemAtual);
  display.print("]");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(posScrollX, 28);
  display.print(mensagemAtual);
  display.setTextWrap(true);
  display.display();

  posScrollX -= 4;
  int larguraTexto = mensagemAtual.length() * 12;
  if (posScrollX < -larguraTexto) {
    posScrollX = 128;
  }
}

void animarRadar() {
  display.clearDisplay();
  display.invertDisplay(telaInvertida);

  int cx = 32, cy = 32, r = 28;
  display.drawCircle(cx, cy, r, SSD1306_WHITE);
  display.drawCircle(cx, cy, r / 2, SSD1306_WHITE);
  display.drawLine(cx - r, cy, cx + r, cy, SSD1306_WHITE);
  display.drawLine(cx, cy - r, cx, cy + r, SSD1306_WHITE);

  float rad = anguloRadar * 0.0174533;
  int px = cx + cos(rad) * r;
  int py = cy + sin(rad) * r;
  display.drawLine(cx, cy, px, py, SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(68, 12);
  display.println("ESP32");
  display.setCursor(68, 24);
  display.println("OLED 0.96");
  display.setCursor(68, 40);
  display.print(anguloRadar);
  display.println(" deg");

  display.display();
  anguloRadar = (anguloRadar + 12) % 360;
}

void processarComando(String cmd, String fonte, Stream &resposta) {
  cmd.trim();
  if (cmd.length() == 0) return;

  String upper = cmd;
  upper.toUpperCase();
  origemAtual = fonte;

  // Botões rápidos do SerialBridge reaproveitados para efeitos no OLED:
  if (upper == "LED_ON") {
    telaInvertida = true;
    modoRadar = false;
    desenharTelaEstatica();
    resposta.println("OK: TELA INVERTIDA (FUNDO BRANCO)");
  }
  else if (upper == "LED_OFF") {
    telaInvertida = false;
    modoRadar = false;
    desenharTelaEstatica();
    resposta.println("OK: TELA NORMAL (FUNDO PRETO)");
  }
  else if (upper == "STOP") {
    modoLetreiro = false;
    modoRadar = false;
    desenharTelaEstatica();
    resposta.println("OK: ANIMACAO PARADA");
  }
  else if (upper == "STATUS") {
    resposta.printf("OLED -> Fonte:%d | Invertido:%s | Scroll:%s\n",
                    tamanhoFonte,
                    telaInvertida ? "SIM" : "NAO",
                    modoLetreiro ? "SIM" : "NAO");
  }
  // Slider PWM controla o tamanho da fonte (1, 2 ou 3)
  else if (upper.startsWith("PWM:")) {
    int val = upper.substring(4).toInt();
    if (val < 85) tamanhoFonte = 1;
    else if (val < 170) tamanhoFonte = 2;
    else tamanhoFonte = 3;

    modoRadar = false;
    if (!modoLetreiro) desenharTelaEstatica();
    resposta.printf("OK: TAMANHO DA FONTE = %d\n", tamanhoFonte);
  }
  // Comandos especiais digitados no campo de texto
  else if (upper == "SCROLL") {
    modoRadar = false;
    modoLetreiro = !modoLetreiro;
    posScrollX = 128;
    resposta.println(modoLetreiro ? "OK: MODO LETREIRO ATIVADO" : "OK: MODO LETREIRO DESATIVADO");
  }
  else if (upper == "RADAR") {
    modoLetreiro = false;
    modoRadar = !modoRadar;
    resposta.println(modoRadar ? "OK: ANIMACAO RADAR ATIVADA" : "OK: ANIMACAO RADAR DESATIVADA");
  }
  else if (upper == "CLEAR") {
    modoLetreiro = false;
    modoRadar = false;
    mensagemAtual = "";
    desenharTelaEstatica();
    resposta.println("OK: TELA LIMPA");
  }
  // Qualquer outro texto digitado aparece direto na tela OLED!
  else {
    modoRadar = false;
    mensagemAtual = cmd;
    if (!modoLetreiro) desenharTelaEstatica();
    resposta.println("EXIBIDO NO OLED: " + cmd);
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ERR: Display OLED nao encontrado!");
  } else {
    desenharTelaEstatica();
  }

  SerialBT.begin("ESP32_OLED");
  WiFi.softAP("ESP32_OLED_AP", "12345678");
  tcpServer.begin();

  Serial.println("OLED PRONTO! Envie qualquer texto pelo SerialBridge.");
}

void loop() {
  if (Serial.available()) {
    processarComando(Serial.readStringUntil('\n'), "USB", Serial);
  }

  if (SerialBT.available()) {
    processarComando(SerialBT.readStringUntil('\n'), "BT", SerialBT);
  }

  if (!tcpClient || !tcpClient.connected()) {
    tcpClient = tcpServer.available();
  } else if (tcpClient.available()) {
    processarComando(tcpClient.readStringUntil('\n'), "WIFI", tcpClient);
  }

  // Atualiza animações a ~25 FPS sem travar a leitura serial
  if (millis() - ultimoFrame >= 40) {
    ultimoFrame = millis();
    if (modoLetreiro) animarLetreiro();
    else if (modoRadar) animarRadar();
  }
}
