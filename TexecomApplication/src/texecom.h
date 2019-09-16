// Copyright 2019 Kevin Cooper

#ifndef __TEXECOM_H_
#define __TEXECOM_H_

#include "Particle.h"

#define texSerial Serial1

class Texecom {
 public:
    typedef enum {
        DISARMED = 0,
        ARMED_HOME = 1,
        ARMED_AWAY = 2,
        PENDING = 3,
        ARMING = 4,
        TRIGGERED = 5,
        ARMED = 6,
        UNKNOWN = 7,
    } ALARM_STATE;

    typedef enum {
        COMMAND_ARMED_STATE = 0,
        COMMAND_SCREEN_STATE = 1
    } COMMAND;

    typedef enum {
        ALARM_STATE_CHANGE = 0,
        ZONE_STATE_CHANGE = 1,
        ALARM_TRIGGERED = 2,
        ALARM_READY_CHANGE = 3,
        SEND_MESSAGE = 4
    } CALLBACK_TYPE;

    typedef enum {
        IDLE,
        START,
        CONFIRM_ARMED,
        CONFIRM_DISARMED,
        CONFIRM_IDLE_SCREEN,
        LOGIN,
        LOGIN_WAIT,
        WAIT_FOR_DISARM_PROMPT,
        WAIT_FOR_ARM_PROMPT,
        WAIT_FOR_PART_ARM_PROMPT,
        WAIT_FOR_NIGHT_ARM_PROMPT,
        ARM_REQUESTED,
        DISARM_REQUESTED
    } OPERATION;

    typedef enum {
        RESULT_NONE,
        TASK_TIMEOUT,
        IS_ARMED,
        IS_DISARMED,
        SCREEN_IDLE,
        SCREEN_PART_ARMED,
        SCREEN_FULL_ARMED,
        SCREEN_AREA_ENTRY,
        SCREEN_AREA_EXIT,
        LOGIN_COMPLETE,
        LOGIN_CONFIRMED,
        FULL_ARM_PROMPT,
        PART_ARM_PROMPT,
        NIGHT_ARM_PROMPT,
        DISARM_PROMPT,
        IS_ARMING,
        UNKNOWN_MESSAGE
    } RESULT;

    typedef enum {
        DISARM = 0,
        ARM = 1
    } TASK_TYPE;
    
    typedef enum {
        FULL_ARM = 0,
        NIGHT_ARM = 1
    } ARM_TYPE;

 public:
    Texecom(void (*callback)(CALLBACK_TYPE, uint8_t, uint8_t, const char*));
    void setup();
    void loop();
    void disarm(const char *code);
    void arm(const char *code, ARM_TYPE type);
    void setDebug(bool enabled) { debugMode = enabled; }
    bool isReady() { return statePinAreaReady == LOW; }
    ALARM_STATE getState() { return alarmState; }

 private:
    bool debugMode = false;
    void requestArmState();
    void requestScreen();
    void processTask(RESULT result);
    void armSystem(RESULT result);
    void disarmSystem(RESULT result);
    void abortTask();
    void request(COMMAND command);
    void (*callback)(CALLBACK_TYPE, uint8_t, uint8_t, const char*);
    void delayCommand(COMMAND command, int delay);
    void updateAlarmState(ALARM_STATE alarmState);
    void updateZoneState(char *message);
    void checkDigiOutputs();

    const char *msgZoneUpdate = "\"Z0";
    const char *msgArmUpdate = "\"A0";
    const char *msgDisarmUpdate = "\"D0";
    const char *msgEntryUpdate = "\"E0";
    const char *msgArmingUpdate = "\"X0";
    const char *msgIntruderUpdate = "\"L0";

    const char *msgUserPinLogin = "\"U0";
    const char *msgUserTagLogin = "\"T0";

    const char *msgReplyDisarmed = "\"N";
    const char *msgReplyArmed = "\"Y";
    const char *msgWelcomeBack = "\"  Welcome Back";
    const char *msgScreenIdle = "\"  The Cooper's";
    const char *msgScreenIdlePartArmed = "\" * PART ARMED *";

    const char *msgScreenArmedPart = "\"Part";
    const char *msgScreenArmedNight = "\"Night";
    const char *msgScreenArmedFull = "\"Area FULL ARMED";

    const char *msgScreenQuestionArm = "\"Do you want to  Arm System?";
    const char *msgScreenQuestionPartArm = "\"Do you want to  Part Arm System?";
    const char *msgScreenQuestionNightArm = "\"Do you want:-   Night Arm";
    const char *msgScreenQuestionDisarm = "\"Do you want to  Disarm System?";

    const char *msgScreenAreainEntry = "\"Area in Entry";
    const char *msgScreenAreainExit = "\"Area in Exit >";

    uint8_t userCount;  // This is set dynamically at class initialisation
    const char *users[4] = {"root", "Kevin", "Nicki", "Mumma"};

    TASK_TYPE task;
    ARM_TYPE armType;
    COMMAND delayedCommand;
    uint32_t delayedCommandExecuteTime = 0;
    const char *commandStrings[2] = {"ASTATUS", "LSTATUS"};
    const uint8_t maxMessageSize = 100;
    char message[101];
    char buffer[101];
    uint8_t bufferPosition;
    uint8_t screenRequestRetryCount = 0;

    bool performLogin = false;

    OPERATION currentTask = IDLE;

    uint32_t disarmStartTime;
    const unsigned int disarmTimeout = 10000;  // 10 seconds

    uint32_t armStartTime;
    const unsigned int armTimeout = 15000;  // 15 seconds

    const int commandWaitTimeout = 2000;
    uint32_t lastCommandTime = 0;
    int commandAttempts = 0;
    const uint8_t maxRetries = 3;

    // uint32_t lastStateCheck;
    // const unsigned int stateCheckFrequency = 300000;

    char userPin[9];
    uint8_t loginPinPosition;
    uint32_t nextPinEntryTime;
    const int PIN_ENTRY_DELAY = 500;
    ALARM_STATE alarmState = UNKNOWN;
    uint32_t lastStateChange;
    const int armingTimeout = 45000;

    uint32_t messageStart;

    uint8_t triggeredZone = 0;

//  Digi Output - Argon Pin - Texecom Configuration
//  1 ----------------- D12 - 22 Full Armed
//  2 ----------------- D16 - 23 Part Armed
//  3 ----------------- D13 - 19 Exiting
//  4 ----------------- D17 - 17 Entry
//  5 ----------------- D14 - 00 Alarm
//  6 ----------------- D18 - 27 Arm Failed
//  7 ----------------- D15 - 66 Fault Present
//  8 ----------------- D19 - 16 Area Ready

    const int pinFullArmed = D12;
    const int pinPartArmed = D16;
    const int pinExiting = D13;
    const int pinEntry = D17;
    const int pinTriggered = D14;
    const int pinArmFailed = D18;
    const int pinFaultPresent = D15;
    const int pinAreaReady = D19;

    bool statePinFullArmed = HIGH;
    bool statePinPartArmed = HIGH;
    bool statePinEntry = HIGH;
    bool statePinExiting = HIGH;
    bool statePinTriggered = HIGH;
    bool statePinArmFailed = HIGH;
    bool statePinFaultPresent = HIGH;
    bool statePinAreaReady = LOW;
};

#endif  // __TEXECOM_H_
