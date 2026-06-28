#include <BLEDevice.h>
#include <BLEUtils.h>

// 1. REMPLACEZ ICI PAR L'ADRESSE MAC DE VOTRE BMS JBD
#define BMS_MAC_ADDRESS "AA:C2:37:0A:B4:5B" 

// UUIDs standards des modules Bluetooth JBD BMS
static BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb");
static BLEUUID charTxUUID("0000ff02-0000-1000-8000-00805f9b34fb"); // Écriture
static BLEUUID charRxUUID("0000ff01-0000-1000-8000-00805f9b34fb"); // Lecture

static boolean connecte = false;
static BLERemoteCharacteristic* pRemoteCharacteristicTx;
static BLERemoteCharacteristic* pRemoteCharacteristicRx;
static BLEClient* pClient = nullptr;

// Trame officielle de requête JBD pour obtenir les infos de base
const uint8_t commandeLecture[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};

// Fonction appelée dès que le BMS renvoie les données
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  Serial.print("Données reçues du BMS (HEX) : ");
  for (size_t i = 0; i < length; i++) {
    if (pData[i] < 0x10) Serial.print("0");
    Serial.print(pData[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

bool connecterAuBMS(String targetAddress) {
  Serial.print("Tentative de connexion directe à : ");
  Serial.println(targetAddress);

  BLEAddress bmsAddress(targetAddress.c_str());
  pClient = BLEDevice::createClient();

  // Connexion directe en utilisant l'adresse MAC
  if (!pClient->connect(bmsAddress)) {
    Serial.println(" -> Échec de la connexion.");
    return false;
  }
  Serial.println(" -> Connecté au BMS !");

  // Récupération du service JBD via son UUID
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    pClient->disconnect();
    return false;
  }

  // Récupération des caractéristiques Tx et Rx
  pRemoteCharacteristicTx = pRemoteService->getCharacteristic(charTxUUID);
  pRemoteCharacteristicRx = pRemoteService->getCharacteristic(charRxUUID);

  if (pRemoteCharacteristicTx == nullptr || pRemoteCharacteristicRx == nullptr) {
    pClient->disconnect();
    return false;
  }

  // Activation des notifications pour écouter les réponses
  if (pRemoteCharacteristicRx->canNotify()) {
    pRemoteCharacteristicRx->registerForNotify(notifyCallback);
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Initialisation du Bluetooth de l'ESP32...");
  
  BLEDevice::init("");
  
  // Tentative de connexion dès le démarrage
  connecte = connecterAuBMS(BMS_MAC_ADDRESS);
}

void loop() {
  // Si la connexion a réussi, on demande les données toutes les 5 secondes
  if (connecte) {
    if (pClient->isConnected()) {
      Serial.println("Demande des données globales au BMS...");
      pRemoteCharacteristicTx->writeValue((uint8_t*)commandeLecture, sizeof(commandeLecture), false);
    } else {
      Serial.println("BMS déconnecté. Tentative de reconnexion...");
      connecte = connecterAuBMS(BMS_MAC_ADDRESS);
    }
  } else {
    // Si la connexion initiale a échoué, on réessaie toutes les 10 secondes
    delay(10000);
    connecte = connecterAuBMS(BMS_MAC_ADDRESS);
  }

  delay(5000); 
}