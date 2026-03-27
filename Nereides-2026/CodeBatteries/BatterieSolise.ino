// ============================================================
//  KVMS BMS — Décodeur de trames CAN -> Serial
//  Basé sur : KVMS_Intranet_Communication_CAN_Protocol.xlsx
//  Hardware  : ESP32 + ESP32-TWAI-CAN
//  CAN H : fil blanc  |  CAN L : fil jaune
//  Pas de résistance de terminaison externe nécessaire
// ============================================================

#include <ESP32-TWAI-CAN.hpp>

#define CAN_TX        22
#define CAN_RX        21
#define CAN_BAUDRATE  250   // kbps

// ----------------------------------------------------------------
//  Masque de board number : les 2 derniers octets de l'ID varient
//  On garde les 3 premiers octets et on ignore les 2 derniers (**).
//  Ex : 0x040080** -> masque = 0x04008000, base = 0x04008000
// ----------------------------------------------------------------
#define ID_MASK             0xFFFFFF00UL

// IDs de base (** = 0x00 pour le masque)
#define ID_CELL_VOLTAGE     0x04008000UL   // 0x040080**
#define ID_CELL_TEMP        0x04018000UL   // 0x040180**
#define ID_TOTAL_INFO0      0x04028000UL   // 0x040280**
#define ID_TOTAL_INFO1      0x04038000UL   // 0x040380**
#define ID_CELL_V_STATS     0x04048000UL   // 0x040480**
#define ID_CELL_T_STATS     0x04058000UL   // 0x040580**
#define ID_STATUS_0         0x04068000UL   // 0x040680**
#define ID_STATUS_1         0x04078000UL   // 0x040780**
#define ID_STATUS_2         0x04088000UL   // 0x040880**
#define ID_FAULT_HW         0x04098000UL   // 0x040980**
#define ID_CHG_INFO         0x040B8000UL   // 0x040B80**
#define ID_CURRENT_LIMIT    0x040D8000UL   // 0x040D80**
#define ID_FAULTS_DETAIL    0x040E8000UL   // 0x040E80**

// Trame de requête périodique
#define ID_QUERY            0x0400FF80UL   // Query Frame (étendu)
#define QUERY_INTERVAL_MS   2000

// ----------------------------------------------------------------
//  Utilitaires
// ----------------------------------------------------------------
static inline uint16_t u16(uint8_t hi, uint8_t lo) {
  return ((uint16_t)hi << 8) | lo;
}

// Retourne l'ID masqué pour la comparaison
static inline uint32_t maskedId(uint32_t id) {
  return id & ID_MASK;
}

// Extrait le numéro de board (2 derniers octets de l'ID)
static inline uint8_t boardNum(uint32_t id) {
  return (uint8_t)(id & 0xFF);
}

// ----------------------------------------------------------------
//  Envoi de la trame de requête périodique
// ----------------------------------------------------------------
void sendQueryFrame() {
  CanFrame query;
  query.identifier      = ID_QUERY;
  query.extd            = 1;
  query.data_length_code = 8;
  for (int i = 0; i < 8; i++) query.data[i] = 0x00;

  if (ESP32Can.writeFrame(query)) {
    Serial.println("[TX] Trame requête 0x0400FF80 envoyée");
  } else {
    Serial.println("[TX] ERREUR envoi requête");
  }
}

// ================================================================
//  Décodeurs par type de trame
// ================================================================

// 0x040080** — Tensions de cellules (paquet de 3 tensions par trame)
// Data0 = n° de paquet (1, 2, …)
// Data1/2 = Volt(3n-2) High/Low, Data3/4 = Volt(3n-1)H/L, Data5/6 = Volt(3n)H/L
// Résolution : 1 mV/bit
void decodeCellVoltage(uint32_t id, uint8_t* d, uint8_t dlc) {
  uint8_t board  = boardNum(id);
  uint8_t packet = d[0];                // numéro de paquet (1-based)
  int base = (packet - 1) * 3 + 1;     // index de la première cellule

  Serial.printf("[Board %02X] Tensions cellules (paquet %d) : ", board, packet);
  if (dlc >= 7) {
    Serial.printf("Cell%d=%d mV  Cell%d=%d mV  Cell%d=%d mV\n",
      base,   u16(d[1], d[2]),
      base+1, u16(d[3], d[4]),
      base+2, u16(d[5], d[6]));
  } else {
    Serial.println("(DLC insuffisant)");
  }
}

