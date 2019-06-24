// Copyright 2019 Kevin Cooper

#include "texecom.h"

Texecom::Texecom(void (*callback)(CALLBACK_TYPE, uint8_t, uint8_t)) {
    texSerial.begin(19200, SERIAL_8N1);  // open serial communications
    this->callback = callback;
    userCount = sizeof(users) / sizeof(char*);
}

void Texecom::request(COMMAND command) {
    texSerial.println(commandStrings[command]);
    lastCommandTime = millis();
}

void Texecom::disarm(const char *code) {
    if (currentTask != IDLE) {
        Log.info("DISARMING: Request already in progress");
        return;
    }
    if (strlen(code) <= 8)
        snprintf(userPin, sizeof(userPin), code);
    else
        return;
    task = DISARM;
    currentTask = START;
    disarmSystem(RESULT_NONE);
}

void Texecom::arm(const char *code, ARM_TYPE type) {
    if (currentTask != IDLE) {
        Log.info("ARMING: Request already in progress");
        return;
    }
    if (strlen(code) <= 8)
        snprintf(userPin, sizeof(userPin), code);
    else
        return;
    armType = type;
    task = ARM;
    currentTask = START;
    armSystem(RESULT_NONE);
}

void Texecom::requestArmState() {
    lastStateCheck = millis();
    request(COMMAND_ARMED_STATE);
}

void Texecom::requestScreen() {
    request(COMMAND_SCREEN_STATE);
}

void Texecom::delayCommand(COMMAND command, int delay) {
    delayedCommand = command;
    delayedCommandExecuteTime = millis() + delay;
}

void Texecom::updateAlarmState(ALARM_STATE state) {
    if (alarmState == state)
        return;

    if (state == ARMED && (alarmState == ARMED_HOME || alarmState == ARMED_AWAY))
        return;

    if (state == ARMED)
        delayCommand(COMMAND_SCREEN_STATE, 1000);

    if (state == DISARMED)
        triggeredZone = 0;

    if (state == TRIGGERED && triggeredZone != 0)
            callback(ALARM_TRIGGERED, triggeredZone, 0);

    lastStateChange = millis();
    alarmState = state;
    callback(ALARM_STATE_CHANGE, 0, alarmState);
}

void Texecom::updateZoneState(char *message) {
    uint8_t zone;
    uint8_t state;

    char zoneChar[4];
    memcpy(zoneChar, &message[2], 3);
    zoneChar[3] = '\0';
    zone = atoi(zoneChar);

    if ((alarmState == PENDING || alarmState == TRIGGERED) &&
        triggeredZone == 0) {
        triggeredZone = zone;

        if (alarmState == TRIGGERED)
            callback(ALARM_TRIGGERED, 0, triggeredZone);
    }

    state = message[5] - '0';

    callback(ZONE_STATE_CHANGE, zone, state);
}

void Texecom::processTask(RESULT result) {
    if (result == TASK_TIMEOUT)
        Log.info("processTask: Task timed out");

    if (task == ARM) {
        armSystem(result);
    } else if (task == DISARM) {
        disarmSystem(result);
    }
}

