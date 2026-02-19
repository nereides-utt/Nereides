#pragma once
#ifndef SONDETEMP_HPP
#define SONDETEMP_HPP
#include "DataCollector.hpp"
#include <Wire.h>
#include <ESP32-TWAI-CAN.hpp>
#include <OneWire.h>
#include <DallasTemperature.h>

class SondeTemp : Sensor {
    private:
      OneWire oneWire;
      DallasTemperature ds;

    public:
      uint16toBytes_t temperature;
      SondeTemp();

      void update();
      void sendDataViaCan() const;
      void decodeCanFrame(const CanFrame &frame);

};


#endif
