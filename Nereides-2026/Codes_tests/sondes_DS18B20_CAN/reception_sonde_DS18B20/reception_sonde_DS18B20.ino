#include <Wire.h>
#include <ESP32-TWAI-CAN.hpp>

#define CAN_TX 5
#define CAN_RX 4

// Identifiant CAN utilisé pour transmettre la température
#define CAN_ID_TEMP_SENSOR 0x013  

// Sonde de température DS18B20
#define TEMP_SENSOR_PIN 4
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature ds(&oneWire);

CanFrame rxFrame, txFrame;

// --------- Décodage d’une trame CAN reçue ----------
void decodeCanFrame(const CanFrame &frame) {
    if (frame.identifier == CAN_ID_TEMP_SENSOR && frame.data_length_code >= 2) {
        // Reconstruction de la valeur envoyée (little-endian)
        int16_t tempCentidegrees = frame.data[0] | (frame.data[1] << 8);
        float temperature = tempCentidegrees / 100.0;

        Serial.printf("Trame reçue -> ID: 0x%03X | Température: %.2f°C\n", 
                      frame.identifier, temperature);

        // Debug : affichage brut
        Serial.printf("Données reçues [DEC]: %d %d %d %d\n", 
                      frame.data[0], frame.data[1], frame.data[2], frame.data[3]);
    }
}

void setup() {
    Serial.begin(115200);

    // Initialisation CAN
    ESP32Can.setPins(CAN_TX, CAN_RX);
    ESP32Can.setRxQueueSize(5);
    ESP32Can.setTxQueueSize(5);
    ESP32Can.setSpeed(ESP32Can.convertSpeed(250));

    if (ESP32Can.begin()) {
        Serial.println("CAN bus started!");
    } else {
        Serial.println("CAN bus failed!");
    }
}

void loop() {
    // Envoi température toutes les secondes

    // Lecture et décodage des trames reçues
    if (ESP32Can.readFrame(rxFrame, 100)) {
        decodeCanFrame(rxFrame);
    }

    delay(1000);
}
