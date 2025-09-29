#include <OneWire.h>
#include <DallasTemperature.h>

// GPIO auquel tu as branché DATA
#define ONE_WIRE_BUS 4  

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup(void) {
  Serial.begin(115200);
  sensors.begin();
}

void loop(void) {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  Serial.print("Température: ");
  Serial.print(tempC);
  Serial.println(" °C");
  delay(1000);
}
