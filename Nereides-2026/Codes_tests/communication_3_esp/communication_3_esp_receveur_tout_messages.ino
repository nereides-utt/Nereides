#include <driver/twai.h>  // Librairie native ESP32 pour CAN (TWAI)

void setup() {
  Serial.begin(115200);

  // Configuration du bus CAN
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_5,GPIO_NUM_4,TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); // vitesse 500 kbps
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); // accepte tous les messages

  // Installation du driver CAN
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("Driver CAN installé");
  } else {
    Serial.println("Erreur installation CAN");
    return;
  }

  // Activation du driver
  if (twai_start() == ESP_OK) {
    Serial.println("Slave prêt (CAN démarré)");
  } else {
    Serial.println("Erreur démarrage CAN");
  }
}

void loop() {
  twai_message_t message;
  if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
    Serial.print("Message reçu ID: 0x");
    Serial.print(message.identifier, HEX);
    Serial.print(" Data: ");
    for (int i = 0; i < message.data_length_code; i++) {
      Serial.print((char)message.data[i]);
    }
    Serial.println();
  }
}
