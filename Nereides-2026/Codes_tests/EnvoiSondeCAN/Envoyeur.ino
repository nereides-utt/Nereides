#include <driver/twai.h>        // Librairie native CAN (TWAI)
#include <OneWire.h>            // Bus 1-Wire
#include <DallasTemperature.h>  // Gestion DS18B20

// --- Brochage ---
#define ONE_WIRE_BUS 4   // GPIO pour le DS18B20
#define CAN_TX GPIO_NUM_22
#define CAN_RX GPIO_NUM_21

// --- Initialisation des objets ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// --- Configuration du CAN (TWAI) ---
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX, CAN_RX, TWAI_MODE_NORMAL);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

void setup() {
  Serial.begin(115200);
  delay(500);
  
  // Démarrage du capteur
  sensors.begin();
  Serial.println("DS18B20 prêt.");

  // Initialisation du driver CAN
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("Driver CAN installé avec succès");
  } else {
    Serial.println("Erreur : installation driver CAN");
    return;
  }

  // Démarrage du bus CAN
  if (twai_start() == ESP_OK) {
    Serial.println("Bus CAN démarré !");
  } else {
    Serial.println("Erreur : démarrage CAN");
    return;
  }

  Serial.println("Slave 2 prêt !");
}

void loop() {
  // --- Lecture température ---
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);
  Serial.print("Température lue : ");
  Serial.print(temperature);
  Serial.println(" °C");

  // --- Préparation message CAN ---
  twai_message_t tx_msg;
  tx_msg.identifier = 0x09;      // même ID que ton code
  tx_msg.extd = 0;               // trame standard
  tx_msg.rtr = 0;                // data frame
  tx_msg.data_length_code = 4;   // 4 octets pour un float

  // Conversion float → 4 octets
  memcpy(tx_msg.data, &temperature, sizeof(float));

  // --- Envoi message sur le bus ---
  if (twai_transmit(&tx_msg, pdMS_TO_TICKS(100)) == ESP_OK) {
    Serial.println("Trame CAN envoyée !");
  } else {
    Serial.println("Erreur d’envoi CAN");
  }

  // --- Lecture éventuelle d’un message entrant ---
  twai_message_t rx_msg;
  if (twai_receive(&rx_msg, pdMS_TO_TICKS(3)) == ESP_OK) {
    if (rx_msg.identifier == 0x09 && !(rx_msg.rtr)) {
      Serial.print("Slave 2 a reçu : ");
      for (int i = 0; i < rx_msg.data_length_code; i++) {
        Serial.print((char)rx_msg.data[i]);
      }
      Serial.println();
    }
  }

  delay(1000);  // envoi toutes les secondes
}