// 0x040180** — Températures de cellules (7 valeurs par trame)
// Résolution : 1 °C/bit, offset -40 (valeur brute 40 = 0 °C)
void decodeCellTemp(uint32_t id, uint8_t* d, uint8_t dlc) {
  uint8_t board  = boardNum(id);
  uint8_t packet = d[0];
  int base = (packet - 1) * 7 + 1;

  Serial.printf("[Board %02X] Températures (paquet %d) : ", board, packet);
  for (int i = 1; i <= 7 && i < dlc; i++) {
    int tempC = (int)d[i] - 40;
    Serial.printf("T%d=%d°C  ", base + i - 1, tempC);
  }
  Serial.println();
}

// 0x040280** — Informations totales 0
// SumV (0.1 V/bit) | Curr (0.1 A/bit, offset -30000) | SOC (0.1 %/bit) | Life (cycles)
void decodeTotalInfo0(uint32_t id, uint8_t* d, uint8_t dlc) {
  uint8_t board = boardNum(id);
  if (dlc < 7) { Serial.printf("[Board %02X] TotalInfo0 : DLC insuffisant\n", board); return; }

  uint16_t sumV_raw = u16(d[0], d[1]);
  uint16_t curr_raw = u16(d[2], d[3]);
  uint16_t soc_raw  = u16(d[4], d[5]);
  uint8_t  life     = d[6];

  float sumV = sumV_raw * 0.1f;
  float curr = ((int32_t)curr_raw - 30000) * 0.1f;  // négatif = charge
  float soc  = soc_raw * 0.1f;

  Serial.printf("[Board %02X] TotalInfo0 : Vtotal=%.1f V  Courant=%.1f A  SOC=%.1f%%  Cycles=%d\n",
    board, sumV, curr, soc, life);
}

// 0x040380** — Informations totales 1
// POWER (1 W/bit) | TotalEnergy (1 Wh/bit) | MOS_Temp | Board_Temp | Heat_Temp | Heat_Cur
void decodeTotalInfo1(uint32_t id, uint8_t* d, uint8_t dlc) {
  uint8_t board = boardNum(id);
  if (dlc < 8) { Serial.printf("[Board %02X] TotalInfo1 : DLC insuffisant\n", board); return; }

  uint16_t power  = u16(d[0], d[1]);
  uint16_t energy = u16(d[2], d[3]);
  int mosTemp    = (int)d[4] - 40;
  int boardTemp  = (int)d[5] - 40;
  int heatTemp   = (int)d[6] - 40;
  uint8_t heatCur = d[7];

  Serial.printf("[Board %02X] TotalInfo1 : Puissance=%d W  EnergieTotal=%d Wh"
    "  T_MOS=%d°C  T_Board=%d°C  T_Chauffe=%d°C  I_Chauffe=%d A\n",
    board, power, energy, mosTemp, boardTemp, heatTemp, heatCur);
}

// 0x040480** — Stats tensions cellules
// MaxV (mV) | MaxV_No | MinV (mV) | MinV_No | DiffV (mV)
void decodeCellVStats(uint32_t id, uint8_t* d, uint8_t dlc) {
  uint8_t board = boardNum(id);
  if (dlc < 8) { Serial.printf("[Board %02X] CellVStats : DLC insuffisant\n", board); return; }

  uint16_t maxV  = u16(d[0], d[1]);
  uint8_t  maxNo = d[2];
  uint16_t minV  = u16(d[3], d[4]);
  uint8_t  minNo = d[5];
  uint16_t diffV = u16(d[6], d[7]);

  Serial.printf("[Board %02X] Stats Tensions : Vmax=%d mV (cell#%d)  Vmin=%d mV (cell#%d)  ΔV=%d mV\n",
    board, maxV, maxNo, minV, minNo, diffV);
}

// 0x040580** — Stats températures cellules
// MaxT | MaxTNo | MinT | MinTNo | DiffT  (offset -40)
void decodeCellTStats(uint32_t id, uint8_t* d, uint8_t dlc) {
  uint8_t board = boardNum(id);
  if (dlc < 5) { Serial.printf("[Board %02X] CellTStats : DLC insuffisant\n", board); return; }

  int maxT  = (int)d[0] - 40;
  uint8_t maxNo = d[1];
  int minT  = (int)d[2] - 40;
  uint8_t minNo = d[3];
  int diffT = (int)d[4];

  Serial.printf("[Board %02X] Stats Températures : Tmax=%d°C (cell#%d)  Tmin=%d°C (cell#%d)  ΔT=%d°C\n",
    board, maxT, maxNo, minT, minNo, diffT);
}

