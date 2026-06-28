#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      // Affiche le nom et l'adresse MAC de chaque appareil détecté
      Serial.print("Appareil trouvé : ");
      Serial.print(advertisedDevice.getName().c_str());
      Serial.print(" | Adresse : ");
      Serial.println(advertisedDevice.getAddress().toString().c_str());

      // Si l'appareil diffuse son UUID de Service, on l'affiche
      if (advertisedDevice.haveServiceUUID()) {
        Serial.print("  -> UUID de Service détecté : ");
        Serial.println(advertisedDevice.getServiceUUID().toString().c_str());
      }
      Serial.println("------------------------------------------");
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Lancement du scanner d'UUIDs...");

  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void loop() {
  // Scanne toutes les 10 secondes
  BLEDevice::getScan()->start(5, false);
  delay(5000);
}