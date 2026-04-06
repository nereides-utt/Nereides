#include "solise.hpp"


// --- 0x040080** : Tensions cellules (mV) ---
uint16_t g_cellVoltage[MAX_CELLS];        // [i] = tension cellule i+1 en mV
uint8_t  g_cellVoltage_board;             // numéro de board source

// --- 0x040180** : Températures cellules (°C) ---
int8_t   g_cellTemp[MAX_NTC];             // [i] = température NTC i+1 en °C (offset -40 appliqué)
uint8_t  g_cellTemp_board;

// --- 0x040280** : Total Info 0 ---
float    g_totalVoltage_V;                // tension totale en V (résolution 0.1 V)
float    g_current_A;                     // courant en A (résolution 0.1 A, offset -30000 ; négatif = charge)
float    g_soc_pct;                       // SOC en % (résolution 0.1 %)
uint8_t  g_lifeCycles;                    // compteur de cycles 0..255
uint8_t  g_totalInfo0_board;

// --- 0x040380** : Total Info 1 ---
uint16_t g_power_W;                       // puissance en W
uint16_t g_totalEnergy_Wh;               // énergie totale en Wh
int8_t   g_mosFetTemp_C;                 // température MOS en °C (offset -40)
int8_t   g_boardTemp_C;                  // température ambiante carte en °C (offset -40)
int8_t   g_heatTemp_C;                   // température film chauffant en °C (offset -40)
uint8_t  g_heatCurrent_A;               // courant de chauffe en A
uint8_t  g_totalInfo1_board;

// --- 0x040480** : Stats tensions cellules ---
uint16_t g_maxCellV_mV;                  // tension max en mV
uint8_t  g_maxCellV_no;                  // numéro de la cellule à Vmax
uint16_t g_minCellV_mV;                  // tension min en mV
uint8_t  g_minCellV_no;                  // numéro de la cellule à Vmin
uint16_t g_diffCellV_mV;                 // différence Vmax - Vmin en mV
uint8_t  g_cellVStats_board;

// --- 0x040580** : Stats températures cellules ---
int8_t   g_maxCellT_C;                   // température max en °C (offset -40)
uint8_t  g_maxCellT_no;                  // numéro de la cellule à Tmax
int8_t   g_minCellT_C;                   // température min en °C (offset -40)
uint8_t  g_minCellT_no;                  // numéro de la cellule à Tmin
int8_t   g_diffCellT_C;                  // différence Tmax - Tmin
uint8_t  g_cellTStats_board;

// --- 0x040680** : Status 0 — état MOSFET ---
bool     g_chgMOS;                       // MOSFET charge actif
bool     g_disChgMOS;                    // MOSFET décharge actif
bool     g_preMOS;                       // MOSFET pré-charge actif
bool     g_heatMOS;                      // MOSFET chauffe actif
bool     g_fanMOS;                       // MOSFET ventilateur actif
uint8_t  g_status0_board;

// --- 0x040780** : Status 1 — état batterie / connexions / ports ---
uint8_t  g_batState;                     // 0=idle, 1=charge, 2=décharge
bool     g_chgDetect;                    // chargeur connecté
bool     g_loadDetect;                   // charge connectée
uint8_t  g_doState;                      // état sorties digitales DO1..DO8 (bitmask)
uint8_t  g_diState;                      // état entrées digitales DI (bitmask)
uint8_t  g_status1_board;

// --- 0x040880** : Status 2 ---
uint8_t  g_cellCount;                    // nombre de cellules en série
uint8_t  g_ntcCount;                     // nombre de sondes NTC
uint32_t g_remainCapacity_mAh;           // capacité restante en mAh
uint16_t g_cycleTimes;                   // nombre de cycles charge/décharge
uint8_t  g_status2_board;

// --- 0x040980** : Alarmes et défauts matériels (8 octets de bits) ---
uint8_t  g_faultByte[8];                 // [0..7] = octets bruts des alarmes/défauts
uint8_t  g_faultHW_board;