void Texecom::disarmSystem(RESULT result) {
    switch (currentTask) {
        case START :
            disarmStartTime = millis();
            Log.info("DISARMING: Starting disarm process");
            currentTask = CONFIRM_ARMED;
            requestArmState();
            break;

        case CONFIRM_ARMED :
            if (result == IS_ARMED) {
                Log.info("DISARMING: Confirmed armed. Confirming idle screen");
                currentTask = CONFIRM_IDLE_SCREEN;
                requestScreen();
            } else if (result == IS_DISARMED) {
                Log.info("DISARMING: System already armed. Aborting");
                abortTask();
            } else {
                abortTask();
            }
            break;

        case CONFIRM_IDLE_SCREEN :
            if (
                result == SCREEN_IDLE ||
                result == SCREEN_PART_ARMED ||
                result == SCREEN_FULL_ARMED ||
                result == SCREEN_AREA_ENTRY) {
                Log.info("DISARMING: Idle screen confirmed. Starting login process");
                performLogin = true;
                currentTask = LOGIN;
            } else {
                Log.info("DISARMING: Screen is not idle. Aborting");
                abortTask();
            }
            break;

        case LOGIN:
            if (result == LOGIN_COMPLETE) {
                Log.info("DISARMING: Login complete. Awaiting confirmed login");
                currentTask = LOGIN_WAIT;
            } else {
                Log.info("DISARMING: Login failed. Aborting");
                abortTask();
            }
            break;

        case LOGIN_WAIT :
            if (result == LOGIN_CONFIRMED) {
                if (alarmState != PENDING) {
                    Log.info("DISARMING: Login confirmed. Waiting for Disarm prompt");
                    currentTask = WAIT_FOR_DISARM_PROMPT;
                    delayCommand(COMMAND_SCREEN_STATE, 500);
                } else {
                    Log.info("DISARMING: Login confirmed. Waiting for Disarm confirmation");
                    currentTask = DISARM_REQUESTED;
                }
            } else {
                Log.info("DISARMING: Login failed to confirm. Aborting");
                abortTask();
            }
            break;

        case WAIT_FOR_DISARM_PROMPT :
            if (result == DISARM_PROMPT) {
                Log.info("DISARMING: Disarm prompt confirmed, disarming");
                if (!debugMode)
                    texSerial.println("KEYY");  // Yes

                currentTask = DISARM_REQUESTED;
            } else {
                Log.info("DISARMING: Unexpected result at WAIT_FOR_DISARM_PROMPT. Aborting");
                abortTask();
            }
            break;

        case DISARM_REQUESTED :
            if (result == IS_DISARMED) {
                Log.info("DISARMING: DISARM CONFIRMED");
                currentTask = IDLE;
                memset(userPin, 0, sizeof userPin);
                disarmStartTime = 0;
            } else {
                Log.info("DISARMING: Unexpected result at DISARM_REQUESTED. Aborting");
                abortTask();
            }
            break;
    }

    if (result == TASK_TIMEOUT && currentTask != IDLE)
        abortTask();
}

void Texecom::armSystem(RESULT result) {
    switch (currentTask) {
        case START :  // Initiate request
            if (armType == FULL_ARM)
                Log.info("ARMING: Starting full arm process");
            else if (armType == NIGHT_ARM)
                Log.info("ARMING: Starting night arm process");
            else
                return;

            armStartTime = millis();
            Log.info("ARMING: Requesting arm state");
            currentTask = CONFIRM_DISARMED;
            requestArmState();
            break;

        case CONFIRM_DISARMED:
            if (result == IS_DISARMED) {
                Log.info("ARMING: Confirmed disarmed. Confirming idle screen");
                currentTask = CONFIRM_IDLE_SCREEN;
                requestScreen();
            } else if (result == IS_ARMED) {
                Log.info("ARMING: System already armed. Aborting");
                abortTask();
            } else {
                abortTask();
            }
            break;

        case CONFIRM_IDLE_SCREEN :
            if (result == SCREEN_IDLE) {
                Log.info("ARMING: Idle screen confirmed. Starting login process");
                performLogin = true;
                currentTask = LOGIN;
            } else {
                Log.info("ARMING: Screen is not idle. Aborting");
                abortTask();
            }
            break;

        case LOGIN:
            if (result == LOGIN_COMPLETE) {
                Log.info("ARMING: Login complete. Awaiting confirmed login");
                currentTask = LOGIN_WAIT;
            } else {
                Log.info("ARMING: Login failed. Aborting");
                abortTask();
            }
            break;

        case LOGIN_WAIT :
            if (result == LOGIN_CONFIRMED) {
                Log.info("ARMING: Login confirmed. Waiting for Arm prompt");
                currentTask = WAIT_FOR_ARM_PROMPT;
                delayCommand(COMMAND_SCREEN_STATE, 500);
            } else {
                Log.info("ARMING: Login failed to confirm. Aborting");
                abortTask();
            }
            break;

        case WAIT_FOR_ARM_PROMPT :
            if (result == FULL_ARM_PROMPT) {
                if (armType == FULL_ARM) {
                    Log.info("ARMING: Full arm prompt confirmed, completing full arm");
                    if (!debugMode)
                        texSerial.println("KEYY");  // Yes
                    currentTask = ARM_REQUESTED;
                } else if (armType == NIGHT_ARM) {
                    Log.info("ARMING: Full arm prompt confirmed, waiting for part arm prompt");
                    currentTask = WAIT_FOR_PART_ARM_PROMPT;
                    texSerial.println("KEYD");  // Down
                    delayCommand(COMMAND_SCREEN_STATE, 500);
                }
            } else {
                Log.info("ARMING: Unexpected result at WAIT_FOR_ARM_PROMPT. Aborting");
                abortTask();
            }
            break;

        case WAIT_FOR_PART_ARM_PROMPT :
            if (result == PART_ARM_PROMPT) {
                Log.info("ARMING: Part arm prompt confirmed, waiting for night arm prompt");
                currentTask = WAIT_FOR_NIGHT_ARM_PROMPT;
                texSerial.println("KEYY");  // Yes
                delayCommand(COMMAND_SCREEN_STATE, 500);
            } else {
                Log.info("ARMING: Unexpected result at WAIT_FOR_PART_ARM_PROMPT. Aborting");
                abortTask();
            }
            break;

        case WAIT_FOR_NIGHT_ARM_PROMPT :
            if (result == NIGHT_ARM_PROMPT) {
                Log.info("ARMING: Night arm prompt confirmed, Completing part arm");
                if (!debugMode)
                    texSerial.println("KEYY");  // Yes
                currentTask = ARM_REQUESTED;
            } else {
                Log.info("ARMING: Unexpected result at WAIT_FOR_NIGHT_ARM_PROMPT. Aborting");
                abortTask();
            }
            break;

        case ARM_REQUESTED :
            if (result == IS_ARMING) {
                Log.info("ARMING: ARM CONFIRMED");
                currentTask = IDLE;
                memset(userPin, 0, sizeof userPin);
                armStartTime = 0;
                delayCommand(COMMAND_SCREEN_STATE, 2500);
            } else {
                Log.info("ARMING: Unexpected result at ARM_REQUESTED. Aborting");
                abortTask();
            }
            break;
    }

    if (result == TASK_TIMEOUT && currentTask != IDLE)
        abortTask();
}

