#include <TinyGPS++.h>
#include <HardwareSerial.h> // Librairie pour pouvoir utiliser le GPS attention penser à bien les installer 

TinyGPSPlus gps;
HardwareSerial SerialGPS(1); // UART1 de l’ESP32

void setup() {
  Serial.begin(115200); // Pour lire dans le serial monitor 
  SerialGPS.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17
  Serial.println("Lecture GPS en cours...");
}

void loop() {
  // Lecture continue des données GPS
  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());
  }

  // Affichage si nouvelles données
  if (gps.location.isUpdated()) {
    Serial.println("------ Données GPS ------");
    Serial.print("Latitude  : "); Serial.println(gps.location.lat(), 6);
    Serial.print("Longitude : "); Serial.println(gps.location.lng(), 6);
    Serial.print("Satellites: "); Serial.println(gps.satellites.value());

    // 🏎️ Vitesse (en km/h)
    Serial.print("Vitesse   : ");
    Serial.print(gps.speed.kmph());
    Serial.println(" km/h");

    Serial.println("-------------------------\n");
  }
}
