// UART0 = Serial pour le moniteur série
// UART1 = Serial1 pour la communication avec le Pi

void setup() {
  Serial.begin(115200);      // Moniteur série pour debug
  Serial1.begin(115200, SERIAL_8N1, 16, 17); // TX=16, RX=17 pour UART1 vers Pi
}

void loop() {
  // Communication avec le Pi
  if (Serial1.available()) {
    String data = Serial1.readStringUntil('\n');
    Serial.println("Reçu du Pi: " + data); // Affiche dans le moniteur
    Serial1.println("ACK: " + data);       // Répond au Pi
  }
  
  // Debug local
  Serial.println("ESP tourne...");
  delay(1000);
}