// --- 0x040B80** : Informations de charge ---
uint16_t g_remainChgTime_min;            // temps de charge restant en minutes
uint8_t  g_wakeupSource;                 // bitmask source de réveil (bit0=KEY, 1=BTN, 2=485, 3=CAN, 4=CURRENT)
uint8_t  g_chgInfo_board;

// --- 0x040D80** : Limitation de courant ---
bool     g_curLimitEnabled;             // limitation activée
float    g_curLimit_A;                  // valeur limite en A (offset -30000)
uint8_t  g_soh_pct;                     // état de santé en %
float    g_pwmDuty_pct;                 // rapport cyclique PWM en %
uint8_t  g_curLimit_board;

// --- 0x040E80** : Défauts détaillés (brut) ---
uint8_t  g_faultDetailPage;             // numéro de page
uint8_t  g_faultDetailData[7];          // octets D1..D7
uint8_t  g_faultDetail_board;

// ============================================================
//  KVMS BMS — Décodeur de trames CAN -> Variables globales
//  Basé sur : KVMS_Intranet_Communication_CAN_Protocol.xlsx
//  Hardware  : ESP32 + ESP32-TWAI-CAN
//  CAN H : fil blanc  |  CAN L : fil jaune
//  Pas de résistance de terminaison externe nécessaire
// ============================================================

// ----------------------------------------------------------------
//  Debug Serial : mettre à true pour activer les prints
// ----------------------------------------------------------------

// ================================================================
//  Utilitaires
// ================================================================
static inline uint16_t u16(uint8_t hi, uint8_t lo) {
  return ((uint16_t)hi << 8) | lo;
}
static inline uint32_t maskedId(uint32_t id) { return id & ID_MASK; }
static inline uint8_t  boardNum(uint32_t id)  { return (uint8_t)(id & 0xFF); }

// ----------------------------------------------------------------
//  Trame de requête périodique
// ----------------------------------------------------------------
void sendQueryFrame() {
  CanFrame query;
  query.identifier       = ID_QUERY;
  query.extd             = 1;
  query.data_length_code = 8;
  for (int i = 0; i < 8; i++) query.data[i] = 0x00;

  if (ESP32Can.writeFrame(query)) {
    DBG_PRINTLN("[TX] Trame requête 0x0400FF80 envoyée");
  } else {
    DBG_PRINTLN("[TX] ERREUR envoi requête");
  }
}

// ================================================================
//  Décodeurs — mettent à jour les variables globales ET affichent
// ================================================================

void decodeCellVoltage(uint32_t id, uint8_t* d, uint8_t dlc) {
  g_cellVoltage_board = boardNum(id);
  uint8_t packet = d[0];
  int base = (packet - 1) * 3;   // index 0-based dans g_cellVoltage[]

  if (dlc >= 7 && base + 2 < MAX_CELLS) {
    g_cellVoltage[base]   = u16(d[1], d[2]);
    g_cellVoltage[base+1] = u16(d[3], d[4]);
    g_cellVoltage[base+2] = u16(d[5], d[6]);
  }

  DBG_PRINTF("[Board %02X] Tensions cellules (paquet %d) : ", g_cellVoltage_board, packet);
  if (dlc >= 7) {
    DBG_PRINTF("Cell%d=%d mV  Cell%d=%d mV  Cell%d=%d mV\n",
      base+1, g_cellVoltage[base],
      base+2, g_cellVoltage[base+1],
      base+3, g_cellVoltage[base+2]);
  } else {
    DBG_PRINTLN("(DLC insuffisant)");
  }
}

void decodeCellTemp(uint32_t id, uint8_t* d, uint8_t dlc) {
  g_cellTemp_board = boardNum(id);
  uint8_t packet = d[0];
  int base = (packet - 1) * 7;   // index 0-based dans g_cellTemp[]

  DBG_PRINTF("[Board %02X] Températures (paquet %d) : ", g_cellTemp_board, packet);
  for (int i = 1; i <= 7 && i < dlc; i++) {
    int idx = base + i - 1;
    if (idx < MAX_NTC) g_cellTemp[idx] = (int8_t)((int)d[i] - 40);
    DBG_PRINTF("T%d=%d°C  ", idx + 1, (int)g_cellTemp[idx]);
  }
  DBG_PRINTLN();
}

