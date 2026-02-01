// UART0 = Serial pour le moniteur série
// UART1 = Serial1 pour la communication avec le Pi
int SOC;
int temperature;
int RPM;
int Current;

#include <ArduinoJson.h>

void setup() {
  Serial.begin(115200);      // Moniteur série pour debug
  Serial1.begin(115200, SERIAL_8N1, 16, 17); // TX=16, RX=17 pour UART1 vers Pi
}

void loop() {
  // Communication avec le Pi
SOC = random(0,100);
temperature = random(100,150);
RPM = random(6000,10000);
Current = random(100,300);

StaticJsonDocument<128> doc;
  doc["batterie"]["temperature"] = temperature;
  doc["batterie"]["SOC"] = SOC;

  doc["CM"]["Current"] = Current;
  doc["CM"]["RPM"] = RPM;
  String buffer;
  serializeJson(doc, buffer);
  Serial.println(buffer);   // 🔥 UNE seule commande
  Serial1.println(buffer);   // 🔥 UNE seule commande
  delay(1000);
}
