#include <BLEDevice.h>
#include <BLEUtils.h>
#include <Wire.h>
#include <ESP32-TWAI-CAN.hpp>

// ==========================================
// CONFIGURATION CAN BUS
// ==========================================
#define CAN_TX 22
#define CAN_RX 21

// IDs CAN arbitraires pour le BMS (À adapter selon ton besoin)
#define CAN_ID_BMS_GLOBAL   0x211
#define CAN_ID_BMS_CELLS    0x212

CanFrame txFrame;

// ==========================================
// CONFIGURATION BLE & BMS JBD
// ==========================================
// 1. METTEZ ICI L'ADRESSE MAC DE VOTRE BMS
#define BMS_MAC_ADDRESS "AA:C2:37:0A:B4:5B" 

// UUIDs standards des modules Bluetooth JBD BMS
static BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb");
static BLEUUID charTxUUID("0000ff02-0000-1000-8000-00805f9b34fb"); // Écriture
static BLEUUID charRxUUID("0000ff01-0000-1000-8000-00805f9b34fb"); // Lecture

static boolean connecte = false;
static BLERemoteCharacteristic* pRemoteCharacteristicTx;
static BLERemoteCharacteristic* pRemoteCharacteristicRx;
static BLEClient* pClient = nullptr;

// Buffer global pour accumuler les fragments de trame
uint8_t bmsBuffer[128];
size_t bufferIndex = 0;

// Trames officielles de requête JBD
const uint8_t commandeGlobales[]  = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77}; // Lecture Infos Globales
const uint8_t commandeCellules[]  = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77}; // Lecture Tensions Cellules
uint8_t etapeLecture = 0; // Permet d'alterner entre 0x03 et 0x04

// ==========================================
// FONCTIONS D'ENVOI CAN
// ==========================================

// Envoi des données globales (Tension, Courant, SoC, Protections)
void sendGlobalDataViaCAN(uint16_t tensionCentivolts, int16_t courantCentiamperes, uint8_t soc, uint16_t statutProtection) {
  txFrame.identifier = CAN_ID_BMS_GLOBAL;
  txFrame.extd = 0;                  // Trame standard (11 bits)
  txFrame.data_length_code = 7;      // 7 octets de données
  
  // Tension (Bytes 0-1) - Little Endian
  txFrame.data[0] = tensionCentivolts & 0xFF;
  txFrame.data[1] = (tensionCentivolts >> 8) & 0xFF;
  
  // Courant (Bytes 2-3) - Little Endian
  txFrame.data[2] = courantCentiamperes & 0xFF;
  txFrame.data[3] = (courantCentiamperes >> 8) & 0xFF;
  
  // SoC (Byte 4)
  txFrame.data[4] = soc;
  
  // Statut Protection (Bytes 5-6) - Little Endian
  txFrame.data[5] = statutProtection & 0xFF;
  txFrame.data[6] = (statutProtection >> 8) & 0xFF;
  
  if (ESP32Can.writeFrame(txFrame)) {
    Serial.println("CAN -> Infos globales envoyées avec succès.");
  } else {
    Serial.println("CAN -> Échec de l'envoi des infos globales.");
  }
}

// Envoi de la tension d'une cellule spécifique
void sendCellVoltageViaCAN(uint8_t cellIndex, uint16_t voltageMv) {
  txFrame.identifier = CAN_ID_BMS_CELLS;
  txFrame.extd = 0;
  txFrame.data_length_code = 3;      // 3 octets (1 pour l'index, 2 pour la tension)
  
  txFrame.data[0] = cellIndex;        // Numéro de la cellule (1, 2, 3...)
  txFrame.data[1] = voltageMv & 0xFF; // Tension LSB
  txFrame.data[2] = (voltageMv >> 8) & 0xFF; // Tension MSB
  
  if (ESP32Can.writeFrame(txFrame)) {
    Serial.printf("CAN -> Cellule %02d : %d mV envoyés.\n", cellIndex, voltageMv);
  } else {
    Serial.println("CAN -> Échec de l'envoi de la cellule.");
  }
}

