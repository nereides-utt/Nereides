#include <Wire.h>
#include <ESP32-TWAI-CAN.hpp>
#include <OneWire.h>
#include <DallasTemperature.h>

#define CAN_TX 22
#define CAN_RX 21
//configuration sonde température globale 
#define TEMP_SENSOR_PIN 4
#define CAN_ID_TEMP_SENSOR 0x013
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature ds(&oneWire);

CanFrame rxFrame, txFrame;




void sendTemperatureViaCAN(float temperature) {
    // Vérifier si la température est valide (DS18B20 retourne -127 en cas d'erreur)
    if (temperature == -127.0 ) {
        Serial.println("Erreur lecture température - trame CAN non envoyée");
        return;
    }
    
    txFrame.identifier = CAN_ID_TEMP_SENSOR;
    txFrame.extd = 0;  // Trame standard (11 bits)
    txFrame.data_length_code = 4;  // 4 bytes de données
    
    // Encoder la température en centièmes de degré (16 bits)
    int16_t tempCentidegreees = (int16_t)(temperature * 100);
    
    // Remplir les données (little-endian)
    txFrame.data[0] = tempCentidegreees & 0xFF;         // LSB
    txFrame.data[1] = (tempCentidegreees >> 8) & 0xFF;  // MSB
    txFrame.data[2] = 0x00;  // Réservé
    txFrame.data[3] = 0x00;  // Réservé
    
    // Envoyer la trame CAN
    if (ESP32Can.writeFrame(txFrame)) {
        Serial.printf("Température envoyée via CAN: %.2f°C (ID: 0x%03X)\n", 
                      temperature, CAN_ID_TEMP_SENSOR);
        
        // Debug: afficher les bytes envoyés
        Serial.printf("Données CAN: %02X %02X %02X %02X\n", 
                      txFrame.data[0], txFrame.data[1], 
                      txFrame.data[2], txFrame.data[3]);
    } else {
        Serial.println("Erreur: Échec envoi trame CAN température");
    }
}

void readAndSendTemperature() {
    ds.requestTemperatures(); //lecture
    float temperature = ds.getTempCByIndex(0); //lecture
    
    Serial.printf("Température lue: %.2f°C\n", temperature);
    
    // Envoyer via CAN
    sendTemperatureViaCAN(temperature);
}




void setup() {
  Serial.begin(115200);


  ds.begin(); //init sonde
 
  //Initialisation CAN
  ESP32Can.setPins(CAN_TX, CAN_RX);
  ESP32Can.setRxQueueSize(5);
  ESP32Can.setTxQueueSize(5);
  ESP32Can.setSpeed(ESP32Can.convertSpeed(250));

  if(ESP32Can.begin()) {
    Serial.println("CAN bus started!");
  } else {
    Serial.println("CAN bus failed!");
  }
}

void loop() {
 
    readAndSendTemperature();
    


  delay(1000);
}
