#include "GPSClass.hpp"

GPSClass::GPSClass(uint8_t uart_n, unsigned long uart_baud, int8_t rxPin, int8_t txPin, uint32_t CANIdentifier, bool printing = false, bool receptor = false) : Sensor(CANIdentifier, printing, receptor), gps(), SerialGPS(uart_n) {
  longitude.val = NAN;
  latitude.val = NAN;
  speed.val = 0xFFFF;
  satellites.val = 0xFFFFFFFF;

  SerialGPS.begin(uart_baud, SERIAL_8N1, rxPin, txPin);

  if (printing) {
    Serial.begin(115200);
    Serial.println("<-- Initialisation du GPS terminée -->");
  }
}

GPSClass::~GPSClass() {
  SerialGPS.end();
}


void GPSClass::update() const override{
  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());
    latitude.val = gps.location.lat();
    longitude.val = gps.location.lng();
    speed.val = gps.speed.knots();

    satellites.val = gps.satellites.value();

    if (printing) Serial.printf("Latitude : %f | Longitude : %f | Vitesse (noeuds) : %.2f | Satellites : %lu\n", latitude.val, longitude.val, speed.val/100.0f, satellites);
  
  }
}

void GPSClass::sendDataViaCan() override{
  CanFrame txFrameMisc { .identifier = CANidentifier, .data_length_code = 6};
  CanFrame txFrameLongitude{ .identifier = CANidentifier + 1, .data_length_code = 8};
  CanFrame txFrameLatitude{ .identifier = CANidentifier + 2, .data_length_code = 8};

  txFrameMisc.data[0] = speed.bytes[0]; txFrameMisc.data[1] = speed.bytes[1];
  
  for (uint8_t i = 0; i < 8; i++) {
    txFrameLongitude.data[i] = longitude.bytes[i];
    txFrameLatitude.data[i] = latitude.bytes[i];
    if (i < 4) txFrameMisc.data[2 + i] = satellites.bytes[i];    
  }

  if (printing) Serial.println("Envoi des données en CAN...");

  if (!ESP32Can.writeFrame(txFrameMisc) && printing) Serial.printf("Erreur envoi de la trame txFrameMisc ! (id: %#lux)\n", txFrameMisc.identifier);
  if (!ESP32Can.writeFrame(txFrameLongitude) && printing) Serial.printf("Erreur envoi de la trame txFrameLongitude ! (id: %#lux)\n", txFrameLongitude.identifier);
  if (!ESP32Can.writeFrame(txFrameLatitude) && printing) Serial.printf("Erreur envoi de la trame txFrameLatitude ! (id: %#lux)\n", txFrameLatitude.identifier);

  if (printing) Serial.println("Fin de l'envoi des données en CAN.");

}

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