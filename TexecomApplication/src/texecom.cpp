// Copyright 2019 Kevin Cooper

#include "texecom.h"
#include "TimeAlarms.h"

TexecomClass::TexecomClass() {}

void TexecomClass::setZoneCallback(void (*zoneCallback)(uint8_t, uint8_t)) {
    this->zoneCallback = zoneCallback;
}

void TexecomClass::setAlarmCallback(void (*alarmCallback)(TexecomClass::ALARM_STATE, uint8_t)) {
    this->alarmCallback = alarmCallback;
}

void TexecomClass::setDebug(bool enabled) {
    savedData.isDebug = enabled;
    EEPROM.put(0, savedData);
}

void TexecomClass::setUDLCode(const char *code) {
    if (strlen(code) == 6) {
        strcpy(savedData.udlCode, code);
        EEPROM.put(0, savedData);
    }
    Log.info("New UDL code = %s", savedData.udlCode);
}

void TexecomClass::requestTimeSync() { Alarm.timerOnce(1, startTimeSync); }

void TexecomClass::startTimeSync() { Texecom.syncTime(); }

void TexecomClass::syncTime() {
    simpleTask = SIMPLE_CHECK_TIME;
    taskStep = SIMPLE_LOGIN_REQUIRED;
    simpleLogin(RESULT_NONE);
}

void TexecomClass::requestZoneSync() { Alarm.timerOnce(1, startZoneSync); }

void TexecomClass::startZoneSync() { Texecom.syncZones(); }

void TexecomClass::syncZones() {
    simpleTask = SIMPLE_ZONE_CHECK;
    taskStep = SIMPLE_LOGIN_REQUIRED;
    simpleLogin(RESULT_NONE);
}

void TexecomClass::requestDisarm(const char *code) {
    if (strlen(userPin) > 0) {
        Log.info("DISARMING: Request already in progress");
        return;
    }

    if (strlen(code) <= 8)
        snprintf(userPin, sizeof(userPin), code);
    else
        return;

    Alarm.timerOnce(1, startDisarm);
}

void TexecomClass::startDisarm() {
    Texecom.disarm();
}

void TexecomClass::disarm() {
    crestronTask = CRESTRON_DISARM;
    taskStep = CRESTRON_START;
    disarmSystem(RESULT_NONE);
}

void TexecomClass::requestArm(const char *code, ARM_TYPE type) {
    if (strlen(userPin) > 0) {
        Log.info("ARMING: Request already in progress");
        return;
    }

    if (strlen(code) <= 8)
        snprintf(userPin, sizeof(userPin), code);
    else
        return;

    armType = type;
    Alarm.timerOnce(1, startArm);
}

void TexecomClass::startArm() {
    Texecom.arm();
}

void TexecomClass::arm() {
    crestronTask = CRESTRON_ARM;
    taskStep = CRESTRON_START;
    armSystem(RESULT_NONE);
}

void TexecomClass::delayCommand(CrestronHelper::CRESTRON_COMMAND command, int delay) {
    delayedCommand = command;
    delayedCommandExecuteTime = millis() + delay;
}

void TexecomClass::updateAlarmState() {
    lastStateChange = millis();
    if (alarmCallback)
        alarmCallback(alarmState, alarmStateFlags);
    
    if (alarmState == TRIGGERED) {
        Alarm.timerOnce(1, startZoneSync);
    }
}

void TexecomClass::decodeZoneState(char *message) {
    uint8_t zone;
    uint8_t state;

    char zoneChar[4];
    memcpy(zoneChar, &message[2], 3);
    zoneChar[3] = '\0';
    zone = atoi(zoneChar) - firstZone;

    state = message[5] - '0';

    if (state == 0) { // Healthy
        zoneStates[zone] &= ~ZONE_ACTIVE;
        zoneStates[zone] &= ~ZONE_TAMPER;
    } else if (state == 1) { // Active
        zoneStates[zone] |= ZONE_ACTIVE;
        zoneStates[zone] &= ~ZONE_TAMPER;
    } else if (state == 2) { // Tamper
        zoneStates[zone] &= ~ZONE_ACTIVE;
        zoneStates[zone] |= ZONE_TAMPER;
    }

    updateZoneState(zone);
}

void TexecomClass::updateZoneState(uint8_t zone) {
    if (zoneCallback)
        zoneCallback(zone+firstZone, zoneStates[zone]);
}