// ==========================================
// DÉCODAGE JBD BMS
// ==========================================

void decoderErreursJBD(uint16_t protStatus) {
  if (protStatus == 0) {
    Serial.println("Batterie 1 / Statut Protection : RAS (Aucune erreur, tout est normal)");
    return;
  }
  
  Serial.print("⚠️ ALERTE / PROTECTION(S) ACTIVÉE(S) : ");
  if (protStatus & (1 << 0))  Serial.print("[Batterie 1 / Surtension bloc cellule] ");
  if (protStatus & (1 << 1))  Serial.print("[Batterie 1 / Sous-tension bloc cellule] ");
  if (protStatus & (1 << 2))  Serial.print("[Batterie 1 / Surtension pack global] ");
  if (protStatus & (1 << 3))  Serial.print("[Batterie 1 / Sous-tension pack global] ");
  if (protStatus & (1 << 4))  Serial.print("[Batterie 1 / Surchauffe en charge] ");
  if (protStatus & (1 << 5))  Serial.print("[Batterie 1 / Sous-température en charge] ");
  if (protStatus & (1 << 6))  Serial.print("[Batterie 1 / Surchauffe en décharge] ");
  if (protStatus & (1 << 7))  Serial.print("[Batterie 1 / Sous-température en décharge] ");
  if (protStatus & (1 << 8))  Serial.print("[Batterie 1 / Surintensité / Surcharge] ");
  if (protStatus & (1 << 9))  Serial.print("[Batterie 1 / Surintensité en décharge] ");
  if (protStatus & (1 << 10)) Serial.print("[Batterie 1 / Court-Circuit !] ");
  if (protStatus & (1 << 11)) Serial.print("[Batterie 1 / Erreur IC de gestion] ");
  if (protStatus & (1 << 12)) Serial.print("[Batterie 1 / Erreur MOSFET] ");
  Serial.println();
}

void decoderTrameJBD(uint8_t* buffer, size_t taille) {
  uint8_t typeTrame = buffer[1]; // 0x03 ou 0x04

  if (typeTrame == 0x03) {
    Serial.println("\n====================================");
    Serial.println("===      INFORMATIONS GLOBALES   ===");
    Serial.println("====================================");
    
    uint16_t tensionBrute = (buffer[4] << 8) | buffer[5]; // en centivolts (V * 100)
    float tensionVolts = tensionBrute / 100.0;
    Serial.print("Batterie 1 / Tension Totale     : "); Serial.print(tensionVolts); Serial.println(" V");

    int16_t courantBrut = (buffer[6] << 8) | buffer[7]; // en centiampères (A * 100)
    float courantAmperes = courantBrut / 100.0;
    Serial.print("Batterie 1 / Courant / Intensité : "); Serial.print(courantAmperes); Serial.println(" A");

    float capRestanteAh = ((buffer[8] << 8) | buffer[9]) / 100.0;
    Serial.print("Batterie 1 / Capacité Restante   : "); Serial.print(capRestanteAh); Serial.println(" Ah");

    uint8_t soc = buffer[23];
    Serial.print("Batterie 1 / Pourcentage (SoC)   : "); Serial.print(soc); Serial.println(" %");
    Serial.print("Batterie 1 / Nombre de Cellules  : "); Serial.println(buffer[25]);

    uint16_t statutProtection = (buffer[20] << 8) | buffer[21];
    decoderErreursJBD(statutProtection);
    Serial.println("------------------------------------");

    // ENVOI SUR LE BUS CAN
    sendGlobalDataViaCAN(tensionBrute, courantBrut, soc, statutProtection);

  } else if (typeTrame == 0x04) {
    Serial.println("\n====================================");
    Serial.println("===     TENSIONS DES CELLULES    ===");
    Serial.println("====================================");
    
    uint8_t longueurDonnees = buffer[3];
    int nbCellulesTrame = longueurDonnees / 2; 

    for (int i = 0; i < nbCellulesTrame; i++) {
      int indexCellule = 4 + (i * 2);
      uint16_t tensionCelluleMv = (buffer[indexCellule] << 8) | buffer[indexCellule + 1];
      float tensionCelluleVolts = tensionCelluleMv / 1000.0;
      
      Serial.print("Batterie 1 / Cellule "); 
      if (i + 1 < 10) Serial.print("0"); 
      Serial.print(i + 1); 
      Serial.print(" : "); 
      Serial.print(tensionCelluleVolts, 3); 
      Serial.println(" V");

      // ENVOI SUR LE BUS CAN (Cellule par cellule)
      sendCellVoltageViaCAN(i + 1, tensionCelluleMv);
      delay(5); // Petit delay pour ne pas saturer le tampon TX du CAN si beaucoup de cellules
    }
    Serial.println("------------------------------------");
  }
}

