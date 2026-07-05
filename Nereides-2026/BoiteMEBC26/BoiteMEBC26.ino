#include "solise.hpp"
#include "controlleurM.hpp"
#include "GPSClass.hpp"
// UART0 = Serial pour le moniteur série
// UART1 = Serial1 pour la communication avec le Pi
int SOC;
int temperature;
int RPM;
int Current;



void setup() {                    // Moniteur série pour debug

  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 16, 17);  // TX=16, RX=17 pour UART1 vers Pi*
  //delay(1000);

  ESP32Can.setPins(CAN_TX, CAN_RX);
  if (!ESP32Can.begin(ESP32Can.convertSpeed(CAN_BAUDRATE))) {
    Serial.println("[ERREUR] Initialisation CAN échouée !");
    while (1) delay(1000);
  }

  DBG_PRINTF("[OK] CAN initialisé à %d kbps\n", CAN_BAUDRATE);
  DBG_PRINTLN("[OK] Démarrage décodage trames KVMS BMS\n");

  sendQueryFrame();
}

void loop() {

  static GPSClass gpsModule(2, 9600, 32, 33, true);
  static unsigned long lastQuery = 0;

  /*if (millis() - lastQuery >= 500) {
    lastQuery = millis();
    sendQueryFrame();
  }*/

  CanFrame frame;
  if (ESP32Can.readFrame(frame, 0)) {
    decodeFrame(frame);
    decodeCanFrameMotor(frame);
  }

  gpsModule.update();
  delay(5);
}
