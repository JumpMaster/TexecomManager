// Copyright 2020 Kevin Cooper

#ifndef __SIMPLEHELPER_H_
#define __SIMPLEHELPER_H_

#include "Particle.h"

#define texSerial Serial1

class SimpleHelper {
 public:
   struct ZONE_STATE {
      bool isActive;
      bool isTamper;
      bool isAlarmed;
      bool hasFault;
   };

 public:
    SimpleHelper();
    bool checkSimpleChecksum(const char *text, uint8_t length);
    void sendSimpleMessage(const char *text, uint8_t length);
    bool processReceivedTime(const char *message);
    bool processReceivedZoneData(const char *message, uint8_t messageLength, uint8_t *zoneState);
    void simpleLogout();
 private:
};

 #endif  //__SIMPLEHELPER_H_