void Texecom::abortTask() {
    currentTask = IDLE;
    texSerial.println("KEYR");
    delayedCommandExecuteTime = 0;
    memset(userPin, 0, sizeof userPin);
    performLogin = false;
    nextPinEntryTime = 0;
    armStartTime = 0;
    disarmStartTime = 0;
    requestArmState();

    lastCommandTime = 0;
    commandAttempts = 0;
}

void Texecom::loop() {
    bool messageReady = false;
    bool messageComplete = true;

    // Read incoming serial data if available and copy to TCP port
    while (texSerial.available() > 0) {
        int incomingByte = texSerial.read();
        if (bufferPosition == 0)
            messageStart = millis();

        if (bufferPosition >= maxMessageSize) {
            memcpy(message, buffer, bufferPosition);
            message[bufferPosition] = '\0';
            messageReady = true;
            bufferPosition = 0;
            break;
        } else if (bufferPosition > 0 && incomingByte == '\"') {
            Log.info("Double message incoming?");
            memcpy(message, buffer, bufferPosition);
            message[bufferPosition] = '\0';
            messageReady = true;
            buffer[0] = incomingByte;
            bufferPosition = 1;
            messageStart = millis();
            break;
        } else if (incomingByte != 10 && incomingByte != 13) {
            buffer[bufferPosition++] = incomingByte;
        } else if (bufferPosition > 0) {
            memcpy(message, buffer, bufferPosition);
            message[bufferPosition] = '\0';
            messageReady = true;
            screenRequestRetryCount = 0;
            bufferPosition = 0;
            break;
        }
    }

    if (bufferPosition > 0 && millis() > (messageStart+100)) {
        Log.info("Message failed to receive within 100ms");
        memcpy(message, buffer, bufferPosition);
        message[bufferPosition] = '\0';
        messageReady = true;
        messageComplete = false;
        bufferPosition = 0;
    }

    if (messageReady) {
        char messageLength = strlen(message);
        Log.info(message);

        // Zone state changed
        if (messageLength == 6 &&
            strncmp(message, msgZoneUpdate, strlen(msgZoneUpdate)) == 0) {
            updateZoneState(message);
        // System Armed
        } else if (messageLength >= 6 &&
                    strncmp(message, msgArmUpdate, strlen(msgArmUpdate)) == 0) {
            updateAlarmState(ARMED);
        // System Disarmed
        } else if (messageLength >= 6 &&
                    strncmp(message, msgDisarmUpdate, strlen(msgDisarmUpdate)) == 0) {
            if (currentTask != IDLE)
                processTask(IS_DISARMED);
            updateAlarmState(DISARMED);
        // Entry while armed
        } else if (messageLength == 6 &&
                    strncmp(message, msgEntryUpdate, strlen(msgEntryUpdate)) == 0) {
            updateAlarmState(PENDING);
        // System arming
        } else if (messageLength == 6 &&
                    strncmp(message, msgArmingUpdate, strlen(msgArmingUpdate)) == 0) {
            if (currentTask != IDLE)
                processTask(IS_ARMING);
            updateAlarmState(ARMING);
        // Intruder
        } else if (messageLength == 6 &&
                    strncmp(message, msgIntruderUpdate, strlen(msgIntruderUpdate)) == 0) {
            updateAlarmState(TRIGGERED);
        // User logged in with code or tag
        } else if (messageLength == 6 &&
                    (strncmp(message, msgUserPinLogin, strlen(msgUserPinLogin)) == 0 ||
                    strncmp(message, msgUserTagLogin, strlen(msgUserTagLogin)) == 0)) {
            int user = message[4] - '0';

            if (user < userCount)
                Log.info("User logged in: %s", users[user]);
            else
                Log.info("User logged in: Outside of user array size");

            if (currentTask != IDLE)
                processTask(LOGIN_CONFIRMED);
        // Reply to ASTATUS request that the system is disarmed
        } else if (messageLength == 5 &&
                    strncmp(message, msgReplyDisarmed, strlen(msgReplyDisarmed)) == 0) {
            if (currentTask != IDLE)
                processTask(IS_DISARMED);
            else
                updateAlarmState(DISARMED);
        // Reply to ASTATUS request that the system is armed
        } else if (messageLength == 5 &&
                    strncmp(message, msgReplyArmed, strlen(msgReplyArmed)) == 0) {
            if (currentTask != IDLE) {
                processTask(IS_ARMED);
            } else {
                updateAlarmState(ARMED);
            }
        } else if (
                (messageLength >= strlen(msgScreenArmedPart) &&
                    strncmp(message, msgScreenArmedPart, strlen(msgScreenArmedPart)) == 0) ||
                (messageLength >= strlen(msgScreenArmedNight) &&
                    strncmp(message, msgScreenArmedNight, strlen(msgScreenArmedNight)) == 0) ||
                (messageLength >= strlen(msgScreenIdlePartArmed) &&
                    strncmp(message, msgScreenIdlePartArmed, strlen(msgScreenIdlePartArmed)) == 0)) {
            if (currentTask != IDLE)
                processTask(SCREEN_PART_ARMED);
            else
                updateAlarmState(ARMED_HOME);
        } else if (messageLength >= strlen(msgScreenArmedFull) &&
                    strncmp(message, msgScreenArmedFull, strlen(msgScreenArmedFull)) == 0) {
            if (currentTask != IDLE)
                processTask(SCREEN_FULL_ARMED);
            else
                updateAlarmState(ARMED_AWAY);
        } else if (messageLength >= strlen(msgScreenIdle) &&
                    strncmp(message, msgScreenIdle, strlen(msgScreenIdle)) == 0) {
            if (currentTask != IDLE)
                processTask(SCREEN_IDLE);
            else if (alarmState == ARMED)
                updateAlarmState(ARMED_AWAY);
        // Shown directly after user logs in
        } else if (messageLength > strlen(msgWelcomeBack) &&
                    strncmp(message, msgWelcomeBack, strlen(msgWelcomeBack)) == 0) {
            if (currentTask == WAIT_FOR_DISARM_PROMPT ||
                    currentTask == WAIT_FOR_ARM_PROMPT)
                delayCommand(COMMAND_SCREEN_STATE, 500);
        // Shown shortly after user logs in
        } else if (messageLength >= strlen(msgScreenQuestionArm) &&
                    strncmp(message, msgScreenQuestionArm, strlen(msgScreenQuestionArm)) == 0) {
            if (currentTask != IDLE)
                processTask(FULL_ARM_PROMPT);
        } else if (messageLength >= strlen(msgScreenQuestionPartArm) &&
                    strncmp(message, msgScreenQuestionPartArm, strlen(msgScreenQuestionPartArm)) == 0) {
            if (currentTask != IDLE)
                processTask(PART_ARM_PROMPT);
        } else if (messageLength >= strlen(msgScreenQuestionNightArm) &&
                    strncmp(message, msgScreenQuestionNightArm, strlen(msgScreenQuestionNightArm)) == 0) {
            if (currentTask != IDLE)
                processTask(NIGHT_ARM_PROMPT);
        } else if (messageLength >= strlen(msgScreenQuestionDisarm) &&
                    strncmp(message, msgScreenQuestionDisarm, strlen(msgScreenQuestionDisarm)) == 0) {
            if (currentTask != IDLE)
                processTask(DISARM_PROMPT);
        } else if (messageLength >= strlen(msgScreenAreainEntry) &&
                    strncmp(message, msgScreenAreainEntry, strlen(msgScreenAreainEntry)) == 0) {
            if (currentTask != IDLE)
                processTask(SCREEN_AREA_ENTRY);
        } else if (messageLength >= strlen(msgScreenAreainExit) &&
            strncmp(message, msgScreenAreainExit, strlen(msgScreenAreainExit)) == 0) {
            if (currentTask != IDLE)
                processTask(SCREEN_AREA_EXIT);
        // Fails to arm. e.g.
        // Zone 012 Active Garage Door
        } else if (messageLength >= 17 &&
                    strncmp(message, "Zone", 4) == 0 &&
                    (strstr(message, "Active") - message) == 9) {
            Log.info("Failed to arm. Zone active while Arming");
            requestArmState();
        } else {
            if (message[0] == '"')
                Log.info(String::format("Unknown texecom command - %s", message));
            else
                Log.info(String::format("Unknown non-texecom command - %s", message));

            if (currentTask != IDLE) {
                if (screenRequestRetryCount < 3) {
                    if (currentTask == CONFIRM_ARMED || currentTask == CONFIRM_DISARMED) {
                        screenRequestRetryCount++;
                        Log.info("Retrying arm state request");
                        requestArmState();
                    } else if (currentTask == CONFIRM_IDLE_SCREEN ||
                               currentTask == WAIT_FOR_ARM_PROMPT ||
                               currentTask == WAIT_FOR_DISARM_PROMPT ||
                               currentTask == WAIT_FOR_PART_ARM_PROMPT ||
                               currentTask == WAIT_FOR_NIGHT_ARM_PROMPT) {
                        screenRequestRetryCount++;
                        Log.info("Retrying screen request");
                        requestScreen();
                    }
                } else {
                    processTask(UNKNOWN_MESSAGE);
                }
            }
        }
    }

    if (performLogin && millis() > nextPinEntryTime) {
        texSerial.print("KEY");
        texSerial.println(userPin[loginPinPosition++]);

        if (loginPinPosition >= strlen(userPin)) {
            loginPinPosition = 0;
            nextPinEntryTime = 0;
            performLogin = false;
            if (currentTask != IDLE) {
                processTask(LOGIN_COMPLETE);
            }
        } else {
            nextPinEntryTime = millis() + PIN_ENTRY_DELAY;
        }
    }

    if (currentTask == IDLE && alarmState == ARMING &&
        millis() > (lastStateChange + armingTimeout)) {
        Log.info("Arming state timed out. Requesting arm state");
        lastStateChange = millis();
        requestArmState();
    }

    // used to execute delayed commands
    if (delayedCommandExecuteTime > 0 &&
        millis() > delayedCommandExecuteTime) {
        request(delayedCommand);
        delayedCommandExecuteTime = 0;
    }

    if (armStartTime != 0 &&
        millis() > (armStartTime + armTimeout))
        processTask(TASK_TIMEOUT);

    if (disarmStartTime != 0 &&
        millis() > (disarmStartTime + disarmTimeout)) {
        processTask(TASK_TIMEOUT);
    }

    if ((currentTask == CONFIRM_IDLE_SCREEN || currentTask == WAIT_FOR_ARM_PROMPT ||
        currentTask == WAIT_FOR_DISARM_PROMPT || currentTask == WAIT_FOR_PART_ARM_PROMPT ||
        currentTask == WAIT_FOR_NIGHT_ARM_PROMPT)
        && bufferPosition == 0 && delayedCommandExecuteTime == 0 && millis() > (lastCommandTime+commandWaitTimeout) && commandAttempts < maxRetries) {
        Log.info("commandWaitTimeout: Retrying request screen");
        commandAttempts++;
        requestScreen();
    }

    if (currentTask == IDLE && (lastStateCheck == 0 || millis() > (lastStateCheck + stateCheckFrequency)))
        requestArmState();
}