// Callback réception Bluetooth
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (length > 0 && pData[0] == 0xDD) {
    bufferIndex = 0;
  }

  for (size_t i = 0; i < length; i++) {
    if (bufferIndex < sizeof(bmsBuffer)) {
      bmsBuffer[bufferIndex++] = pData[i];
    }
  }

  if (bufferIndex > 4 && bmsBuffer[0] == 0xDD && bmsBuffer[bufferIndex - 1] == 0x77) {
    decoderTrameJBD(bmsBuffer, bufferIndex);
    bufferIndex = 0; 
  }
}

bool connecterAuBMS(String targetAddress) {
  Serial.print("Batterie 1 / Connexion à l'adresse MAC : ");
  Serial.println(targetAddress);

  BLEAddress bmsAddress(targetAddress.c_str());
  pClient = BLEDevice::createClient();

  if (!pClient->connect(bmsAddress)) {
    Serial.println("Batterie 1 /  -> Échec.");
    return false;
  }
  Serial.println("Batterie 1 /  -> Connecté !");

  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) { pClient->disconnect(); return false; }

  pRemoteCharacteristicTx = pRemoteService->getCharacteristic(charTxUUID);
  pRemoteCharacteristicRx = pRemoteService->getCharacteristic(charRxUUID);

  if (pRemoteCharacteristicTx == nullptr || pRemoteCharacteristicRx == nullptr) {
    pClient->disconnect(); return false;
  }

  if (pRemoteCharacteristicRx->canNotify()) {
    pRemoteCharacteristicRx->registerForNotify(notifyCallback);
  }
  return true;
}

// ==========================================
// SETUP & LOOP
// ==========================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Batterie 1 / Démarrage du système JBD Monitor & CAN Bus...");

  // Initialisation CAN Bus (250 kbps)
  ESP32Can.setPins(CAN_TX, CAN_RX);
  ESP32Can.setRxQueueSize(5);
  ESP32Can.setTxQueueSize(5);
  ESP32Can.setSpeed(ESP32Can.convertSpeed(250));

  if (ESP32Can.begin()) {
    Serial.println("CAN bus started successfully!");
  } else {
    Serial.println("CAN bus failed to start!");
  }

  // Initialisation BLE
  BLEDevice::init("");
  connecte = connecterAuBMS(BMS_MAC_ADDRESS);
}

void loop() {
  if (connecte && pClient->isConnected()) {
    // Alternance des requêtes toutes les 3 secondes
    if (etapeLecture == 0) {
      pRemoteCharacteristicTx->writeValue((uint8_t*)commandeGlobales, sizeof(commandeGlobales), false);
      etapeLecture = 1; 
    } else {
      pRemoteCharacteristicTx->writeValue((uint8_t*)commandeCellules, sizeof(commandeCellules), false);
      etapeLecture = 0;
    }
  } else {
    Serial.println("Batterie 1 / BMS déconnecté. Reconnexion dans 10s...");
    delay(10000);
    connecte = connecterAuBMS(BMS_MAC_ADDRESS);
  }

  delay(3000); // Pause de 3 secondes entre chaque cycle
}
