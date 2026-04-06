#include <Arduino.h>
#include <ESP32-TWAI-CAN.hpp>

#define PRINTING_DEBUG true

#if PRINTING_DEBUG
  #define DBG_PRINT(...)    Serial.print(__VA_ARGS__)
  #define DBG_PRINTLN(...)  Serial.println(__VA_ARGS__)
  #define DBG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
  #define DBG_PRINT(...)
  #define DBG_PRINTLN(...)
  #define DBG_PRINTF(...)
#endif

// ----------------------------------------------------------------
//  Config CAN
// ----------------------------------------------------------------
#define CAN_TX        22
#define CAN_RX        21
#define CAN_BAUDRATE  250   // kbps

#define ID_MASK             0xFFFFFF00UL
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

#define ID_QUERY            0x0400FF80UL
#define QUERY_INTERVAL_MS   1000

#define MAX_CELLS     32   // nombre max de cellules supportées
#define MAX_NTC       16   // nombre max de sondes de température


// ================================================================
//  VARIABLES GLOBALES — mises à jour à chaque trame reçue
// ================================================================

#include <ArduinoJson.h>
// --- 0x040080** : Tensions cellules (mV) ---
extern uint16_t g_cellVoltage[MAX_CELLS];        // [i] = tension cellule i+1 en mV
extern uint8_t  g_cellVoltage_board;             // numéro de board source

// --- 0x040180** : Températures cellules (°C) ---
extern int8_t   g_cellTemp[MAX_NTC];             // [i] = température NTC i+1 en °C (offset -40 appliqué)
extern uint8_t  g_cellTemp_board;

// --- 0x040280** : Total Info 0 ---
extern float    g_totalVoltage_V;                // tension totale en V (résolution 0.1 V)
extern float    g_current_A;                     // courant en A (résolution 0.1 A, offset -30000 ; négatif = charge)
extern float    g_soc_pct;                       // SOC en % (résolution 0.1 %)
extern uint8_t  g_lifeCycles;                    // compteur de cycles 0..255
extern uint8_t  g_totalInfo0_board;

// --- 0x040380** : Total Info 1 ---
extern uint16_t g_power_W;                       // puissance en W
extern uint16_t g_totalEnergy_Wh;               // énergie totale en Wh
extern int8_t   g_mosFetTemp_C;                 // température MOS en °C (offset -40)
extern int8_t   g_boardTemp_C;                  // température ambiante carte en °C (offset -40)
extern int8_t   g_heatTemp_C;                   // température film chauffant en °C (offset -40)
extern uint8_t  g_heatCurrent_A;               // courant de chauffe en A
extern uint8_t  g_totalInfo1_board;

// --- 0x040480** : Stats tensions cellules ---
extern uint16_t g_maxCellV_mV;                  // tension max en mV
extern uint8_t  g_maxCellV_no;                  // numéro de la cellule à Vmax
extern uint16_t g_minCellV_mV;                  // tension min en mV
extern uint8_t  g_minCellV_no;                  // numéro de la cellule à Vmin
extern uint16_t g_diffCellV_mV;                 // différence Vmax - Vmin en mV
extern uint8_t  g_cellVStats_board;

// --- 0x040580** : Stats températures cellules ---
extern int8_t   g_maxCellT_C;                   // température max en °C (offset -40)
extern uint8_t  g_maxCellT_no;                  // numéro de la cellule à Tmax
extern int8_t   g_minCellT_C;                   // température min en °C (offset -40)
extern uint8_t  g_minCellT_no;                  // numéro de la cellule à Tmin
extern int8_t   g_diffCellT_C;                  // différence Tmax - Tmin
extern uint8_t  g_cellTStats_board;

// --- 0x040680** : Status 0 — état MOSFET ---
extern bool     g_chgMOS;                       // MOSFET charge actif
extern bool     g_disChgMOS;                    // MOSFET décharge actif
extern bool     g_preMOS;                       // MOSFET pré-charge actif
extern bool     g_heatMOS;                      // MOSFET chauffe actif
extern bool     g_fanMOS;                       // MOSFET ventilateur actif
extern uint8_t  g_status0_board;

// --- 0x040780** : Status 1 — état batterie / connexions / ports ---
extern uint8_t  g_batState;                     // 0=idle, 1=charge, 2=décharge
extern bool     g_chgDetect;                    // chargeur connecté
extern bool     g_loadDetect;                   // charge connectée
extern uint8_t  g_doState;                      // état sorties digitales DO1..DO8 (bitmask)
extern uint8_t  g_diState;                      // état entrées digitales DI (bitmask)
extern uint8_t  g_status1_board;

// --- 0x040880** : Status 2 ---
extern uint8_t  g_cellCount;                    // nombre de cellules en série
extern uint8_t  g_ntcCount;                     // nombre de sondes NTC
extern uint32_t g_remainCapacity_mAh;           // capacité restante en mAh
extern uint16_t g_cycleTimes;                   // nombre de cycles charge/décharge
extern uint8_t  g_status2_board;

// --- 0x040980** : Alarmes et défauts matériels (8 octets de bits) ---
extern uint8_t  g_faultByte[8];                 // [0..7] = octets bruts des alarmes/défauts
extern uint8_t  g_faultHW_board;

// --- 0x040B80** : Informations de charge ---
extern uint16_t g_remainChgTime_min;            // temps de charge restant en minutes
extern uint8_t  g_wakeupSource;                 // bitmask source de réveil (bit0=KEY, 1=BTN, 2=485, 3=CAN, 4=CURRENT)
extern uint8_t  g_chgInfo_board;

// --- 0x040D80** : Limitation de courant ---
extern bool     g_curLimitEnabled;             // limitation activée
extern float    g_curLimit_A;                  // valeur limite en A (offset -30000)
extern uint8_t  g_soh_pct;                     // état de santé en %
extern float    g_pwmDuty_pct;                 // rapport cyclique PWM en %
extern uint8_t  g_curLimit_board;

// --- 0x040E80** : Défauts détaillés (brut) ---
extern uint8_t  g_faultDetailPage;             // numéro de page
extern uint8_t  g_faultDetailData[7];          // octets D1..D7
extern uint8_t  g_faultDetail_board;

void decodeFrame(CanFrame& f);
void sendQueryFrame();