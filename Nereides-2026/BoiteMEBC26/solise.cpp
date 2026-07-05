#include "solise.hpp"

// ============================================================
//  BMS JBD (batteries actuellement utilisées) — Décodeur CAN
//  Basé sur : tableau_can_batterie.xlsx (feuilles "bms 1" / "bms2")
//  Hardware  : ESP32 + ESP32-TWAI-CAN
// ============================================================

BatteryData g_battery1;
BatteryData g_battery2;

// ================================================================
//  Utilitaires
// ================================================================

// Les trames de ce BMS sont encodées little-endian (LSB en premier octet).
static inline uint16_t u16LE(uint8_t lsb, uint8_t msb) {
  return ((uint16_t)msb << 8) | lsb;
}

// ----------------------------------------------------------------
//  Envoi JSON vers le Raspberry Pi (UART1)
//  Les noms de clés ("SOC", "Tension", "Current") sont conservés
//  à l'identique de l'ancien firmware KVMS pour ne pas casser le
//  parsing côté Pi. La clé racine est "Batterie1" / "Batterie2"
//  (majuscule) pour distinguer les deux packs et correspondre au
//  mapping de la chaîne de réception (ecran.py FIELD_MAP + backend
//  flatten_nested -> battery1_* / battery2_*).
// ----------------------------------------------------------------
static void sendBatteryJson(const char* rootKey, const BatteryData& bat) {
  StaticJsonDocument<256> doc;
  doc[rootKey]["SOC"] = bat.soc_pct;
  doc[rootKey]["Tension"] = bat.totalVoltage_V;
  doc[rootKey]["Current"] = bat.current_A;
  doc[rootKey]["Protection"] = bat.protectionStatus;

  String buffer;
  serializeJson(doc, buffer);
  //Serial.println(buffer);   //  UNE seule commande
  Serial1.println(buffer);  //  UNE seule commande
}

// ----------------------------------------------------------------
//  Envoi JSON de la température max (trame séparée 0x213 / 0x313)
// ----------------------------------------------------------------
static void sendBatteryTempJson(const char* rootKey, const BatteryData& bat) {
  StaticJsonDocument<128> doc;
  doc[rootKey]["TempMax"] = bat.tempMax_C;

  String buffer;
  serializeJson(doc, buffer);
  //Serial.println(buffer);   //  UNE seule commande
  Serial1.println(buffer);  //  UNE seule commande
}

// ----------------------------------------------------------------
//  Affichage debug du registre de protection
// ----------------------------------------------------------------
static void printProtectionFlags(uint16_t status) {
  if (!status) return;
  DBG_PRINT("  Alertes : ");
  if (status & (1 << PROT_BIT_CELL_OV))       DBG_PRINT("Surtension_cellule ");
  if (status & (1 << PROT_BIT_CELL_UV))       DBG_PRINT("Sous-tension_cellule ");
  if (status & (1 << PROT_BIT_PACK_OV))       DBG_PRINT("Surtension_pack ");
  if (status & (1 << PROT_BIT_PACK_UV))       DBG_PRINT("Sous-tension_pack ");
  if (status & (1 << PROT_BIT_CHG_OT))        DBG_PRINT("Surchauffe_charge ");
  if (status & (1 << PROT_BIT_CHG_UT))        DBG_PRINT("Sous-temp_charge ");
  if (status & (1 << PROT_BIT_DISCHG_OT))     DBG_PRINT("Surchauffe_decharge ");
  if (status & (1 << PROT_BIT_DISCHG_UT))     DBG_PRINT("Sous-temp_decharge ");
  if (status & (1 << PROT_BIT_CHG_OC))        DBG_PRINT("Surintensite_charge ");
  if (status & (1 << PROT_BIT_DISCHG_OC))     DBG_PRINT("Surintensite_decharge ");
  if (status & (1 << PROT_BIT_SHORT_CIRCUIT)) DBG_PRINT("Court-circuit ");
  if (status & (1 << PROT_BIT_IC_ERROR))      DBG_PRINT("Erreur_IC ");
  if (status & (1 << PROT_BIT_MOSFET_ERROR))  DBG_PRINT("Erreur_MOSFET ");
  DBG_PRINTLN();
}

// ----------------------------------------------------------------
//  Trame globale (0x211 / 0x311) :
//  B0-B1 Tension totale uint16 LE, 0.01 V
//  B2-B3 Courant int16 LE, 0.01 A (positif = charge, négatif = décharge)
//  B4    SOC uint8, 1 %
//  B5-B6 Statut protection uint16 LE, registre de bits
// ----------------------------------------------------------------
static void decodeGlobalFrame(BatteryData& bat, const char* jsonKey, uint32_t id, uint8_t* d, uint8_t dlc) {
  if (dlc < 7) {
    DBG_PRINTF("[0x%03X] Trame globale : DLC insuffisant (%d)\n", id, dlc);
    return;
  }

  uint16_t rawV = u16LE(d[0], d[1]);
  int16_t  rawI = (int16_t)u16LE(d[2], d[3]);

  bat.totalVoltage_V   = rawV * 0.01f;
  bat.current_A         = rawI * 0.01f;
  bat.soc_pct           = d[4];
  bat.protectionStatus  = u16LE(d[5], d[6]);
  bat.receivedGlobal    = true;

  DBG_PRINTF("[0x%03X] Vtotal=%.2f V  I=%.2f A (%s)  SOC=%d%%  Protection=0x%04X\n",
    id, bat.totalVoltage_V, bat.current_A,
    bat.current_A >= 0 ? "charge" : "decharge",
    bat.soc_pct, bat.protectionStatus);

  printProtectionFlags(bat.protectionStatus);

  sendBatteryJson(jsonKey, bat);
}

