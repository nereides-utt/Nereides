#include <driver/twai.h>  // Librairie native CAN (TWAI) de l’ESP32


void setup() {
  Serial.begin(115200);


  // Configuration des broches et vitesse du bus
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_5, GPIO_NUM_4, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();


  // Initialisation du driver TWAI
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("Driver installé avec succès");
  } else {
    Serial.println("Échec installation driver");
    return;
  }


  // Démarrage du bus
  if (twai_start() == ESP_OK) {
    Serial.println("CAN (TWAI) démarré !");
  } else {
    Serial.println("Échec démarrage CAN");
  }


  Serial.println("Slave 2 Ready");
}


void loop() {
  twai_message_t rx_msg;


  // Réception avec timeout de 3 ms
  if (twai_receive(&rx_msg, pdMS_TO_TICKS(3)) == ESP_OK) {
    if (rx_msg.identifier == 0x09 && !(rx_msg.rtr)) {  // Seulement si Data Frame avec ID = 0x09
      Serial.print("Slave 2 got: ");
      for (int i = 0; i < rx_msg.data_length_code; i++) {
        Serial.print((char)rx_msg.data[i]);
      }
      Serial.println();
    }
  }
}