void decodeTotalInfo0(uint32_t id, uint8_t* d, uint8_t dlc) {
  g_totalInfo0_board = boardNum(id);
  if (dlc < 7) { DBG_PRINTF("[Board %02X] TotalInfo0 : DLC insuffisant\n", g_totalInfo0_board); return; }

  g_totalVoltage_V = u16(d[0], d[1]) * 0.1f;
  g_current_A      = ((int32_t)u16(d[2], d[3]) - 30000) * 0.1f;
  g_soc_pct        = u16(d[4], d[5]) * 0.1f;
  g_lifeCycles     = d[6];

  DBG_PRINTF("[Board %02X] TotalInfo0 : Vtotal=%.1f V  Courant=%.1f A  SOC=%.1f%%  Cycles=%d\n",
    g_totalInfo0_board, g_totalVoltage_V, g_current_A, g_soc_pct, g_lifeCycles);

  
  StaticJsonDocument<256> doc;
  doc["batterie"]["SOC"] = g_soc_pct;
  doc["batterie"]["Tension"] = g_totalVoltage_V;
  doc["batterie"]["Current"] = g_current_A;
  String buffer;
  serializeJson(doc, buffer);
  //Serial.println(buffer);   //  UNE seule commande
  Serial1.println(buffer);  //  UNE seule commande
}


void decodeTotalInfo1(uint32_t id, uint8_t* d, uint8_t dlc) {
  g_totalInfo1_board = boardNum(id);
  if (dlc < 8) { DBG_PRINTF("[Board %02X] TotalInfo1 : DLC insuffisant\n", g_totalInfo1_board); return; }

  g_power_W         = u16(d[0], d[1]);
  g_totalEnergy_Wh  = u16(d[2], d[3]);
  g_mosFetTemp_C    = (int8_t)((int)d[4] - 40);
  g_boardTemp_C     = (int8_t)((int)d[5] - 40);
  g_heatTemp_C      = (int8_t)((int)d[6] - 40);
  g_heatCurrent_A   = d[7];

  DBG_PRINTF("[Board %02X] TotalInfo1 : Puissance=%d W  EnergieTotal=%d Wh"
    "  T_MOS=%d°C  T_Board=%d°C  T_Chauffe=%d°C  I_Chauffe=%d A\n",
    g_totalInfo1_board, g_power_W, g_totalEnergy_Wh,
    g_mosFetTemp_C, g_boardTemp_C, g_heatTemp_C, g_heatCurrent_A);
}

void decodeCellVStats(uint32_t id, uint8_t* d, uint8_t dlc) {
  g_cellVStats_board = boardNum(id);
  if (dlc < 8) { DBG_PRINTF("[Board %02X] CellVStats : DLC insuffisant\n", g_cellVStats_board); return; }

  g_maxCellV_mV  = u16(d[0], d[1]);
  g_maxCellV_no  = d[2];
  g_minCellV_mV  = u16(d[3], d[4]);
  g_minCellV_no  = d[5];
  g_diffCellV_mV = u16(d[6], d[7]);

  DBG_PRINTF("[Board %02X] Stats Tensions : Vmax=%d mV (cell#%d)  Vmin=%d mV (cell#%d)  ΔV=%d mV\n",
    g_cellVStats_board, g_maxCellV_mV, g_maxCellV_no, g_minCellV_mV, g_minCellV_no, g_diffCellV_mV);
}

void decodeCellTStats(uint32_t id, uint8_t* d, uint8_t dlc) {
  g_cellTStats_board = boardNum(id);
  if (dlc < 5) { DBG_PRINTF("[Board %02X] CellTStats : DLC insuffisant\n", g_cellTStats_board); return; }

  g_maxCellT_C  = (int8_t)((int)d[0] - 40);
  g_maxCellT_no = d[1];
  g_minCellT_C  = (int8_t)((int)d[2] - 40);
  g_minCellT_no = d[3];
  g_diffCellT_C = (int8_t)(int)d[4];

  DBG_PRINTF("[Board %02X] Stats Températures : Tmax=%d°C (cell#%d)  Tmin=%d°C (cell#%d)  ΔT=%d°C\n",
    g_cellTStats_board, g_maxCellT_C, g_maxCellT_no, g_minCellT_C, g_minCellT_no, g_diffCellT_C);
  
  StaticJsonDocument<256> doc;
  doc["batterie"]["TempMax"] = g_maxCellT_C;
  doc["batterie"]["TempMin"] = g_minCellT_C;
  String buffer;
  serializeJson(doc, buffer);
  //Serial.println(buffer);   //  UNE seule commande
  Serial1.println(buffer);  //  UNE seule commande

}