// 0x040680** — Status 0 : état des MOSFET
void decodeStatus0(uint32_t id, uint8_t* d, uint8_t dlc) {
  uint8_t board = boardNum(id);
  if (dlc < 5) { Serial.printf("[Board %02X] Status0 : DLC insuffisant\n", board); return; }

  Serial.printf("[Board %02X] Status0 MOSFET : Chg=%s  DisChg=%s  PreChg=%s  Heat=%s  Fan=%s\n",
    board,
    d[0] ? "ON" : "OFF",
    d[1] ? "ON" : "OFF",
    d[2] ? "ON" : "OFF",
    d[3] ? "ON" : "OFF",
    d[4] ? "ON" : "OFF");
}

// 0x040780** — Status 1 : état batterie / détections / ports DO/DI
void decodeStatus1(uint32_t id, uint8_t* d, uint8_t dlc) {
  uint8_t board = boardNum(id);
  if (dlc < 5) { Serial.printf("[Board %02X] Status1 : DLC insuffisant\n", board); return; }

  const char* batStates[] = { "Idle", "Charge", "Décharge", "?" };
  Serial.printf("[Board %02X] Status1 : BatState=%s  Chargeur=%s  Charge=%s\n",
    board,
    batStates[d[0] & 0x03],
    d[1] ? "Connecté" : "Déconnecté",
    d[2] ? "Connecté" : "Déconnecté");

  // Ports DO (bit0..7 = DO1..DO8)
  Serial.printf("  DO : ");
  for (int i = 0; i < 8; i++) Serial.printf("DO%d=%s ", i+1, (d[3] >> i) & 1 ? "ON" : "OFF");
  Serial.println();

  // Ports DI (bit0=POWER_KEY, bit1=Key, bit2=SoftSwitch, bit3..7=DI1..5)
  const char* diLabels[] = { "POWER_KEY", "KEY", "SoftSwitch", "DI1", "DI2", "DI3", "DI4", "DI5" };
  Serial.printf("  DI : ");
  for (int i = 0; i < 8; i++) Serial.printf("%s=%s ", diLabels[i], (d[4] >> i) & 1 ? "ON" : "OFF");
  Serial.println();
}

// 0x040880** — Status 2 : infos générales batterie
void decodeStatus2(uint32_t id, uint8_t* d, uint8_t dlc) {
  uint8_t board = boardNum(id);
  if (dlc < 8) { Serial.printf("[Board %02X] Status2 : DLC insuffisant\n", board); return; }

  uint8_t  cellCount   = d[0];
  uint8_t  ntcCount    = d[1];
  uint32_t remainCap   = ((uint32_t)d[2] << 24) | ((uint32_t)d[3] << 16)
                       | ((uint32_t)d[4] << 8)  | d[5];
  uint16_t cycles      = u16(d[6], d[7]);

  Serial.printf("[Board %02X] Status2 : Cellules=%d  NTC=%d  Cap.Restante=%lu mAh  Cycles=%d\n",
    board, cellCount, ntcCount, (unsigned long)remainCap, cycles);
}

