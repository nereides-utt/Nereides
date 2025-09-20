#include <Arduino.h>
#include "driver/twai.h"
// Broches CAN (adapter selon ton montage : GPIO 5 = TX, GPIO 4 = RX)
#define CAN_TX GPIO_NUM_5
#define CAN_RX GPIO_NUM_4
void setup() {
  Serial.begin(115200);
  delay(1000);
  // --- Configuration du bus CAN (TWAI) ---
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX, CAN_RX, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  // --- Installation du driver ---
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("Driver TWAI installé !");
  } else {
    Serial.println("Erreur : installation driver TWAI");
    while (1);
  }
  // --- Démarrage du bus CAN ---
  if (twai_start() == ESP_OK) {
    Serial.println("CAN Master Ready :coche_blanche:");
  } else {
    Serial.println("Erreur : impossible de démarrer le CAN");
    while (1);
  }
}
void loop() {
  // --- Création de la trame pour esclave 1 ---
  twai_message_t tx_msg = {};
  tx_msg.identifier = 0x08;       // ID esclave 1
  tx_msg.extd = 0;                // Trame standard (11 bits)
  tx_msg.data_length_code = 4;
  tx_msg.data[0] = 'H';
  tx_msg.data[1] = 'E';
  tx_msg.data[2] = 'L';
  tx_msg.data[3] = 'O';
  if (twai_transmit(&tx_msg, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.println(":coche_blanche: Sent HELLO to Slave 1");
  } else {
    Serial.println(":x: Erreur envoi vers Slave 1");
  }
  delay(1000);
  // --- Envoi à l’esclave 2 ---
  tx_msg.identifier = 0x09; // ID esclave 2
  if (twai_transmit(&tx_msg, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.println(":coche_blanche: Sent HELLO to Slave 2");
  } else {
    Serial.println(":x: Erreur envoi vers Slave 2");
  }
  delay(2000);
}