void decodeStatus0(uint32_t id, uint8_t* d, uint8_t dlc) {
  g_status0_board = boardNum(id);
  if (dlc < 5) { DBG_PRINTF("[Board %02X] Status0 : DLC insuffisant\n", g_status0_board); return; }

  g_chgMOS    = (bool)d[0];
  g_disChgMOS = (bool)d[1];
  g_preMOS    = (bool)d[2];
  g_heatMOS   = (bool)d[3];
  g_fanMOS    = (bool)d[4];

  DBG_PRINTF("[Board %02X] Status0 MOSFET : Chg=%s  DisChg=%s  PreChg=%s  Heat=%s  Fan=%s\n",
    g_status0_board,
    g_chgMOS    ? "ON" : "OFF",
    g_disChgMOS ? "ON" : "OFF",
    g_preMOS    ? "ON" : "OFF",
    g_heatMOS   ? "ON" : "OFF",
    g_fanMOS    ? "ON" : "OFF");
}

void decodeStatus1(uint32_t id, uint8_t* d, uint8_t dlc) {
  g_status1_board = boardNum(id);
  if (dlc < 5) { DBG_PRINTF("[Board %02X] Status1 : DLC insuffisant\n", g_status1_board); return; }

  g_batState   = d[0];
  g_chgDetect  = (bool)d[1];
  g_loadDetect = (bool)d[2];
  g_doState    = d[3];
  g_diState    = d[4];

  const char* batStates[] = { "Idle", "Charge", "Décharge", "?" };
  DBG_PRINTF("[Board %02X] Status1 : BatState=%s  Chargeur=%s  Charge=%s\n",
    g_status1_board,
    batStates[g_batState & 0x03],
    g_chgDetect  ? "Connecté" : "Déconnecté",
    g_loadDetect ? "Connecté" : "Déconnecté");

  DBG_PRINT("  DO : ");
  for (int i = 0; i < 8; i++) DBG_PRINTF("DO%d=%s ", i+1, (g_doState >> i) & 1 ? "ON" : "OFF");
  DBG_PRINTLN();

  const char* diLabels[] = { "POWER_KEY", "KEY", "SoftSwitch", "DI1", "DI2", "DI3", "DI4", "DI5" };
  DBG_PRINT("  DI : ");
  for (int i = 0; i < 8; i++) DBG_PRINTF("%s=%s ", diLabels[i], (g_diState >> i) & 1 ? "ON" : "OFF");
  DBG_PRINTLN();
}

void decodeStatus2(uint32_t id, uint8_t* d, uint8_t dlc) {
  g_status2_board = boardNum(id);
  if (dlc < 8) { DBG_PRINTF("[Board %02X] Status2 : DLC insuffisant\n", g_status2_board); return; }

  g_cellCount          = d[0];
  g_ntcCount           = d[1];
  g_remainCapacity_mAh = ((uint32_t)d[2] << 24) | ((uint32_t)d[3] << 16)
                       | ((uint32_t)d[4] << 8)  | d[5];
  g_cycleTimes         = u16(d[6], d[7]);

  DBG_PRINTF("[Board %02X] Status2 : Cellules=%d  NTC=%d  Cap.Restante=%lu mAh  Cycles=%d\n",
    g_status2_board, g_cellCount, g_ntcCount, (unsigned long)g_remainCapacity_mAh, g_cycleTimes);
}