// 0x040980** — Alarmes et défauts matériels (8 octets de bits)
void decodeFaultHW(uint32_t id, uint8_t* d, uint8_t dlc) {
  uint8_t board = boardNum(id);
  Serial.printf("[Board %02X] Alarmes/Défauts matériels :\n", board);

  // Byte 0 — surtensions/sous-tensions cellules et totales
  if (d[0]) {
    Serial.print("  [D0] ");
    if (d[0] & 0x01) Serial.print("Survoltage cell Niv1 ");
    if (d[0] & 0x02) Serial.print("Survoltage cell Niv2 ");
    if (d[0] & 0x04) Serial.print("Sous-voltage cell Niv1 ");
    if (d[0] & 0x08) Serial.print("Sous-voltage cell Niv2 ");
    if (d[0] & 0x10) Serial.print("Survoltage total Niv1 ");
    if (d[0] & 0x20) Serial.print("Survoltage total Niv2 ");
    if (d[0] & 0x40) Serial.print("Sous-voltage total Niv1 ");
    if (d[0] & 0x80) Serial.print("Sous-voltage total Niv2 ");
    Serial.println();
  }
  // Byte 1 — températures de charge/décharge
  if (d[1]) {
    Serial.print("  [D1] ");
    if (d[1] & 0x01) Serial.print("T_chg haute Niv1 ");
    if (d[1] & 0x02) Serial.print("T_chg haute Niv2 ");
    if (d[1] & 0x04) Serial.print("T_chg basse Niv1 ");
    if (d[1] & 0x08) Serial.print("T_chg basse Niv2 ");
    if (d[1] & 0x10) Serial.print("T_dischg haute Niv1 ");
    if (d[1] & 0x20) Serial.print("T_dischg haute Niv2 ");
    if (d[1] & 0x40) Serial.print("T_dischg basse Niv1 ");
    if (d[1] & 0x80) Serial.print("T_dischg basse Niv2 ");
    Serial.println();
  }
  // Byte 2 — surcourants / SOC
  if (d[2]) {
    Serial.print("  [D2] ");
    if (d[2] & 0x01) Serial.print("Surcourant chg Niv1 ");
    if (d[2] & 0x02) Serial.print("Surcourant chg Niv2 ");
    if (d[2] & 0x04) Serial.print("Surcourant dischg Niv1 ");
    if (d[2] & 0x08) Serial.print("Surcourant dischg Niv2 ");
    if (d[2] & 0x10) Serial.print("SOC élevé Niv1 ");
    if (d[2] & 0x20) Serial.print("SOC élevé Niv2 ");
    if (d[2] & 0x40) Serial.print("SOC faible Niv1 ");
    if (d[2] & 0x80) Serial.print("SOC faible Niv2 ");
    Serial.println();
  }
  // Byte 3 — différences tension/température / T° MOSFET / T° ambiante
  if (d[3]) {
    Serial.print("  [D3] ");
    if (d[3] & 0x01) Serial.print("ΔV trop grand Niv1 ");
    if (d[3] & 0x02) Serial.print("ΔV trop grand Niv2 ");
    if (d[3] & 0x04) Serial.print("ΔT trop grand Niv1 ");
    if (d[3] & 0x08) Serial.print("ΔT trop grand Niv2 ");
    if (d[3] & 0x10) Serial.print("T_MOS haute Niv1 ");
    if (d[3] & 0x20) Serial.print("T_MOS haute Niv2 ");
    if (d[3] & 0x40) Serial.print("T_ambiante haute Niv1 ");
    if (d[3] & 0x80) Serial.print("T_ambiante haute Niv2 ");
    Serial.println();
  }
  // Byte 4 — défauts MOSFET
  if (d[4]) {
    Serial.print("  [D4] ");
    if (d[4] & 0x01) Serial.print("Surtemp MOS_chg ");
    if (d[4] & 0x02) Serial.print("Surtemp MOS_dischg ");
    if (d[4] & 0x04) Serial.print("Capteur MOS_chg défaut ");
    if (d[4] & 0x08) Serial.print("Capteur MOS_dischg défaut ");
    if (d[4] & 0x10) Serial.print("Collage MOS_chg ");
    if (d[4] & 0x20) Serial.print("Collage MOS_dischg ");
    if (d[4] & 0x40) Serial.print("Circuit ouvert MOS_chg ");
    if (d[4] & 0x80) Serial.print("Circuit ouvert MOS_dischg ");
    Serial.println();
  }
  // Byte 5 — défauts AFE / capteurs / stockage
  if (d[5]) {
    Serial.print("  [D5] ");
    if (d[5] & 0x01) Serial.print("Défaut puce AFE ");
    if (d[5] & 0x02) Serial.print("Déconnexion cellule ");
    if (d[5] & 0x04) Serial.print("Défaut capteur T° cellule ");
    if (d[5] & 0x08) Serial.print("Défaut EEPROM ");
    if (d[5] & 0x10) Serial.print("Défaut RTC ");
    if (d[5] & 0x20) Serial.print("Échec pré-charge ");
    if (d[5] & 0x40) Serial.print("Défaut com. véhicule ");
    if (d[5] & 0x80) Serial.print("Défaut module com. réseau ");
    Serial.println();
  }
  // Byte 6 — autres défauts hardware
  if (d[6]) {
    Serial.print("  [D6] ");
    if (d[6] & 0x01) Serial.print("Défaut module courant ");
    if (d[6] & 0x02) Serial.print("Défaut détection Vtotal interne ");
    if (d[6] & 0x04) Serial.print("Défaut protection court-circuit ");
    if (d[6] & 0x08) Serial.print("Interdiction chg basse tension ");
    if (d[6] & 0x10) Serial.print("GPS/SoftSwitch déconnecte MOS ");
    if (d[6] & 0x20) Serial.print("Chargeur hors-bord ");
    if (d[6] & 0x40) Serial.print("THERMAL RUNAWAY ! ");
    if (d[6] & 0x80) Serial.print("Défaut chauffe ");
    Serial.println();
  }
  // Byte 7 — défauts module d'équilibrage
  if (d[7]) {
    Serial.print("  [D7] ");
    if (d[7] & 0x01) Serial.print("Défaut com. module balance ");
    if (d[7] & 0x02) Serial.print("Condition activation balance non remplie ");
    Serial.println();
  }
}

