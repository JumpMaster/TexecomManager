#include "crestonhelper.h"

CrestronHelper::CrestronHelper() {}

void CrestronHelper::request(CRESTRON_COMMAND command) {
    if (command == COMMAND_SCREEN_STATE)
        texSerial.println("LSTATUS");
    else if (command == COMMAND_ARMED_STATE) {
        texSerial.println("ASTATUS");
    }
    // lastCommandTime = millis();
}

void CrestronHelper::requestArmState() {
    request(COMMAND_ARMED_STATE);
}

void CrestronHelper::requestScreen() {
    request(COMMAND_SCREEN_STATE);
}