void decodeFaultHW(uint32_t id, uint8_t* d, uint8_t dlc) {
  g_faultHW_board = boardNum(id);
  for (int i = 0; i < 8 && i < dlc; i++) g_faultByte[i] = d[i];

  DBG_PRINTF("[Board %02X] Alarmes/Défauts matériels :\n", g_faultHW_board);

  if (g_faultByte[0]) {
    DBG_PRINT("  [D0] ");
    if (g_faultByte[0] & 0x01) DBG_PRINT("Survoltage cell Niv1 ");
    if (g_faultByte[0] & 0x02) DBG_PRINT("Survoltage cell Niv2 ");
    if (g_faultByte[0] & 0x04) DBG_PRINT("Sous-voltage cell Niv1 ");
    if (g_faultByte[0] & 0x08) DBG_PRINT("Sous-voltage cell Niv2 ");
    if (g_faultByte[0] & 0x10) DBG_PRINT("Survoltage total Niv1 ");
    if (g_faultByte[0] & 0x20) DBG_PRINT("Survoltage total Niv2 ");
    if (g_faultByte[0] & 0x40) DBG_PRINT("Sous-voltage total Niv1 ");
    if (g_faultByte[0] & 0x80) DBG_PRINT("Sous-voltage total Niv2 ");
    DBG_PRINTLN();
  }
  if (g_faultByte[1]) {
    DBG_PRINT("  [D1] ");
    if (g_faultByte[1] & 0x01) DBG_PRINT("T_chg haute Niv1 ");
    if (g_faultByte[1] & 0x02) DBG_PRINT("T_chg haute Niv2 ");
    if (g_faultByte[1] & 0x04) DBG_PRINT("T_chg basse Niv1 ");
    if (g_faultByte[1] & 0x08) DBG_PRINT("T_chg basse Niv2 ");
    if (g_faultByte[1] & 0x10) DBG_PRINT("T_dischg haute Niv1 ");
    if (g_faultByte[1] & 0x20) DBG_PRINT("T_dischg haute Niv2 ");
    if (g_faultByte[1] & 0x40) DBG_PRINT("T_dischg basse Niv1 ");
    if (g_faultByte[1] & 0x80) DBG_PRINT("T_dischg basse Niv2 ");
    DBG_PRINTLN();
  }
  if (g_faultByte[2]) {
    DBG_PRINT("  [D2] ");
    if (g_faultByte[2] & 0x01) DBG_PRINT("Surcourant chg Niv1 ");
    if (g_faultByte[2] & 0x02) DBG_PRINT("Surcourant chg Niv2 ");
    if (g_faultByte[2] & 0x04) DBG_PRINT("Surcourant dischg Niv1 ");
    if (g_faultByte[2] & 0x08) DBG_PRINT("Surcourant dischg Niv2 ");
    if (g_faultByte[2] & 0x10) DBG_PRINT("SOC élevé Niv1 ");
    if (g_faultByte[2] & 0x20) DBG_PRINT("SOC élevé Niv2 ");
    if (g_faultByte[2] & 0x40) DBG_PRINT("SOC faible Niv1 ");
    if (g_faultByte[2] & 0x80) DBG_PRINT("SOC faible Niv2 ");
    DBG_PRINTLN();
  }
  if (g_faultByte[3]) {
    DBG_PRINT("  [D3] ");
    if (g_faultByte[3] & 0x01) DBG_PRINT("ΔV trop grand Niv1 ");
    if (g_faultByte[3] & 0x02) DBG_PRINT("ΔV trop grand Niv2 ");
    if (g_faultByte[3] & 0x04) DBG_PRINT("ΔT trop grand Niv1 ");
    if (g_faultByte[3] & 0x08) DBG_PRINT("ΔT trop grand Niv2 ");
    if (g_faultByte[3] & 0x10) DBG_PRINT("T_MOS haute Niv1 ");
    if (g_faultByte[3] & 0x20) DBG_PRINT("T_MOS haute Niv2 ");
    if (g_faultByte[3] & 0x40) DBG_PRINT("T_ambiante haute Niv1 ");
    if (g_faultByte[3] & 0x80) DBG_PRINT("T_ambiante haute Niv2 ");
    DBG_PRINTLN();
  }
  if (g_faultByte[4]) {
    DBG_PRINT("  [D4] ");
    if (g_faultByte[4] & 0x01) DBG_PRINT("Surtemp MOS_chg ");
    if (g_faultByte[4] & 0x02) DBG_PRINT("Surtemp MOS_dischg ");
    if (g_faultByte[4] & 0x04) DBG_PRINT("Capteur MOS_chg défaut ");
    if (g_faultByte[4] & 0x08) DBG_PRINT("Capteur MOS_dischg défaut ");
    if (g_faultByte[4] & 0x10) DBG_PRINT("Collage MOS_chg ");
    if (g_faultByte[4] & 0x20) DBG_PRINT("Collage MOS_dischg ");
    if (g_faultByte[4] & 0x40) DBG_PRINT("Circuit ouvert MOS_chg ");
    if (g_faultByte[4] & 0x80) DBG_PRINT("Circuit ouvert MOS_dischg ");
    DBG_PRINTLN();
  }
  if (g_faultByte[5]) {
    DBG_PRINT("  [D5] ");
    if (g_faultByte[5] & 0x01) DBG_PRINT("Défaut puce AFE ");
    if (g_faultByte[5] & 0x02) DBG_PRINT("Déconnexion cellule ");
    if (g_faultByte[5] & 0x04) DBG_PRINT("Défaut capteur T° cellule ");
    if (g_faultByte[5] & 0x08) DBG_PRINT("Défaut EEPROM ");
    if (g_faultByte[5] & 0x10) DBG_PRINT("Défaut RTC ");
    if (g_faultByte[5] & 0x20) DBG_PRINT("Échec pré-charge ");
    if (g_faultByte[5] & 0x40) DBG_PRINT("Défaut com. véhicule ");
    if (g_faultByte[5] & 0x80) DBG_PRINT("Défaut module com. réseau ");
    DBG_PRINTLN();
  }
  if (g_faultByte[6]) {
    DBG_PRINT("  [D6] ");
    if (g_faultByte[6] & 0x01) DBG_PRINT("Défaut module courant ");
    if (g_faultByte[6] & 0x02) DBG_PRINT("Défaut détection Vtotal interne ");
    if (g_faultByte[6] & 0x04) DBG_PRINT("Défaut protection court-circuit ");
    if (g_faultByte[6] & 0x08) DBG_PRINT("Interdiction chg basse tension ");
    if (g_faultByte[6] & 0x10) DBG_PRINT("GPS/SoftSwitch déconnecte MOS ");
    if (g_faultByte[6] & 0x20) DBG_PRINT("Chargeur hors-bord ");
    if (g_faultByte[6] & 0x40) DBG_PRINT("THERMAL RUNAWAY ! ");
    if (g_faultByte[6] & 0x80) DBG_PRINT("Défaut chauffe ");
    DBG_PRINTLN();
  }
  if (g_faultByte[7]) {
    DBG_PRINT("  [D7] ");
    if (g_faultByte[7] & 0x01) DBG_PRINT("Défaut com. module balance ");
    if (g_faultByte[7] & 0x02) DBG_PRINT("Condition activation balance non remplie ");
    DBG_PRINTLN();
  }
}

