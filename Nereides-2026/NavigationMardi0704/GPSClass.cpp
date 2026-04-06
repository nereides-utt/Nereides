#include "GPSClass.hpp"

GPSClass::GPSClass(uint8_t uart_n, unsigned long uart_baud, int8_t rxPin, int8_t txPin, bool printing) : Sensor(printing), gps(), SerialGPS(uart_n) {
  longitude.val = NAN;
  latitude.val = NAN;
  speed.val = 0xFFFF;
  satellites.val = 0xFFFFFFFF;

  SerialGPS.begin(uart_baud, SERIAL_8N1, rxPin, txPin);

  if (printing) {
    //Serial.begin(115200);
    Serial.println("<-- Initialisation du GPS terminée -->");
  }
}

GPSClass::~GPSClass() {
  SerialGPS.end();
}


void GPSClass::update() {
  while (SerialGPS.available() > 0) {
    //Serial.println("BOUCLE!!");
    gps.encode(SerialGPS.read());
  }

  if (gps.location.isUpdated() && gps.location.isValid()) {

    latitude.val = gps.location.lat();
    longitude.val = gps.location.lng();
    speed.val = (uint16_t)(gps.speed.knots()*100.0f);

    satellites.val = gps.satellites.value();

    if (printing) Serial.printf("Latitude : %f | Longitude : %f | Vitesse (noeuds) : %.2f | Satellites : %lu \n", latitude.val, longitude.val, speed.val/100.0f, satellites.val);
    sendData();
  }
}

void GPSClass::sendData(){
  StaticJsonDocument<256> doc;
  doc["GPS"]["vitesse"] = speed.val/100.0f;
  doc["GPS"]["latitude"] = latitude.val;
  doc["GPS"]["longitude"] = longitude.val;
  doc["GPS"]["Satellites"] = satellites.val;
  String buffer;
  serializeJson(doc, buffer);
  //Serial.println(buffer);   //  UNE seule commande
  Serial1.println(buffer);  //  UNE seule commande
}
/*
void GPSClass::decodeCanFrame(const CanFrame &frame) {
  switch (frame.identifer)
  {
  case CANidentifier:
    for (uint8_t i = 0; i < 4; i++) {
      
      if (i < 2) { speed.bytes[i] = frame.data[i]; }
      satellites.bytes[i] = frame.data[2 + i];
    }

    if (printing) {Serial.printf("")}

    break;
  
  default:
    break;
  }


}
*/