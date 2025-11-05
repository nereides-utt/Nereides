#include <TinyGPS++.h>
#include <HardwareSerial.h>

TinyGPSPlus gps;
HardwareSerial SerialGPS(1); // Utilise le port série 1 de l’ESP32

void setup() {
  Serial.begin(115200);
  SerialGPS.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17
  Serial.println("Lecture GPS en cours...");
}

void loop() {
  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());
  }

  if (gps.location.isUpdated()) {
    Serial.print("Latitude : "); Serial.println(gps.location.lat(), 6);
    Serial.print("Longitude : "); Serial.println(gps.location.lng(), 6);
    Serial.print("Satellites : "); Serial.println(gps.satellites.value());
  }
}