void TexecomClass::processTask(TASK_STEP_RESULT result) {
    if (result == CRESTRON_TASK_TIMEOUT)
        Log.info("processTask: Task timed out");

    if (taskStep == SIMPLE_LOGIN) {
        simpleLogin(result);
    } else if (activeProtocol == SIMPLE) {
        if (simpleTask == SIMPLE_CHECK_TIME) {
            checkTime(result);
        } else if (simpleTask == SIMPLE_ZONE_CHECK) {
            zoneCheck(result);
        }
    } else if (activeProtocol == CRESTRON) {
        if (crestronTask == CRESTRON_ARM) {
            armSystem(result);
        } else if (crestronTask == CRESTRON_DISARM) {
            disarmSystem(result);
        }
    }
}

void TexecomClass::disarmSystem(TASK_STEP_RESULT result) {
    switch (taskStep) {
        case CRESTRON_START :
            disarmStartTime = millis();
            Log.info("DISARMING: Starting disarm process");
            taskStep = CRESTRON_CONFIRM_ARMED;
            crestronHelper.requestArmState();
            break;

        case CRESTRON_CONFIRM_ARMED :
            if (result == CRESTRON_IS_ARMED) {
                Log.info("DISARMING: Confirmed armed. Confirming idle screen");
                taskStep = CRESTRON_CONFIRM_IDLE_SCREEN;
                crestronHelper.requestScreen();
            } else if (result == CRESTRON_IS_DISARMED) {
                Log.info("DISARMING: System already armed. Aborting");
                abortCrestronTask();
            } else {
                abortCrestronTask();
            }
            break;

        case CRESTRON_CONFIRM_IDLE_SCREEN :
            if (
                result == CRESTRON_SCREEN_IDLE ||
                result == CRESTRON_SCREEN_PART_ARMED ||
                result == CRESTRON_SCREEN_FULL_ARMED ||
                result == CRESTRON_SCREEN_AREA_ENTRY) {
                Log.info("DISARMING: Idle screen confirmed. Starting login process");
                taskStep = CRESTRON_LOGIN;
            } else {
                Log.info("DISARMING: Screen is not idle. Aborting");
                abortCrestronTask();
            }
            break;

        case CRESTRON_LOGIN:
            if (result == CRESTRON_LOGIN_COMPLETE) {
                Log.info("DISARMING: Login complete. Awaiting confirmed login");
                taskStep = CRESTRON_LOGIN_WAIT;
            } else {
                Log.info("DISARMING: Login failed. Aborting");
                abortCrestronTask();
            }
            break;

        case CRESTRON_LOGIN_WAIT :
            if (result == CRESTRON_LOGIN_CONFIRMED) {
                if (alarmState != PENDING) {
                    Log.info("DISARMING: Login confirmed. Waiting for Disarm prompt");
                    taskStep = CRESTRON_WAIT_FOR_DISARM_PROMPT;
                    delayCommand(CrestronHelper::COMMAND_SCREEN_STATE, 500);
                } else {
                    Log.info("DISARMING: Login confirmed. Waiting for Disarm confirmation");
                    taskStep = CRESTRON_DISARM_REQUESTED;
                }
            } else {
                Log.info("DISARMING: Login failed to confirm. Aborting");
                abortCrestronTask();
            }
            break;

        case CRESTRON_WAIT_FOR_DISARM_PROMPT :
            if (result == CRESTRON_DISARM_PROMPT) {
                Log.info("DISARMING: Disarm prompt confirmed, disarming");
                if (!savedData.isDebug)
                    texSerial.println("KEYY");  // Yes

                taskStep = CRESTRON_DISARM_REQUESTED;
            } else {
                Log.info("DISARMING: Unexpected result at WAIT_FOR_DISARM_PROMPT. Aborting");
                abortCrestronTask();
            }
            break;

        case CRESTRON_DISARM_REQUESTED :
            if (result == CRESTRON_IS_DISARMED) {
                Log.info("DISARMING: DISARM CONFIRMED");
                crestronTask = CRESTRON_IDLE;
                memset(userPin, 0, sizeof userPin);
                disarmStartTime = 0;
                Alarm.completeTriggeredAlarm();
            } else {
                Log.info("DISARMING: Unexpected result at DISARM_REQUESTED. Aborting");
                abortCrestronTask();
            }
            break;
    }

    if (result == CRESTRON_TASK_TIMEOUT && crestronTask != CRESTRON_IDLE)
        abortCrestronTask();
}

