#include <BLEDevice.h>
#include <BLEUtils.h>

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

// Fonction pour décoder les messages d'erreur / protections (Index 20 et 21 de la trame 0x03)
void decoderErreursJBD(uint16_t protStatus) {
  if (protStatus == 0) {
    Serial.println("Statut Protection : RAS (Aucune erreur, tout est normal)");
    return;
  }
  
  Serial.print("⚠️ ALERTE / PROTECTION(S) ACTIVÉE(S) : ");
  if (protStatus & (1 << 0))  Serial.print("[Surtension bloc cellule] ");
  if (protStatus & (1 << 1))  Serial.print("[Sous-tension bloc cellule] ");
  if (protStatus & (1 << 2))  Serial.print("[Surtension pack global] ");
  if (protStatus & (1 << 3))  Serial.print("[Sous-tension pack global] ");
  if (protStatus & (1 << 4))  Serial.print("[Surchauffe en charge] ");
  if (protStatus & (1 << 5))  Serial.print("[Sous-température en charge] ");
  if (protStatus & (1 << 6))  Serial.print("[Surchauffe en décharge] ");
  if (protStatus & (1 << 7))  Serial.print("[Sous-température en décharge] ");
  if (protStatus & (1 << 8))  Serial.print("[Surintensité / Surcharge] ");
  if (protStatus & (1 << 9))  Serial.print("[Surintensité en décharge] ");
  if (protStatus & (1 << 10)) Serial.print("[Court-Circuit !] ");
  if (protStatus & (1 << 11)) Serial.print("[Erreur IC de gestion] ");
  if (protStatus & (1 << 12)) Serial.print("[Erreur MOSFET] ");
  Serial.println();
}

// Fonction de décodage globale
void decoderTrameJBD(uint8_t* buffer, size_t taille) {
  uint8_t typeTrame = buffer[1]; // 0x03 ou 0x04

  if (typeTrame == 0x03) {
    Serial.println("\n====================================");
    Serial.println("===      INFORMATIONS GLOBALES   ===");
    Serial.println("====================================");
    
    float tensionVolts = ((buffer[4] << 8) | buffer[5]) / 100.0;
    Serial.print("Tension Totale     : "); Serial.print(tensionVolts); Serial.println(" V");

    int16_t courantBrut = (buffer[6] << 8) | buffer[7];
    float courantAmperes = courantBrut / 100.0;
    Serial.print("Courant / Intensité : "); Serial.print(courantAmperes); Serial.println(" A");

    float capRestanteAh = ((buffer[8] << 8) | buffer[9]) / 100.0;
    Serial.print("Capacité Restante   : "); Serial.print(capRestanteAh); Serial.println(" Ah");

    Serial.print("Pourcentage (SoC)   : "); Serial.print(buffer[23]); Serial.println(" %");
    Serial.print("Nombre de Cellules  : "); Serial.println(buffer[25]);

    // Décodage des erreurs (Octets d'index 20 et 21 combinés)
    uint16_t statutProtection = (buffer[20] << 8) | buffer[21];
    decoderErreursJBD(statutProtection);
    Serial.println("------------------------------------");

  } else if (typeTrame == 0x04) {
    Serial.println("\n====================================");
    Serial.println("===     TENSIONS DES CELLULES    ===");
    Serial.println("====================================");
    
    uint8_t longueurDonnees = buffer[3];
    int nbCellulesTrame = longueurDonnees / 2; // Chaque cellule prend 2 octets (mV)

    for (int i = 0; i < nbCellulesTrame; i++) {
      int indexCellule = 4 + (i * 2);
      uint16_t tensionCelluleMv = (buffer[indexCellule] << 8) | buffer[indexCellule + 1];
      float tensionCelluleVolts = tensionCelluleMv / 1000.0;
      
      Serial.print("Cellule "); 
      if (i + 1 < 10) Serial.print("0"); // Alignement de l'affichage
      Serial.print(i + 1); 
      Serial.print(" : "); 
      Serial.print(tensionCelluleVolts, 3); // 3 décimales pour être précis au mV
      Serial.println(" V");
    }
    Serial.println("------------------------------------");
  }
}

// Callback appelé à la réception de chaque morceau (fragment) Bluetooth
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  // Si on reçoit un début de trame (0xDD), on réinitialise le buffer d'accumulation
  if (length > 0 && pData[0] == 0xDD) {
    bufferIndex = 0;
  }

  // Stockage des octets reçus
  for (size_t i = 0; i < length; i++) {
    if (bufferIndex < sizeof(bmsBuffer)) {
      bmsBuffer[bufferIndex++] = pData[i];
    }
  }

  // Vérification de fin de trame complète (0x77)
  if (bufferIndex > 4 && bmsBuffer[0] == 0xDD && bmsBuffer[bufferIndex - 1] == 0x77) {
    decoderTrameJBD(bmsBuffer, bufferIndex);
    bufferIndex = 0; // Reset
  }
}

bool connecterAuBMS(String targetAddress) {
  Serial.print("Connexion à l'adresse MAC : ");
  Serial.println(targetAddress);

  BLEAddress bmsAddress(targetAddress.c_str());
  pClient = BLEDevice::createClient();

  if (!pClient->connect(bmsAddress)) {
    Serial.println(" -> Échec.");
    return false;
  }
  Serial.println(" -> Connecté !");

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

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Démarrage du système JBD Monitor...");
  BLEDevice::init("");
  connecte = connecterAuBMS(BMS_MAC_ADDRESS);
}

void loop() {
  if (connecte && pClient->isConnected()) {
    
    // Alternance des requêtes toutes les 3 secondes pour ne pas surcharger le BMS
    if (etapeLecture == 0) {
      // Étape 0 : On demande les infos globales + les erreurs
      pRemoteCharacteristicTx->writeValue((uint8_t*)commandeGlobales, sizeof(commandeGlobales), false);
      etapeLecture = 1; 
    } else {
      // Étape 1 : On demande la tension des cellules individuelles
      pRemoteCharacteristicTx->writeValue((uint8_t*)commandeCellules, sizeof(commandeCellules), false);
      etapeLecture = 0;
    }
    
  } else {
    Serial.println("BMS déconnecté. Reconnexion dans 10s...");
    delay(10000);
    connecte = connecterAuBMS(BMS_MAC_ADDRESS);
  }

  delay(3000); // Pause de 3 secondes entre chaque échange
}