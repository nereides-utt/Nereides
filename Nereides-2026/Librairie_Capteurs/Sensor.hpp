#pragma once
#ifndef SENSOR_HPP
#define SENSOR_HPP
#include <ESP32-TWAI-CAN.hpp>

typedef union {
  double val;
  uint8_t bytes[8];
} doubleToBytes_t; //Permet de convertir en 8 octets un double.

typedef union {
  uint16_t val;
  uint8_t bytes[2];
} uint16toBytes_t; //Permet de séparer les 2 octets d'un uint16_t

typedef union {
  uint32_t val;
  uint8_t bytes[4];
} uint32toBytes_t; //Permet de séparer les 4 octets d'un uint32_t

class Sensor {
    public:
        uint32_t CANidentifier; //CAN frame identifier used to transfer data collected by the sensor.
        bool printing = false; //Priting, true = state of the sensor will be displaed in the Serial Monitor; false = nothing will be displayed.
        bool receptor = false; //Boolean true = this instance is used to recept data from CAN bus; false = this instance is used to collect data.

        Sensor(uint32_t CANidentifier, bool printing) : CANidentifier(CANidentifier), printing(printing) {} //Constructor;

        virtual void sendDataViaCan() const = 0; // Method used to transfer data into the CAN bus.
        virtual void decodeCanFrame(const CanFrame &frame) = 0; // Method to decode the CAN frame. 
};

#endif