#include <ESP32-TWAI-CAN.hpp>

#define CAN_TX_PIN 22
#define CAN_RX_PIN 21
// #define CAN_ID     0x102

void setup() {
    Serial.begin(115200);
    //sensors.begin();
    ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN);
    ESP32Can.setSpeed(ESP32Can.convertSpeed(500));
    if (!ESP32Can.begin()) {
      Serial.println("[TX-TEMP] CAN init FAIL");
      while (1) delay(1000);
    }
    Serial.println("[TX-TEMP] CAN ready (500k).");
}

void loop() {
  CanFrame frme;
  frme.identifier = 0x600;
  frme.data_length_code = 8;

  frme.data[0] = 0x00;
  frme.data[1] = 0x0F;
  frme.data[2] = 0xF0;
  frme.data[3] = 0xFF;
  if (!ESP32Can.writeFrame(frme, 1000)) {
    Serial.printf("Failed to send frame 0x%X \n", frme.identifier);
  }
  else {
    Serial.printf("Frame 0x%X was sent successfully\n", frme.identifier);
  }
  
  delay(1000);
}
