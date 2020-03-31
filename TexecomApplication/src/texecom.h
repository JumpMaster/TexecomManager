// Copyright 2019 Kevin Cooper

#ifndef __TEXECOM_H_
#define __TEXECOM_H_

#include "Particle.h"

#define texSerial Serial1

class Texecom {
 public:

    struct SAVE_DATA {
        bool isDebug;
        char udlCode[7];
    };

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
        ALARM_STATE_CHANGE = 0,
        ZONE_STATE_CHANGE = 1,
        ALARM_TRIGGERED = 2,
        ALARM_READY_CHANGE = 3,
        SEND_MESSAGE = 4
    } CALLBACK_TYPE;

    typedef enum {
        COMMAND_ARMED_STATE = 0,
        COMMAND_SCREEN_STATE = 1
    } CRESTRON_COMMAND;

    typedef enum {
        // CRESTRON_IDLE,
        CRESTRON_START,
        CRESTRON_CONFIRM_ARMED,
        CRESTRON_CONFIRM_DISARMED,
        CRESTRON_CONFIRM_IDLE_SCREEN,
        CRESTRON_LOGIN,
        CRESTRON_LOGIN_WAIT,
        CRESTRON_WAIT_FOR_DISARM_PROMPT,
        CRESTRON_WAIT_FOR_ARM_PROMPT,
        CRESTRON_WAIT_FOR_PART_ARM_PROMPT,
        CRESTRON_WAIT_FOR_NIGHT_ARM_PROMPT,
        CRESTRON_ARM_REQUESTED,
        CRESTRON_DISARM_REQUESTED,
        SIMPLE_LOGIN_REQUIRED,
        SIMPLE_LOGIN,
        SIMPLE_START,
        SIMPLE_LOGOUT,
        SIMPLE_REQUEST_TIME,
    } TASK_STEP;

    typedef enum {
        RESULT_NONE,
        CRESTRON_TASK_TIMEOUT,
        CRESTRON_IS_ARMED,
        CRESTRON_IS_DISARMED,
        CRESTRON_SCREEN_IDLE,
        CRESTRON_SCREEN_PART_ARMED,
        CRESTRON_SCREEN_FULL_ARMED,
        CRESTRON_SCREEN_AREA_ENTRY,
        CRESTRON_SCREEN_AREA_EXIT,
        CRESTRON_LOGIN_COMPLETE,
        CRESTRON_LOGIN_CONFIRMED,
        CRESTRON_FULL_ARM_PROMPT,
        CRESTRON_PART_ARM_PROMPT,
        CRESTRON_NIGHT_ARM_PROMPT,
        CRESTRON_DISARM_PROMPT,
        CRESTRON_IS_ARMING,
        UNKNOWN_MESSAGE,
        SIMPLE_OK,
        SIMPLE_ERROR,
        SIMPLE_LOGIN_CONFIRMED,
        SIMPLE_LOGIN_TIMEOUT,
        SIMPLE_TIME_CHECK_OK,
        SIMPLE_TIME_CHECK_OUT
    } TASK_STEP_RESULT;

    typedef enum {
        CRESTRON_IDLE = 0,
        CRESTRON_DISARM = 1,
        CRESTRON_ARM = 2
    } CRESTRON_TASK;
    
    typedef enum {
        SIMPLE_IDLE = 0,
        SIMPLE_CHECK_TIME = 1,
        SIMPLE_SET_TIME = 2
    } SIMPLE_TASK;

    typedef enum {
        FULL_ARM = 0,
        NIGHT_ARM = 1
    } ARM_TYPE;

    typedef enum {
        CRESTRON = 0,
        SIMPLE = 1
    } PROTOCOL;

 public:
    Texecom(void (*callback)(CALLBACK_TYPE, uint8_t, uint8_t, const char*));
    void setup();
    void loop();
    void disarm(const char *code);
    void arm(const char *code, ARM_TYPE type);
    void setDebug(bool enabled);
    bool isReady() { return statePinAreaReady == LOW; }
    ALARM_STATE getState() { return alarmState; }
    void sendTest(const  char *text);
    void setUDLCode(const char *code);

 private:
    void requestArmState();
    void requestScreen();
    void processTask(TASK_STEP_RESULT result);
    void armSystem(TASK_STEP_RESULT result);
    void disarmSystem(TASK_STEP_RESULT result);
    void simpleLogin(TASK_STEP_RESULT result);
    void checkTime(TASK_STEP_RESULT result);
    void abortTask();
    void request(CRESTRON_COMMAND command);
    void (*callback)(CALLBACK_TYPE, uint8_t, uint8_t, const char*);
    void delayCommand(CRESTRON_COMMAND command, int delay);
    void updateAlarmState(ALARM_STATE alarmState);
    void updateZoneState(char *message);
    void checkDigiOutputs();
    bool processCrestronMessage(char *message, uint8_t messageLength);
    bool processSimpleMessage(char *message, uint8_t messageLength);
    void sendSimpleMessage(const char *text, uint8_t length);
    bool checkSimpleChecksum(const char *text, uint8_t length);

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

    static const uint8_t userCount = 4;
    const char *users[userCount] = {"root", "Kevin", "Nicki", "Mumma"};

    PROTOCOL activeProtocol = CRESTRON;
    CRESTRON_TASK crestronTask = CRESTRON_IDLE;
    SIMPLE_TASK simpleTask = SIMPLE_IDLE;
    ARM_TYPE armType;
    CRESTRON_COMMAND delayedCommand;
    uint32_t delayedCommandExecuteTime = 0;
    const char *commandStrings[2] = {"ASTATUS", "LSTATUS"};
    const uint8_t maxMessageSize = 100;
    char message[101];
    char buffer[101];
    uint8_t bufferPosition;
    uint8_t screenRequestRetryCount = 0;

    TASK_STEP taskStep = CRESTRON_START;

    uint32_t disarmStartTime;
    const unsigned int disarmTimeout = 10000;  // 10 seconds

    uint32_t armStartTime;
    const unsigned int armTimeout = 15000;  // 15 seconds

    const int commandWaitTimeout = 2000;
    uint32_t lastCommandTime = 0;
    int commandAttempts = 0;
    const uint8_t maxRetries = 3;

    char userPin[9];
    uint8_t loginPinPosition;
    uint32_t nextPinEntryTime;
    const int PIN_ENTRY_DELAY = 500;
    ALARM_STATE alarmState = UNKNOWN;
    uint32_t lastStateChange;
    const int armingTimeout = 45000;

    uint32_t messageStart;

    SAVE_DATA savedData;
    uint32_t simpleProtocolTimeout;
    uint32_t simpleCommandLastSent;

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
