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

  if (millis() - lastQuery >= QUERY_INTERVAL_MS) {
    lastQuery = millis();
    sendQueryFrame();
  }

  CanFrame frame;
  if (ESP32Can.readFrame(frame, 0)) {
    decodeFrame(frame);
    decodeCanFrameMotor(frame);
  }

  gpsModule.update();
  delay(5);
  // Communication avec le Pi
  //SOC = random(0, 100);
  //temperature = random(100, 150);
  //RPM = random(6000, 10000);
  //Current = random(100, 300);
  /*
  StaticJsonDocument<128> doc;
  doc["batterie"]["TempMax"] = g_maxCellT_C;
  doc["batterie"]["TempMin"] = g_minCellT_C;
  doc["batterie"]["SOC"] = g_soc_pct;
  doc["batterie"]["Tension"] = g_totalVoltage_V;
  doc["batterie"]["Current"] = g_current_A;

  doc["CM"]["Current"] = controllerData.motor_current;
  doc["CM"]["RPM"] = controllerData.speed_rpm;
  doc["CM"]["Tension"] = controllerData.battery_voltage;
  doc["CM"]["Current"] = controllerData.motor_current;
  doc["CM"]["TempMoteur"] = controllerData.motor_temp;
  doc["CM"]["TempCM"] = controllerData.controller_temp;  
  String buffer;
  serializeJson(doc, buffer);
  //Serial.println(buffer);   //  UNE seule commande
  Serial1.println(buffer);  //  UNE seule commande
  */
  //delay(1000);
}