// ----------------------------------------------------------------
//  Trame cellule (0x212 / 0x312) :
//  B0    Index cellule uint8 (1 = cellule 1, 2 = cellule 2, ...)
//  B1-B2 Tension cellule uint16 LE, 1 mV
// ----------------------------------------------------------------
static void decodeCellFrame(BatteryData& bat, uint32_t id, uint8_t* d, uint8_t dlc) {
  if (dlc < 3) {
    DBG_PRINTF("[0x%03X] Trame cellule : DLC insuffisant (%d)\n", id, dlc);
    return;
  }

  uint8_t  cellIndex = d[0];               // 1-based
  uint16_t cellMv     = u16LE(d[1], d[2]);

  if (cellIndex >= 1 && cellIndex <= MAX_CELLS) {
    bat.cellVoltage_mV[cellIndex - 1] = cellMv;
    if (cellIndex > bat.cellCount) bat.cellCount = cellIndex;
  }

  DBG_PRINTF("[0x%03X] Cellule %d = %d mV\n", id, cellIndex, cellMv);
}

// ----------------------------------------------------------------
//  Trame température max (0x213 / 0x313) :
//  B0-B1 Température max int16 LE, 0.1 °C (envoyée par l'ESP32 côté BMS,
//        déjà calculée comme le max des sondes NTC reçues en Bluetooth)
// ----------------------------------------------------------------
static void decodeTempFrame(BatteryData& bat, const char* jsonKey, uint32_t id, uint8_t* d, uint8_t dlc) {
  if (dlc < 2) {
    DBG_PRINTF("[0x%03X] Trame température : DLC insuffisant (%d)\n", id, dlc);
    return;
  }

  int16_t rawT = (int16_t)u16LE(d[0], d[1]);

  bat.tempMax_C    = rawT * 0.1f;
  bat.receivedTemp = true;

  DBG_PRINTF("[0x%03X] Temperature MAX = %.1f °C\n", id, bat.tempMax_C);

  sendBatteryTempJson(jsonKey, bat);
}

// ----------------------------------------------------------------
//  Trame inconnue — dump brut
// ----------------------------------------------------------------
static void dumpRaw(CanFrame& f) {
  DBG_PRINTF("[RAW] ID=0x%03X DLC=%d : ", f.identifier, f.data_length_code);
  for (int i = 0; i < f.data_length_code; i++) DBG_PRINTF("%02X ", f.data[i]);
  DBG_PRINTLN();
}

// ================================================================
//  Dispatch principal
// ================================================================
void decodeFrame(CanFrame& f) {
  // Les trames du contrôleur moteur sont décodées par controlleurM.cpp
  if ((f.identifier == 0x0CF11E05) || (f.identifier == 0x0CF11F05)) return;

  //DBG_PRINTF("réception de la trame %X \n", f.identifier);
  uint8_t* d   = f.data;
  uint8_t  dlc = f.data_length_code;

  switch (f.identifier) {
    case ID_BAT1_GLOBAL:
      decodeGlobalFrame(g_battery1, "Batterie1", f.identifier, d, dlc);
      break;
    case ID_BAT1_CELL:
      decodeCellFrame(g_battery1, f.identifier, d, dlc);
      break;
    case ID_BAT1_TEMP_MAX:
      decodeTempFrame(g_battery1, "Batterie1", f.identifier, d, dlc);
      break;
    case ID_BAT2_GLOBAL:
      decodeGlobalFrame(g_battery2, "Batterie2", f.identifier, d, dlc);
      break;
    case ID_BAT2_CELL:
      decodeCellFrame(g_battery2, f.identifier, d, dlc);
      break;
    case ID_BAT2_TEMP_MAX:
      decodeTempFrame(g_battery2, "Batterie2", f.identifier, d, dlc);
      break;
    default:
      //dumpRaw(f);
      break;
  }
}

// ----------------------------------------------------------------
//  Trame de requête périodique — non requise pour ce BMS
// ----------------------------------------------------------------
void sendQueryFrame() {
  // Le BMS JBD utilisé sur les deux batteries diffuse ses trames (0x211/0x212
  // et 0x311/0x312) automatiquement et en continu sur le bus CAN, contrairement
  // à l'ancien BMS KVMS qui nécessitait l'envoi périodique d'une trame de requête
  // (0x0400FF80) pour obtenir une réponse.
  // Cette fonction est conservée vide uniquement pour ne pas casser sendRasb.ino,
  // qui l'appelle toujours dans setup() et dans loop().
}