void TexecomClass::armSystem(TASK_STEP_RESULT result) {
    switch (taskStep) {
        case CRESTRON_START :  // Initiate request
            if (armType == FULL_ARM)
                Log.info("ARMING: Starting full arm process");
            else if (armType == NIGHT_ARM)
                Log.info("ARMING: Starting night arm process");
            else
                return;

            armStartTime = millis();
            Log.info("ARMING: Requesting arm state");
            taskStep = CRESTRON_CONFIRM_DISARMED;
            crestronHelper.requestArmState();
            break;

        case CRESTRON_CONFIRM_DISARMED:
            if (result == CRESTRON_IS_DISARMED) {
                Log.info("ARMING: Confirmed disarmed. Confirming idle screen");
                taskStep = CRESTRON_CONFIRM_IDLE_SCREEN;
                crestronHelper.requestScreen();
            } else if (result == CRESTRON_IS_ARMED) {
                Log.info("ARMING: System already armed. Aborting");
                abortCrestronTask();
            } else {
                abortCrestronTask();
            }
            break;

        case CRESTRON_CONFIRM_IDLE_SCREEN :
            if (result == CRESTRON_SCREEN_IDLE) {
                Log.info("ARMING: Idle screen confirmed. Starting login process");
                taskStep = CRESTRON_LOGIN;
            } else {
                Log.info("ARMING: Screen is not idle. Aborting");
                abortCrestronTask();
            }
            break;

        case CRESTRON_LOGIN:
            if (result == CRESTRON_LOGIN_COMPLETE) {
                Log.info("ARMING: Login complete. Awaiting confirmed login");
                taskStep = CRESTRON_LOGIN_WAIT;
            } else {
                Log.info("ARMING: Login failed. Aborting");
                abortCrestronTask();
            }
            break;

        case CRESTRON_LOGIN_WAIT :
            if (result == CRESTRON_LOGIN_CONFIRMED) {
                Log.info("ARMING: Login confirmed. Waiting for Arm prompt");
                taskStep = CRESTRON_WAIT_FOR_ARM_PROMPT;
                delayCommand(CrestronHelper::COMMAND_SCREEN_STATE, 500);
            } else {
                Log.info("ARMING: Login failed to confirm. Aborting");
                abortCrestronTask();
            }
            break;

        case CRESTRON_WAIT_FOR_ARM_PROMPT :
            if (result == CRESTRON_FULL_ARM_PROMPT) {
                if (armType == FULL_ARM) {
                    Log.info("ARMING: Full arm prompt confirmed, completing full arm");
                    if (!savedData.isDebug)
                        texSerial.println("KEYY");  // Yes
                    taskStep = CRESTRON_ARM_REQUESTED;
                } else if (armType == NIGHT_ARM) {
                    Log.info("ARMING: Full arm prompt confirmed, waiting for part arm prompt");
                    taskStep = CRESTRON_WAIT_FOR_PART_ARM_PROMPT;
                    texSerial.println("KEYD");  // Down
                    delayCommand(CrestronHelper::COMMAND_SCREEN_STATE, 500);
                }
            } else {
                Log.info("ARMING: Unexpected result at WAIT_FOR_ARM_PROMPT. Aborting");
                abortCrestronTask();
            }
            break;

        case CRESTRON_WAIT_FOR_PART_ARM_PROMPT :
            if (result == CRESTRON_PART_ARM_PROMPT) {
                Log.info("ARMING: Part arm prompt confirmed, waiting for night arm prompt");
                taskStep = CRESTRON_WAIT_FOR_NIGHT_ARM_PROMPT;
                texSerial.println("KEYY");  // Yes
                delayCommand(CrestronHelper::COMMAND_SCREEN_STATE, 500);
            } else {
                Log.info("ARMING: Unexpected result at WAIT_FOR_PART_ARM_PROMPT. Aborting");
                abortCrestronTask();
            }
            break;

        case CRESTRON_WAIT_FOR_NIGHT_ARM_PROMPT :
            if (result == CRESTRON_NIGHT_ARM_PROMPT) {
                Log.info("ARMING: Night arm prompt confirmed, Completing part arm");
                if (!savedData.isDebug)
                    texSerial.println("KEYY");  // Yes
                taskStep = CRESTRON_ARM_REQUESTED;
            } else {
                Log.info("ARMING: Unexpected result at WAIT_FOR_NIGHT_ARM_PROMPT. Aborting");
                abortCrestronTask();
            }
            break;

        case CRESTRON_ARM_REQUESTED :
            if (result == CRESTRON_IS_ARMING) {
                Log.info("ARMING: ARM CONFIRMED");
                crestronTask = CRESTRON_IDLE;
                memset(userPin, 0, sizeof userPin);
                armStartTime = 0;
                Alarm.completeTriggeredAlarm();
            } else {
                Log.info("ARMING: Unexpected result at ARM_REQUESTED. Aborting");
                abortCrestronTask();
            }
            break;
    }

    if (result == CRESTRON_TASK_TIMEOUT && crestronTask != CRESTRON_IDLE)
        abortCrestronTask();
}

