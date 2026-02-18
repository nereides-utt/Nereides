#pragma once
#ifndef GPSCLASS_HPP
#define GPSCLASS_HPP

#include <stdint.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <ESP32-TWAI-CAN.hpp>
#include <math.h>
#include "DataCollector.hpp"

class GPSClass : public Sensor {

  private:
    doubleToBytes_t latitude; //lattitude (stockée sur un nombre décimal de 64 bits)
    doubleToBytes_t longitude; //longitude (stockée sur un nombre décimal de 64 bits)
    uint16toBytes_t speed; //vitesse en NOEUDS
    uint32toBytes_t satellites;

    TinyGPSPlus gps; //
    HardwareSerial SerialGPS;


  public:
    GPSClass(uint8_t uart_n, unsigned long uart_baud, int8_t rxPin, int8_t txPin, uint32_t CANIdentifier, bool printing = false); //construteur
    ~GPSClass(); // destructeur

    void update() override;
    void sendDataViaCan() const override;
    void decodeCanFrame() override;

};

#endif