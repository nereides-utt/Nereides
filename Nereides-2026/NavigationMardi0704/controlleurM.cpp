
#include "controlleurM.hpp"

MotorControllerData controllerData;
//Fonctions pour recevoir et stocker les données du CM dans la structure controllerData
void decodeMessage1(const CanFrame& frame) {
  controllerData.speed_rpm = (frame.data[1] << 8) | frame.data[0];
  controllerData.motor_current = ((frame.data[3] << 8) | frame.data[2]) / 10.0 - 2000;
  controllerData.battery_voltage = ((frame.data[5] << 8) | frame.data[4]) / 10.0;
  controllerData.error_code = (frame.data[7] << 8) | frame.data[6];

  //Serial.print("--- Message 1 (0x0CF11E05) ---");
  /*Serial.printf("Speed: %d RPM\n", controllerData.speed_rpm);
    Serial.printf("Motor Current: %.1f A\n", controllerData.motor_current);
    Serial.printf("Battery Voltage: %.1f V\n", controllerData.battery_voltage);
    Serial.printf("Error Code: 0x%04X\n", controllerData.error_code);*/

  for (int i = 0; i < 16; i++) {
    if (controllerData.error_code & (1 << i)) {
      Serial.printf("ERR%d: Active\n", i);
    }
  }
  

  StaticJsonDocument<256> doc;
  doc["CM"]["Current"] = controllerData.motor_current;
  doc["CM"]["RPM"] = controllerData.speed_rpm;
  doc["CM"]["Tension"] = controllerData.battery_voltage;
  doc["CM"]["Current"] = controllerData.motor_current;
  doc["CM"]["ErrorCode"] = controllerData.error_code;
  String buffer;
  serializeJson(doc, buffer);

  //Serial.println(buffer);   //  UNE seule commande
  Serial1.println(buffer);  //  UNE seule commande
  //delay(1000);
}

void decodeMessage2(const CanFrame& frame) {
  controllerData.throttle_raw = frame.data[0];
  controllerData.throttle_voltage = controllerData.throttle_raw * 5.0 / 255.0;
  controllerData.controller_temp = frame.data[1] - 40;
  controllerData.motor_temp = frame.data[2] - 30;

  uint8_t controller_status = frame.data[4];
  uint8_t switch_signals = frame.data[5];

  const char* commands[] = { "Neutral", "Forward", "Backward", "Reserved" };
  const char* feedbacks[] = { "Stationary", "Forward", "Backward", "Reserved" };

  controllerData.command = commands[controller_status & 0x03];
  controllerData.feedback = feedbacks[(controller_status >> 2) & 0x03];

  controllerData.switch_boost = (switch_signals >> 7) & 0x01;
  controllerData.switch_foot = (switch_signals >> 6) & 0x01;
  controllerData.switch_fwd = (switch_signals >> 5) & 0x01;
  controllerData.switch_bwd = (switch_signals >> 4) & 0x01;
  controllerData.switch_brake = (switch_signals >> 3) & 0x01;

  controllerData.hall_a = (switch_signals >> 0) & 0x01;
  controllerData.hall_b = (switch_signals >> 1) & 0x01;
  controllerData.hall_c = (switch_signals >> 2) & 0x01;

  Serial.print("--- Message 2 (0x0CF11F05) ---");
  /*
    Serial.printf("Throttle: %d (%.2f V)\n", controllerData.throttle_raw, controllerData.throttle_voltage);
    Serial.printf("Controller Temp: %d°C\n", controllerData.controller_temp);
    Serial.printf("Motor Temp: %d°C\n", controllerData.motor_temp);
    Serial.printf("Command: %s, Feedback: %s\n", controllerData.command, controllerData.feedback);
    Serial.printf("Switches: Boost=%d, Foot=%d, Fwd=%d, Bwd=%d, Brake=%d\n",
                  controllerData.switch_boost, controllerData.switch_foot,
                  controllerData.switch_fwd, controllerData.switch_bwd,
                  controllerData.switch_brake);
    Serial.printf("Hall Sensors: A=%d, B=%d, C=%d\n",
                  controllerData.hall_a, controllerData.hall_b, controllerData.hall_c);*/

  StaticJsonDocument<256> doc;
  doc["CM"]["TempMoteur"] = controllerData.motor_temp;
  doc["CM"]["TempCM"] = controllerData.controller_temp; 
  doc["CM"]["ThrottleV"] = controllerData.throttle_voltage;
  doc["CM"]["Commande"] = controllerData.command;
  if (controllerData.switch_fwd > 0) {
    doc["CM"]["FNB"] = "F";
  }
  else if (controllerData.switch_bwd > 0) {
    doc["CM"]["FNB"] = "B";

  }
  else {
    doc["CM"]["FNB"] = "N";

  }
  doc["CM"]["Feedback"] = controllerData.feedback;
  String buffer;
  serializeJson(doc, buffer);

  //Serial.println(buffer);   //  UNE seule commande
  Serial1.println(buffer);  //  UNE seule commande
  //delay(1000);
}

void decodeCanFrameMotor(const CanFrame& frame) {
 switch (frame.identifier) {
      case 0x0CF11E05:
        decodeMessage1(frame);
        break;
      case 0x0CF11F05:
        decodeMessage2(frame);
        break;

      // Autres (tu peux activer ça pour debug)
      default:
        /*
          Serial.println("Unknown frame, printing raw data:");
          for (int i = 0; i < rxFrame.data_length_code; i++) {
            Serial.printf("Data[%d]: 0x%02X\n", i, rxFrame.data[i]);
          }
          */
        break;
    }
}
/*
void setup() {
  Serial.begin(115200);



  Wire.begin(SLAVE_ADDRESS);
  Wire.onReceive(receiveEvent);
  Serial.println("Esclave I2C prêt à recevoir les données...");

  //Initialisation CAN
  ESP32Can.setPins(CAN_TX, CAN_RX);
  ESP32Can.setRxQueueSize(10);
  ESP32Can.setTxQueueSize(10);
  ESP32Can.setSpeed(ESP32Can.convertSpeed(250));

  if (ESP32Can.begin()) {
    Serial.println("CAN bus started!");
  } else {
    Serial.println("CAN bus failed!");
  }
}
unsigned long lastRequestTime = 0;

void loop() {
  uint16_t* temperatures = multiplexeur1.getTemperatures();

  // Lecture des trames entrantes à chaque itération
  while (ESP32Can.readFrame(rxFrame, 0)) {  // Pas de délai d’attente
    Serial.printf("\n[CAN] Received frame: 0x%08X, DLC=%d\n", rxFrame.identifier, rxFrame.data_length_code);

   
  }
  // Pas de delay(1000), laisse le CPU libre pour lire les trames
}


*/