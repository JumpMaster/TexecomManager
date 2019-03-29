// Copyright 2019 Kevin Cooper

#ifndef __TEXECOM_H_
#define __TEXECOM_H_

#include "application.h"

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
        ALARM_TRIGGERED = 2
    } CALLBACK_TYPE;

    typedef enum {
        IDLE,
        START,
        CONFIRM_ARMED,
        CONFIRM_DISARMED,
        DISARMED_CONFIRMED,
        CONFIRM_IDLE_SCREEN,
        LOGIN,
        LOGIN_WAIT,
        WAIT_FOR_PROMPT,
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
    Texecom(void (*callback)(CALLBACK_TYPE, uint8_t, uint8_t));
    void loop();
    void nightArm(const char *code);
    void fullArm(const char *code);
    void disarm(const char *code);
    void arm(const char *code, ARM_TYPE type);
    void setDebug(bool enabled) { debugMode = enabled; }
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
    void (*callback)(CALLBACK_TYPE, uint8_t, uint8_t);
    void delayCommand(COMMAND command, int delay);
    void updateAlarmState(ALARM_STATE alarmState);
    void updateZoneState(char *message);

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
    bool messageReady = false;
    uint8_t bufferPosition;

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

    uint32_t lastStateCheck;
    const unsigned int stateCheckFrequency = 300000;

    char userPin[9];
    uint8_t loginPinPosition;
    uint32_t nextPinEntryTime;
    const int PIN_ENTRY_DELAY = 500;
    ALARM_STATE alarmState = UNKNOWN;
    uint32_t lastStateChange;
    const int armingTimeout = 45000;

    uint32_t messageStart;

    uint8_t triggeredZone = 0;
};

#endif  // __TEXECOM_H_
