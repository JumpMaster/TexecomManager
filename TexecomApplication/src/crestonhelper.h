// Copyright 2020 Kevin Cooper

#ifndef __CRESTRONHELPER_H_
#define __CRESTRONHELPER_H_

#include "Particle.h"

#define texSerial Serial1

class CrestronHelper {
 public:
    typedef enum {
        COMMAND_ARMED_STATE = 0,
        COMMAND_SCREEN_STATE = 1
    } CRESTRON_COMMAND;   

 public:
    CrestronHelper();
    void request(CRESTRON_COMMAND command);
    void requestArmState();
    void requestScreen();
 private:
};

 #endif  //__CRESTRONHELPER_H_