void decodeChgInfo(uint32_t id, uint8_t* d, uint8_t dlc) {
  g_chgInfo_board = boardNum(id);
  if (dlc < 3) { DBG_PRINTF("[Board %02X] ChgInfo : DLC insuffisant\n", g_chgInfo_board); return; }

  g_remainChgTime_min = u16(d[0], d[1]);
  g_wakeupSource      = d[2];

  DBG_PRINTF("[Board %02X] Info Charge : Temps restant=%d min  Source réveil :", g_chgInfo_board, g_remainChgTime_min);
  if (g_wakeupSource & 0x01) DBG_PRINT(" KEY");
  if (g_wakeupSource & 0x02) DBG_PRINT(" Bouton");
  if (g_wakeupSource & 0x04) DBG_PRINT(" RS485");
  if (g_wakeupSource & 0x08) DBG_PRINT(" CAN");
  if (g_wakeupSource & 0x10) DBG_PRINT(" Courant");
  DBG_PRINTLN();
}

void decodeCurrentLimit(uint32_t id, uint8_t* d, uint8_t dlc) {
  g_curLimit_board = boardNum(id);
  if (dlc < 3) { DBG_PRINTF("[Board %02X] CurrentLimit : DLC insuffisant\n", g_curLimit_board); return; }

  g_curLimitEnabled = (d[0] == 1);
  g_curLimit_A      = ((int32_t)u16(d[1], d[2]) - 30000) * 0.1f;
  g_soh_pct         = 0;
  g_pwmDuty_pct     = 0.0f;
  if (dlc >= 7) {
    g_soh_pct     = (uint8_t)(u16(d[3], d[4]) & 0xFF);
    g_pwmDuty_pct = u16(d[5], d[6]) * 0.1f;
  }

  DBG_PRINTF("[Board %02X] Limitation courant : %s  Limite=%.1f A  SOH=%d%%  PWM=%.1f%%\n",
    g_curLimit_board, g_curLimitEnabled ? "ACTIVÉE" : "désactivée",
    g_curLimit_A, g_soh_pct, g_pwmDuty_pct);
}