void TexecomClass::simpleLogin(TASK_STEP_RESULT result) {
    switch (taskStep) {
        case SIMPLE_LOGIN_REQUIRED :  // Initiate request
            if (activeProtocol != SIMPLE) {
                Log.info("Starting login process");
                taskStep = SIMPLE_LOGIN;
            }
            break;
        case SIMPLE_LOGIN :
            if (result == SIMPLE_OK) {
                Log.info("Simple login confirmed");
                activeProtocol = SIMPLE;
                simpleProtocolTimeout = millis() + 30000;
                taskStep = SIMPLE_START;
                switch (simpleTask) {
                    case SIMPLE_CHECK_TIME :
                        checkTime(SIMPLE_LOGIN_CONFIRMED);
                        break;
                    case SIMPLE_ZONE_CHECK :
                        zoneCheck(SIMPLE_LOGIN_CONFIRMED);
                        break;
                }
            } else {
                Log.info("Uh oh 1 - %d", result);
            }
            break;
    }
}

void TexecomClass::checkTime(TASK_STEP_RESULT result) {
    switch (taskStep) {
        case SIMPLE_START :
            Log.info("Requesting time");
            simpleHelper.sendSimpleMessage("\\T?/", 4);
            taskStep = SIMPLE_REQUEST_TIME;
            break;
        case SIMPLE_REQUEST_TIME :
            if (result == SIMPLE_TIME_CHECK_OK) {
                Log.info("Time ok, logging out");
                simpleHelper.sendSimpleMessage("\\H/", 3);
                taskStep = SIMPLE_LOGOUT;
            } else if (result == SIMPLE_TIME_CHECK_OUT) {
                Log.info("Time is out, Setting time");
                char setTimeMsg[8];
                setTimeMsg[0] = '\\';
                setTimeMsg[1] = 'T';
                setTimeMsg[2] = Time.day();
                setTimeMsg[3] = Time.month();
                setTimeMsg[4] = Time.year()-2000;
                setTimeMsg[5] = Time.hour();
                setTimeMsg[6] = Time.minute();
                setTimeMsg[7] = '/';
                simpleHelper.sendSimpleMessage(setTimeMsg, 8);
                taskStep = SIMPLE_SEND_TIME;
                break;
            } else {
                Log.info("Uh oh Time 1 - %d", result);
            }
            break;
        case SIMPLE_SEND_TIME :
            Log.info("Time set, logging out");
            simpleHelper.sendSimpleMessage("\\H/", 3);
            taskStep = SIMPLE_LOGOUT;
            break;
        case SIMPLE_LOGOUT :
            if (result == SIMPLE_OK) {
                Log.info("Logout confirmed");
                activeProtocol = CRESTRON;
                simpleTask = SIMPLE_IDLE;
                Alarm.completeTriggeredAlarm();
            } else {
                Log.info("Uh oh Time 2 - %d", result);
            }
            break;
    }
}

void TexecomClass::zoneCheck(TASK_STEP_RESULT result) {
    switch (taskStep) {
        case SIMPLE_START :
            Log.info("Requesting Zone state");
            taskStep = SIMPLE_READ_ZONE_STATE;
            char zoneRequestMessage[5];
            zoneRequestMessage[0] = '\\';
            zoneRequestMessage[1] = 'Z';
            zoneRequestMessage[2] = firstZone-1;
            zoneRequestMessage[3] = zoneCount;
            zoneRequestMessage[4] = '/';
            simpleHelper.sendSimpleMessage(zoneRequestMessage, 5); //  \ Z 8 11 /
            break;
        case SIMPLE_READ_ZONE_STATE :
            Log.info("zoneCheck Logging Out");
            simpleHelper.sendSimpleMessage("\\H/", 3);
            taskStep = SIMPLE_LOGOUT;
            break;
        case SIMPLE_LOGOUT :
            if (result == SIMPLE_OK) {
                Log.info("Logout confirmed");
                activeProtocol = CRESTRON;
                simpleTask = SIMPLE_IDLE;
                Alarm.completeTriggeredAlarm();
            } else {
                Log.info("Uh oh 3 - %d", result);
            }
            break;
    }
}

void TexecomClass::abortCrestronTask() {
    crestronTask = CRESTRON_IDLE;
    texSerial.println("KEYR");
    delayedCommandExecuteTime = 0;
    memset(userPin, 0, sizeof userPin);
    nextPinEntryTime = 0;
    armStartTime = 0;
    disarmStartTime = 0;
    crestronHelper.requestArmState();
    commandAttempts = 0;
    Alarm.completeTriggeredAlarm();
}