// 0x040B80** — Informations de charge
void decodeChgInfo(uint32_t id, uint8_t* d, uint8_t dlc) {
  uint8_t board = boardNum(id);
  if (dlc < 3) { Serial.printf("[Board %02X] ChgInfo : DLC insuffisant\n", board); return; }

  uint16_t restTime   = u16(d[0], d[1]);
  uint8_t  wakeSrc    = d[2];

  Serial.printf("[Board %02X] Info Charge : Temps restant=%d min  Source réveil :", board, restTime);
  if (wakeSrc & 0x01) Serial.print(" KEY");
  if (wakeSrc & 0x02) Serial.print(" Bouton");
  if (wakeSrc & 0x04) Serial.print(" RS485");
  if (wakeSrc & 0x08) Serial.print(" CAN");
  if (wakeSrc & 0x10) Serial.print(" Courant");
  Serial.println();
}

// 0x040D80** — Limitation de courant
void decodeCurrentLimit(uint32_t id, uint8_t* d, uint8_t dlc) {
  uint8_t board = boardNum(id);
  if (dlc < 3) { Serial.printf("[Board %02X] CurrentLimit : DLC insuffisant\n", board); return; }

  bool     enabled  = d[0] == 1;
  uint16_t cur_raw  = u16(d[1], d[2]);
  float    cur_A    = ((int32_t)cur_raw - 30000) * 0.1f;

  uint8_t soh = 0; float pwm = 0.0f;
  if (dlc >= 7) {
    soh = (uint8_t)(u16(d[3], d[4]) & 0xFF);
    pwm = u16(d[5], d[6]) * 0.1f;
  }

  Serial.printf("[Board %02X] Limitation courant : %s  Limite=%.1f A  SOH=%d%%  PWM=%.1f%%\n",
    board, enabled ? "ACTIVÉE" : "désactivée", cur_A, soh, pwm);
}

// 0x040E80** — Défauts détaillés (pages)
void decodeFaultsDetail(uint32_t id, uint8_t* d, uint8_t dlc) {
  uint8_t board  = boardNum(id);
  uint8_t pageNo = d[0];
  Serial.printf("[Board %02X] Défauts détaillés (page %d) : ", board, pageNo);
  // Affichage brut des 7 octets de données utiles (D1..D7)
  for (int i = 1; i < dlc; i++) Serial.printf("D%d=0x%02X ", i, d[i]);
  Serial.println();
}

// ================================================================
//  Trame inconnue — dump brut
// ================================================================
void dumpRaw(CanFrame& f) {
  Serial.printf("[RAW] ID=0x%08X DLC=%d : ", f.identifier, f.data_length_code);
  for (int i = 0; i < f.data_length_code; i++) Serial.printf("%02X ", f.data[i]);
  Serial.println();
}

// ================================================================
//  Dispatch principal
// ================================================================
void decodeFrame(CanFrame& f) {
  uint32_t mid = maskedId(f.identifier);
  uint8_t* d   = f.data;
  uint8_t  dlc = f.data_length_code;

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

  Serial.printf("[OK] CAN initialisé à %d kbps\n", CAN_BAUDRATE);
  Serial.println("[OK] Démarrage décodage trames KVMS BMS\n");

  sendQueryFrame();
}

void loop() {
  static unsigned long lastQuery = 0;

  // Envoi périodique de la requête (toutes les 2 s)
  if (millis() - lastQuery >= QUERY_INTERVAL_MS) {
    lastQuery = millis();
    sendQueryFrame();
  }

  // Lecture et décodage des trames reçues (non-bloquant)
  CanFrame frame;
  if (ESP32Can.readFrame(frame, 0)) {
    decodeFrame(frame);
  }
}