void decodeFaultsDetail(uint32_t id, uint8_t* d, uint8_t dlc) {
  g_faultDetail_board = boardNum(id);
  g_faultDetailPage   = d[0];
  for (int i = 1; i < dlc && i <= 7; i++) g_faultDetailData[i-1] = d[i];

  DBG_PRINTF("[Board %02X] Défauts détaillés (page %d) : ", g_faultDetail_board, g_faultDetailPage);
  for (int i = 1; i < dlc; i++) DBG_PRINTF("D%d=0x%02X ", i, d[i]);
  DBG_PRINTLN();
}

// ----------------------------------------------------------------
//  Trame inconnue — dump brut
// ----------------------------------------------------------------
void dumpRaw(CanFrame& f) {
  DBG_PRINTF("[RAW] ID=0x%08X DLC=%d : ", f.identifier, f.data_length_code);
  for (int i = 0; i < f.data_length_code; i++) DBG_PRINTF("%02X ", f.data[i]);
  DBG_PRINTLN();
}

// ================================================================
//  Dispatch principal
// ================================================================
void decodeFrame(CanFrame& f) {

 if ((f.identifier == 0x0CF11E05) || (f.identifier == 0x0CF11F05)) return;
  uint32_t mid = maskedId(f.identifier);
  uint8_t* d   = f.data;
  uint8_t  dlc = f.data_length_code;
  //Serial.printf("Message %ll de la batterie\n", mid);
  if      (mid == ID_CELL_VOLTAGE)  decodeCellVoltage  (f.identifier, d, dlc);
  else if (mid == ID_CELL_TEMP)     decodeCellTemp     (f.identifier, d, dlc);
  else if (mid == ID_TOTAL_INFO0)   decodeTotalInfo0   (f.identifier, d, dlc);
  else if (mid == ID_TOTAL_INFO1)   decodeTotalInfo1   (f.identifier, d, dlc);
  else if (mid == ID_CELL_V_STATS)  decodeCellVStats   (f.identifier, d, dlc);
  else if (mid == ID_CELL_T_STATS)  decodeCellTStats   (f.identifier, d, dlc);
  else if (mid == ID_STATUS_0)      decodeStatus0      (f.identifier, d, dlc);
  else if (mid == ID_STATUS_1)      decodeStatus1      (f.identifier, d, dlc);
  else if (mid == ID_STATUS_2)      decodeStatus2      (f.identifier, d, dlc);
  else if (mid == ID_FAULT_HW)      decodeFaultHW      (f.identifier, d, dlc);
  else if (mid == ID_CHG_INFO)      decodeChgInfo      (f.identifier, d, dlc);
  else if (mid == ID_CURRENT_LIMIT) decodeCurrentLimit (f.identifier, d, dlc);
  else if (mid == ID_FAULTS_DETAIL) decodeFaultsDetail (f.identifier, d, dlc);
  else                              dumpRaw            (f);
}

/*
// ================================================================
//  Setup / Loop
// ================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

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
  static unsigned long lastQuery = 0;

  if (millis() - lastQuery >= QUERY_INTERVAL_MS) {
    lastQuery = millis();
    sendQueryFrame();
  }

}
*/