bool TexecomClass::processCrestronMessage(char *message, uint8_t messageLength) {

    // Zone state changed
    if (messageLength == 6 &&
        strncmp(message, msgZoneUpdate, strlen(msgZoneUpdate)) == 0) {
        decodeZoneState(message);
        return true;
    // System Armed
    } else if (messageLength >= 6 &&
                strncmp(message, msgArmUpdate, strlen(msgArmUpdate)) == 0) {
        // if (crestronTask != CRESTRON_IDLE) {
            // processTask(CRESTRON_IS_ARMED);
        // }
        return true;
    // System Disarmed
    } else if (messageLength >= 6 &&
                strncmp(message, msgDisarmUpdate, strlen(msgDisarmUpdate)) == 0) {
        // if (crestronTask != CRESTRON_IDLE) {
            // processTask(CRESTRON_IS_DISARMED);
        // }
        return true;
    // Entry while armed
    } else if (messageLength == 6 &&
                strncmp(message, msgEntryUpdate, strlen(msgEntryUpdate)) == 0) {
        return true;
    // System arming
    } else if (messageLength == 6 &&
                strncmp(message, msgArmingUpdate, strlen(msgArmingUpdate)) == 0) {
        return true;
    // Intruder
    } else if (messageLength == 6 &&
                strncmp(message, msgIntruderUpdate, strlen(msgIntruderUpdate)) == 0) {
        return true;
    // User logged in with code or tag
    } else if (messageLength == 6 &&
                (strncmp(message, msgUserPinLogin, strlen(msgUserPinLogin)) == 0 ||
                strncmp(message, msgUserTagLogin, strlen(msgUserTagLogin)) == 0)) {
        int user = message[4] - '0';

        if (user < userCount)
            Log.info("User logged in: %s", users[user]);
        else
            Log.info("User logged in: Outside of user array size");

        if (crestronTask != CRESTRON_IDLE) {
            processTask(CRESTRON_LOGIN_CONFIRMED);
        }
        return true;
    // Reply to ASTATUS request that the system is disarmed
    } else if (messageLength == 5 &&
                strncmp(message, msgReplyDisarmed, strlen(msgReplyDisarmed)) == 0) {
        if (crestronTask != CRESTRON_IDLE) {
            processTask(CRESTRON_IS_DISARMED);
        }
        return true;
    // Reply to ASTATUS request that the system is armed
    } else if (messageLength == 5 &&
                strncmp(message, msgReplyArmed, strlen(msgReplyArmed)) == 0) {
        if (crestronTask != CRESTRON_IDLE) {
            processTask(CRESTRON_IS_ARMED);
        }
        return true;
    } else if (
            (messageLength >= strlen(msgScreenArmedPart) &&
                strncmp(message, msgScreenArmedPart, strlen(msgScreenArmedPart)) == 0) ||
            (messageLength >= strlen(msgScreenArmedNight) &&
                strncmp(message, msgScreenArmedNight, strlen(msgScreenArmedNight)) == 0) ||
            (messageLength >= strlen(msgScreenIdlePartArmed) &&
                strncmp(message, msgScreenIdlePartArmed, strlen(msgScreenIdlePartArmed)) == 0)) {
        if (crestronTask != CRESTRON_IDLE) {
            processTask(CRESTRON_SCREEN_PART_ARMED);
        }
        return true;
    } else if (messageLength >= strlen(msgScreenArmedFull) &&
                strncmp(message, msgScreenArmedFull, strlen(msgScreenArmedFull)) == 0) {
        if (crestronTask != CRESTRON_IDLE) {
            processTask(CRESTRON_SCREEN_FULL_ARMED);
        }
        return true;
    } else if (messageLength >= strlen(msgScreenIdle) &&
                strncmp(message, msgScreenIdle, strlen(msgScreenIdle)) == 0) {
        if (crestronTask != CRESTRON_IDLE) {
            processTask(CRESTRON_SCREEN_IDLE);
        }
        return true;
    // Shown directly after user logs in
    } else if (messageLength > strlen(msgWelcomeBack) &&
                strncmp(message, msgWelcomeBack, strlen(msgWelcomeBack)) == 0) {
        if (taskStep == CRESTRON_WAIT_FOR_DISARM_PROMPT ||
                taskStep == CRESTRON_WAIT_FOR_ARM_PROMPT) {
            delayCommand(CrestronHelper::COMMAND_SCREEN_STATE, 500);
        }
        return true;
    // Shown shortly after user logs in
    } else if (messageLength >= strlen(msgScreenQuestionArm) &&
                strncmp(message, msgScreenQuestionArm, strlen(msgScreenQuestionArm)) == 0) {
        if (crestronTask != CRESTRON_IDLE) {
            processTask(CRESTRON_FULL_ARM_PROMPT);
        }
        return true;
    } else if (messageLength >= strlen(msgScreenQuestionPartArm) &&
                strncmp(message, msgScreenQuestionPartArm, strlen(msgScreenQuestionPartArm)) == 0) {
        if (crestronTask != CRESTRON_IDLE) {
            processTask(CRESTRON_PART_ARM_PROMPT);
        }
        return true;
    } else if (messageLength >= strlen(msgScreenQuestionNightArm) &&
                strncmp(message, msgScreenQuestionNightArm, strlen(msgScreenQuestionNightArm)) == 0) {
        if (crestronTask != CRESTRON_IDLE) {
            processTask(CRESTRON_NIGHT_ARM_PROMPT);
        }
        return true;
    } else if (messageLength >= strlen(msgScreenQuestionDisarm) &&
                strncmp(message, msgScreenQuestionDisarm, strlen(msgScreenQuestionDisarm)) == 0) {
        if (crestronTask != CRESTRON_IDLE) {
            processTask(CRESTRON_DISARM_PROMPT);
        }
        return true;
    } else if (messageLength >= strlen(msgScreenAreainEntry) &&
                strncmp(message, msgScreenAreainEntry, strlen(msgScreenAreainEntry)) == 0) {
        if (crestronTask != CRESTRON_IDLE) {
            processTask(CRESTRON_SCREEN_AREA_ENTRY);
        }
        return true;
    } else if (messageLength >= strlen(msgScreenAreainExit) &&
        strncmp(message, msgScreenAreainExit, strlen(msgScreenAreainExit)) == 0) {
        if (crestronTask != CRESTRON_IDLE) {
            processTask(CRESTRON_SCREEN_AREA_EXIT);
        }
        return true;
    }
    return false;
}

