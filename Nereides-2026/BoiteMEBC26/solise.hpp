#pragma once
#ifndef SOLISE_HPP
#define SOLISE_HPP

#include <Arduino.h>
#include <ESP32-TWAI-CAN.hpp>
#include <ArduinoJson.h>

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

// ----------------------------------------------------------------
//  Identifiants CAN des batteries (BMS JBD - protocole "intranet")
//  Ce sont des identifiants standards 11 bits (pas étendus, contrairement
//  à l'ancien BMS KVMS qui utilisait des ID 29 bits du type 0x040xxx80).
//
//  Batterie 1 :
//    0x211 -> trame globale (tension totale / courant / SOC / protection)
//    0x212 -> trame "1 cellule par message" (index cellule + tension)
//    0x213 -> température maximale (parmi les sondes NTC du BMS)
//  Batterie 2 :
//    0x311 -> trame globale
//    0x312 -> trame cellule
//    0x313 -> température maximale
// ----------------------------------------------------------------
#define ID_BAT1_GLOBAL     0x211
#define ID_BAT1_CELL       0x212
#define ID_BAT1_TEMP_MAX   0x213
#define ID_BAT2_GLOBAL     0x311
#define ID_BAT2_CELL       0x312
#define ID_BAT2_TEMP_MAX   0x313

#define MAX_CELLS  32   // nombre max de cellules supportées par batterie

// ----------------------------------------------------------------
//  Bits du registre "Statut Protection" (B5/B6 LSB first des trames
//  globales 0x211 / 0x311)
// ----------------------------------------------------------------
#define PROT_BIT_CELL_OV        0   // Surtension bloc cellule
#define PROT_BIT_CELL_UV        1   // Sous-tension bloc cellule
#define PROT_BIT_PACK_OV        2   // Surtension pack global
#define PROT_BIT_PACK_UV        3   // Sous-tension pack global
#define PROT_BIT_CHG_OT         4   // Surchauffe en charge
#define PROT_BIT_CHG_UT         5   // Sous-température en charge
#define PROT_BIT_DISCHG_OT      6   // Surchauffe en décharge
#define PROT_BIT_DISCHG_UT      7   // Sous-température en décharge
#define PROT_BIT_CHG_OC         8   // Surintensité / surcharge en charge
#define PROT_BIT_DISCHG_OC      9   // Surintensité en décharge
#define PROT_BIT_SHORT_CIRCUIT  10  // Court-circuit
#define PROT_BIT_IC_ERROR       11  // Erreur IC de gestion
#define PROT_BIT_MOSFET_ERROR   12  // Erreur MOSFET
// Bits 13 à 15 : réservés, non utilisés par le BMS JBD

// ----------------------------------------------------------------
//  Données décodées d'une batterie
// ----------------------------------------------------------------
struct BatteryData {
  float    totalVoltage_V    = 0.0f;  // tension totale, résolution 0.01 V
  float    current_A         = 0.0f;  // courant, résolution 0.01 A (positif = charge, négatif = décharge)
  uint8_t  soc_pct            = 0;    // état de charge, 0 à 100 %
  uint16_t protectionStatus   = 0;    // registre de bits, voir PROT_BIT_*

  uint16_t cellVoltage_mV[MAX_CELLS] = {0}; // tension de chaque cellule en mV (indice 0 = cellule 1)
  uint8_t  cellCount          = 0;    // plus grand index de cellule reçu jusqu'ici

  float    tempMax_C          = 0.0f; // température maximale parmi les sondes NTC, résolution 0.1 °C
  bool     receivedTemp       = false; // au moins une trame de température a été reçue

  bool     receivedGlobal     = false; // au moins une trame globale a été reçue
};

extern BatteryData g_battery1;  // batterie #1 (IDs 0x211 / 0x212)
extern BatteryData g_battery2;  // batterie #2 (IDs 0x311 / 0x312)

void decodeFrame(CanFrame& f);

// Conservée pour compatibilité avec sendRasb.ino : ce BMS diffuse ses trames
// automatiquement en continu, aucune trame de requête n'est nécessaire.
void sendQueryFrame();

#endif