#include <ESP32-TWAI-CAN.hpp>

#define CAN_TX_PIN 22
#define CAN_RX_PIN 21

void setup() {
  Serial.begin(115200);
  ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
  ESP32Can.setSpeed(ESP32Can.convertSpeed(500));
  if (!ESP32Can.begin()) {
    Serial.println("[RX] CAN init FAIL");
    while (1) delay(1000);
  }
  Serial.println("[RX] CAN ready (500k).");
}

void loop() {
  CanFrame f;
  while (ESP32Can.readFrame(f, 0)) {
    Serial.printf("Received frame with id 0x%X", f.identifier);
    switch (f.identifier) {
      case 0x600:
        Serial.printf("%X %X %X %X", f.data[0], f.data[1], f.data[2], f.data[3]);
        break;
    }
  }
}