bool TexecomClass::processSimpleMessage(char *message, uint8_t messageLength) {

    if (strncmp(message, "OK", 2) == 0) {
        if (simpleTask != SIMPLE_IDLE) {
            processTask(SIMPLE_OK);
        }
        return true;
    } else if (strncmp(message, "ERROR", 5) == 0) {
        if (simpleTask != SIMPLE_IDLE) {
            processTask(SIMPLE_OK);
        }
        return true;
    } else if (taskStep == SIMPLE_REQUEST_TIME && messageLength == 5) {
        bool timeInSync = simpleHelper.processReceivedTime(message);
        if (timeInSync)
            processTask(SIMPLE_TIME_CHECK_OK);
        else
            processTask(SIMPLE_TIME_CHECK_OUT);
        return true;
    } else if (taskStep == SIMPLE_READ_ZONE_STATE) {
        simpleHelper.processReceivedZoneData(message, messageLength, zoneStates);

        for (uint8_t i = 0; i < zoneCount; i++)
            updateZoneState(i);

        processTask(SIMPLE_OK);
        return true;
    }
    Log.info ("Unknown simple message rejected");
    processTask(UNKNOWN_MESSAGE);
    return false;
}

void TexecomClass::checkDigiOutputs() {
    bool changeDetected = false;

    bool _state = digitalRead(pinFullArmed);

    if (_state != statePinFullArmed) {
        changeDetected = true;
        statePinFullArmed = _state;
        if (_state == LOW) {
            alarmState = ARMED_AWAY;
        } else {
            alarmState = DISARMED;
            if (crestronTask != CRESTRON_IDLE) {
                processTask(CRESTRON_IS_DISARMED);
            }
        }
    }

    _state = digitalRead(pinPartArmed);

    if (_state != statePinPartArmed) {
        changeDetected = true;
        statePinPartArmed = _state;
        if (_state == LOW) {
            alarmState = ARMED_HOME;
        } else {
            alarmState = DISARMED;
            if (crestronTask != CRESTRON_IDLE) {
                processTask(CRESTRON_IS_DISARMED);
            }
        }
    }

    _state = digitalRead(pinEntry);

    if (_state != statePinEntry) {
        changeDetected = true;
        statePinEntry = _state;
        if (_state == LOW) {
            alarmState = PENDING;
        }
    }

    _state = digitalRead(pinExiting);

    if (_state != statePinExiting) {
        changeDetected = true;
        statePinExiting = _state;
        if (_state == LOW) {
            alarmState = PENDING;
            if (crestronTask != CRESTRON_IDLE) {
               processTask(CRESTRON_IS_ARMING);
            }
        }
    }

    _state = digitalRead(pinTriggered);

    if (_state != statePinTriggered) {
        changeDetected = true;
        statePinTriggered = _state;
        if (_state == LOW) {
            alarmState = TRIGGERED;
        }
    }

    _state = digitalRead(pinAreaReady);

    if (_state != statePinAreaReady) {
        changeDetected = true;
        statePinAreaReady = _state;

        if (statePinAreaReady == LOW) {
            alarmStateFlags |= ALARM_READY;
        } else {
            alarmStateFlags &= ~ALARM_READY;
        }
    }

    _state = digitalRead(pinFaultPresent);

    if (_state != statePinFaultPresent) {
        changeDetected = true;
        statePinFaultPresent = _state;

        if (statePinFaultPresent == LOW) {
            alarmStateFlags |= ALARM_FAULT;
            Log.error("Alarm is reporting a fault");
        } else {
            alarmStateFlags &= ~ALARM_FAULT;
            Log.info("Alarm fault cleared");
        }
    }

    _state = digitalRead(pinArmFailed);

    if (_state != statePinArmFailed) {
        changeDetected = true;
        statePinArmFailed = _state;

        if (statePinArmFailed == LOW) {
            alarmStateFlags |= ALARM_ARM_FAILED;
            Log.error("Alarm failed to arm");
        } else {
            alarmStateFlags &= ~ALARM_ARM_FAILED;
            Log.error("Alarm arm failure cleared");
        }
    }

    if (changeDetected)
        updateAlarmState();

}

