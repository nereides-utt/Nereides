#include "OneWire.h"
#include "SondeTempClass.hpp"

SondeTemp::SondeTemp(uint8_t tempPin, uint32_t CANIdentifier, bool printing = false)
  : Sensor(CANIdentifier, printing), oneWire(tempPin), ds(&oneWire) {
  temperature.val = 0xFFFF;

  if (printing) Serial.println("Initalisation de la Sonde de température...");
}

void SondeTemp::update() {
  ds.requestTemperatures();
  temperature.val = ds.getTempCByIndex(0) * 100;

  if (printing) Serial.println("Temperature lue : %.2f °C", temperature.val / 100.0f);
  if (temperature.val == 12700) {
    if (printing) Serial.println("Erreur de lecture !!");
  }
}

void SondeTemp::sendDataViaCan() const{
  if (temperature.val == 12700) { return; }

  CanFrame txFrame;
  txFrame.identifier = CANidentifier;
  txFrame.extd = 0;
  txFrame.data_length_code = 2;

  txFrame.data[0] = temperature.bytes[0];
  txFrame.data[1] = temperature.bytes[1];

  if (printing) Serial.println("Envoi des données de température en CAN...");
  if (!ESP32Can.writeFrame(txFrame) && printing) Serial.printf("Erreur envoi de la trame de température ! (id: %#lux)\n", txFrame.identifier);
  if (printing) Serial.println("Fin de l'envoi des données de température en CAN.");
}

void SondeTemp::decodeCanFrame(const CanFrame &frame) {
  if (frame.identifier != CANidentifier) return;

  temperature.bytes[0] = frame.data[0];
  temperature.bytes[1] = frame.data[1];

  if (printing) Serial.println("<-- Temperature reçue via CAN : %.2f -->", temperature.val / 100.0f);
}


void SondeTemp::
