#if defined(ESP32)
  const int LED_PIN = 2;  // LED onboard padrão na maioria dos ESP32
#else
  const int LED_PIN = 13; // LED 'L' onboard no Arduino Uno / Nano
#endif

bool modoPiscar = true;
bool estadoLed = false;
unsigned long ultimoTempo = 0;
const unsigned long intervaloMs = 500;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  delay(800);
  Serial.println("FIRMWARE_PRONTO: Aguardando comandos do SerialBridge...");
}

void loop() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "LED_ON") {
      modoPiscar = false;
      estadoLed = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println("OK: LED LIGADO FIXO");
    } 
    else if (cmd == "LED_OFF" || cmd == "STOP") {
      modoPiscar = false;
      estadoLed = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println("OK: LED DESLIGADO");
    } 
    else if (cmd == "BLINK_ON") {
      modoPiscar = true;
      Serial.println("OK: PISCA-PISCA ATIVADO");
    } 
    else if (cmd == "STATUS") {
      Serial.print("STATUS -> Piscar: ");
      Serial.print(modoPiscar ? "SIM" : "NAO");
      Serial.print(" | LED: ");
      Serial.println(estadoLed ? "ACESO" : "APAGADO");
    } 
    else if (cmd.length() > 0) {
      Serial.println("ECO: " + cmd);
    }
  }

  if (modoPiscar && (millis() - ultimoTempo >= intervaloMs)) {
    ultimoTempo = millis();
    estadoLed = !estadoLed;
    digitalWrite(LED_PIN, estadoLed ? HIGH : LOW);
  }
}
