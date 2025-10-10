#include <ESP32-TWAI-CAN.hpp>

#define CAN_TX 5
#define CAN_RX 4

CanFrame rxFrame;

// ---------- Fonction utilitaire pour afficher une trame brute ----------
void printCanFrame(const CanFrame &frame) {
    Serial.println("Nouvelle trame reçue :");
    Serial.printf("CAN ID: %03X\n", frame.identifier);

    // Trame en HEX
    Serial.print("Trame en HEX :");
    for (int i = 0; i < frame.data_length_code; i++) {
        Serial.printf(" %02X", frame.data[i]);
    }
    Serial.println();

    // Trame en binaire
    Serial.print("Trame en binaire :");
    for (int i = 0; i < frame.data_length_code; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            Serial.print((frame.data[i] >> bit) & 0x01);
        }
        Serial.print(" "); // séparateur entre octets
    }
    Serial.println();
    Serial.println("------------------------------------------------------------------------------------------------------");
}

void setup() {
    Serial.begin(115200);

    ESP32Can.setPins(CAN_TX, CAN_RX);
    ESP32Can.setRxQueueSize(5);
    ESP32Can.setTxQueueSize(5);
    ESP32Can.setSpeed(ESP32Can.convertSpeed(250));

    if (ESP32Can.begin()) {
        Serial.println("CAN bus started!");
    } else {
        Serial.println("CAN bus failed!");
    }

    if (ESP32Can.begin(ESP32Can.convertSpeed(250), CAN_TX, CAN_RX, 10, 10)) {
        Serial.println("CAN bus started!");
    } else {
        Serial.println("CAN bus failed!");
    }
}

void loop() {
    if (ESP32Can.readFrame(rxFrame, 1000)) {
        printCanFrame(rxFrame);
    }
}