void TexecomClass::setup() {
    texSerial.begin(19200, SERIAL_8N2);  // open serial communications

    pinMode(pinFullArmed, INPUT);
    pinMode(pinPartArmed, INPUT);
    pinMode(pinEntry, INPUT);
    pinMode(pinExiting, INPUT);
    pinMode(pinTriggered, INPUT);
    pinMode(pinArmFailed, INPUT);
    pinMode(pinFaultPresent, INPUT);
    pinMode(pinAreaReady, INPUT);

    EEPROM.get(0, savedData);

    if (savedData.isDebug)
        Log.info("UDL code = %s", savedData.udlCode);

    Alarm.timerRepeat(180, Texecom.startZoneSync);
    Alarm.alarmRepeat(3, 0, 0, Texecom.startTimeSync);

    checkDigiOutputs();
}

void TexecomClass::loop() {
    bool messageReady = false;
    bool messageComplete = true;
    uint8_t messageLength = 0;

    // Read incoming serial data if available and copy to TCP port
    while (texSerial.available() > 0) {
        int incomingByte = texSerial.read();
        // Log.info("S %d", incomingByte);
        if (bufferPosition == 0)
            messageStart = millis();

        // Will never happen but just in case
        if (bufferPosition >= maxMessageSize) {
            memcpy(message, buffer, bufferPosition);
            message[bufferPosition] = '\0';
            messageReady = true;
            messageLength = bufferPosition;
            bufferPosition = 0;
            break;
        // 13+10 (CRLF) signifies the end of message
        } else if (bufferPosition > 2 &&
                   incomingByte == 10 &&
                   buffer[bufferPosition-1] == 13) {
            
            if (activeProtocol == SIMPLE || taskStep == SIMPLE_LOGIN) {
                if (simpleHelper.checkSimpleChecksum(buffer, bufferPosition-2)) {
                    Log.info("Checksum valid");
                    buffer[bufferPosition-2] = '\0'; // Overwrite the checksum
                    memcpy(message, buffer, bufferPosition-1);
                    messageReady = true;
                    messageLength = bufferPosition-2;
                } else {
                    buffer[bufferPosition++] = incomingByte;
                }
            } else {
                buffer[bufferPosition-1] = '\0'; // Replace 13 with termination
                memcpy(message, buffer, bufferPosition);
                messageReady = true;
                messageLength = bufferPosition-1;
            }

            if (messageReady) {
                bufferPosition = 0;
                screenRequestRetryCount = 0;
                break;
            }

        } else {
            buffer[bufferPosition++] = incomingByte;
        }
    } // while (texSerial.available() > 0)

    if (bufferPosition > 0 && millis() > (messageStart+50)) {
        Log.info("Message failed to receive within 50ms");
        memcpy(message, buffer, bufferPosition);
        message[bufferPosition] = '\0';
        messageReady = true;
        messageLength = bufferPosition;
        messageComplete = false;
        bufferPosition = 0;
    }

    if (messageReady) {
        Log.info(message);

        bool processedSuccessfully = false;
        if (activeProtocol == SIMPLE || taskStep == SIMPLE_LOGIN) {
            processedSuccessfully = processSimpleMessage(message, messageLength);
        } else if (activeProtocol == CRESTRON) {
            processedSuccessfully = processCrestronMessage(message, messageLength);
        }

        if (!processedSuccessfully) {
            if (message[0] == '"') {
                Log.info(String::format("Unknown Crestron command - %s", message));
            } else {
                Log.info("Unknown non-Crestron command - %s", message);
                for (uint8_t i = 0; i < messageLength; i++) {
                    Log.info("%d\n", message[i]);
                }
            }

            if (crestronTask != CRESTRON_IDLE && !messageComplete) {
                if (screenRequestRetryCount++ < 3) {
                    if (taskStep == CRESTRON_CONFIRM_ARMED || taskStep == CRESTRON_CONFIRM_DISARMED) {
                        Log.info("Retrying arm state request");
                        crestronHelper.requestArmState();
                    } else if (taskStep == CRESTRON_CONFIRM_IDLE_SCREEN ||
                                taskStep == CRESTRON_WAIT_FOR_ARM_PROMPT ||
                                taskStep == CRESTRON_WAIT_FOR_DISARM_PROMPT ||
                                taskStep == CRESTRON_WAIT_FOR_PART_ARM_PROMPT ||
                                taskStep == CRESTRON_WAIT_FOR_NIGHT_ARM_PROMPT) {
                        Log.info("Retrying screen request");
                        crestronHelper.requestScreen();
                    } else {
                        Log.info("Retry count exceeded");
                    }
                }
            } else {
                processTask(UNKNOWN_MESSAGE);
            }
        }
    }

    // HANDLE CRESTON LOGIN VIA KEYPRESS ON VIRTUAL SCREEN
    if (taskStep == CRESTRON_LOGIN && millis() > nextPinEntryTime) {
        texSerial.print("KEY");
        texSerial.println(userPin[loginPinPosition++]);

        if (loginPinPosition >= strlen(userPin)) {
            loginPinPosition = 0;
            nextPinEntryTime = 0;
            processTask(CRESTRON_LOGIN_COMPLETE);
        } else {
            nextPinEntryTime = millis() + PIN_ENTRY_DELAY;
        }
    }

    // THERE IS NO NOTIFICATION IF AN INCORRECT USER CODE IS ENTERED
    // WE HAVE TO RELY ON A TIMEOUT
    if (crestronTask == CRESTRON_IDLE && alarmState == ARMING &&
        millis() > (lastStateChange + armingTimeout)) {
        Log.info("Arming state timed out. Requesting arm state");
        lastStateChange = millis();
        crestronHelper.requestArmState();
    }

    // USED TO EXECUTE DELAYED COMMANDS
    if (delayedCommandExecuteTime > 0 &&
        millis() > delayedCommandExecuteTime) {
        crestronHelper.request(delayedCommand);
        delayedCommandExecuteTime = 0;
    }

    // DETECT AN ARMING FAILURE VIA A TIMEOUT
    if (armStartTime != 0 &&
        millis() > (armStartTime + armTimeout))
        processTask(CRESTRON_TASK_TIMEOUT);

    // DETECT A DISARM FAILURE VIA A TIMEOUT
    if (disarmStartTime != 0 &&
        millis() > (disarmStartTime + disarmTimeout)) {
        processTask(CRESTRON_TASK_TIMEOUT);
    }

    // SWITCH TO SIMPLE PROTOCOL BY SENDING
    // THE UDL CODE AS \W1234/ TWICE
    if (simpleTask != SIMPLE_IDLE && taskStep == SIMPLE_LOGIN && millis() > (simpleCommandLastSent+500)) {
        // TODO: This needs to error if there are too many login attempts
        Log.info("Performing simple login");
        simpleCommandLastSent = millis();

        char loginData[9];
        loginData[0] = '\\';
        loginData[1] = 'W';
        for (int i = 0; i < 6; i++)
            loginData[2+i] = savedData.udlCode[i];
        loginData[8] = '/';

        simpleHelper.sendSimpleMessage(loginData, 9);
    }

    // Auto-logout of the Simple Protocol. Should never be required.
    if (millis() > simpleProtocolTimeout && activeProtocol == SIMPLE) {
        simpleProtocolTimeout = millis() + 10000;
        if (taskStep != SIMPLE_LOGOUT) {
            taskStep = SIMPLE_LOGOUT;
            simpleHelper.sendSimpleMessage("\\H/", 3);
            Log.info("Simple Protocol timeout");
        } else {
            activeProtocol = CRESTRON;
            simpleTask = SIMPLE_IDLE;
            Log.info("Simple logout failed and was forced");
        }
    }

    checkDigiOutputs();
    Alarm.loop();
}

// make one instance for the user to use
TexecomClass Texecom = TexecomClass();