// Copyright 2020 Kevin Cooper

#ifndef __SIMPLEHELPER_H_
#define __SIMPLEHELPER_H_

#include "Particle.h"

#define texSerial Serial1

class SimpleHelper {
 public:
    SimpleHelper();
    bool checkSimpleChecksum(const char *text, uint8_t length);
    void sendSimpleMessage(const char *text, uint8_t length);
    bool processReceivedTime(const char *message);
    void processReceivedZoneData(const char *message, uint8_t messageLength);
 private:
};

 #endif  //__SIMPLEHELPER_H_