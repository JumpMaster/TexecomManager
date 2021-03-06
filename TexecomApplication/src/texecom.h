// Copyright 2019 Kevin Cooper

#ifndef __TEXECOM_H_
#define __TEXECOM_H_

#include "Particle.h"
#include "crestonhelper.h"
#include "simplehelper.h"

#define texSerial Serial1

#define firstZone 9 // Zone 1 = 1
#define zoneCount 11 // 1 == 1

class TexecomClass {
 public:

    struct SAVE_DATA {
        bool isDebug;
        char udlCode[7];
    };

    typedef enum {
        ZONE_ACTIVE = 1 << 0,
        ZONE_TAMPER = 1 << 1,
        ZONE_FAULT = 1 << 2,
        ZONE_FAILED_TEST = 1 << 3,
        ZONE_ALARMED = 1 << 4,
        ZONE_MANUAL_BYPASS = 1 << 5,
        ZONE_AUTO_BYPASS = 1 << 6,
        ZONE_ALWAYS_ZERO = 1 << 7,
    } ZONE_FLAGS;

    typedef enum {
        ALARM_READY = 1 << 0,
        ALARM_FAULT = 1 << 1,
        ALARM_ARM_FAILED = 1 << 2,
    } ALARM_FLAGS;

    typedef enum {
        DISARMED = 0,
        ARMED_HOME = 1,
        ARMED_AWAY = 2,
        ENTRY = 3,
        EXIT = 4,
        TRIGGERED = 5,
    } ALARM_STATE;

    typedef enum {
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
        SIMPLE_SEND_TIME,
        SIMPLE_READ_ZONE_STATE,
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
        SIMPLE_SET_TIME = 2,
        SIMPLE_ZONE_CHECK = 3,
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
    TexecomClass();
    void setZoneCallback(void (*zoneCallback)(uint8_t, uint8_t));
    void setAlarmCallback(void (*alarmCallback)(TexecomClass::ALARM_STATE, uint8_t));
    SimpleHelper simpleHelper;
    CrestronHelper crestronHelper;
    void setup();
    void loop();
    void setDebug(bool enabled);
    bool isReady() { return statePinAreaReady == LOW; }
    ALARM_STATE getState() { return alarmState; }
    void updateAlarmState();
    void sendTest(const  char *text);
    void setUDLCode(const char *code);

    void requestTimeSync();
    static void startTimeSync();
    void syncTime();

    void requestZoneSync();
    static void startZoneSync();
    void syncZones();
    
    void requestDisarm(const char *code);
    static void startDisarm();
    void disarm();

    void requestArm(const char *code, ARM_TYPE type);
    static void startArm();
    void arm();


 private:
    void processTask(TASK_STEP_RESULT result);
    void armSystem(TASK_STEP_RESULT result);
    void disarmSystem(TASK_STEP_RESULT result);
    void simpleLogin(TASK_STEP_RESULT result);
    void checkTime(TASK_STEP_RESULT result);
    void zoneCheck(TASK_STEP_RESULT result);
    void abortCrestronTask();
    void (*zoneCallback)(uint8_t, uint8_t);
    void (*alarmCallback)(TexecomClass::ALARM_STATE, uint8_t);
    void delayCommand(CrestronHelper::CRESTRON_COMMAND command, int delay);
    void decodeZoneState(char *message);
    void updateZoneState(uint8_t zone);
    void checkDigiOutputs();
    bool processCrestronMessage(char *message, uint8_t messageLength);
    bool processSimpleMessage(char *message, uint8_t messageLength);

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
    CrestronHelper::CRESTRON_COMMAND delayedCommand;
    uint32_t delayedCommandExecuteTime = 0;
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
    ALARM_STATE alarmState = ARMED_AWAY;
    // uint32_t lastStateChange;
    uint32_t exitToDisarmTimeout = 0;
    const int armingTimeout = 45000;

    uint32_t messageStart;

    SAVE_DATA savedData;
    uint32_t simpleProtocolTimeout;
    uint32_t simpleCommandLastSent;

    uint8_t zoneStates[zoneCount];
    uint8_t alarmStateFlags;

//  Digi Output - Argon Pin - Texecom Configuration
//  1 ----------------- D12 - 22 Full Armed
//  2 ----------------- D16 - 23 Part Armed
//  3 ----------------- D13 - 19 Exit
//  4 ----------------- D17 - 17 Entry
//  5 ----------------- D14 - 00 Alarm
//  6 ----------------- D18 - 27 Arm Failed
//  7 ----------------- D15 - 66 Fault Present
//  8 ----------------- D19 - 16 Area Ready

    const int pinFullArmed = D12;
    const int pinPartArmed = D16;
    const int pinExit = D13;
    const int pinEntry = D17;
    const int pinTriggered = D14;
    const int pinArmFailed = D18;
    const int pinFaultPresent = D15;
    const int pinAreaReady = D19;

    bool statePinFullArmed = HIGH;
    bool statePinPartArmed = HIGH;
    bool statePinEntry = HIGH;
    bool statePinExit = HIGH;
    bool statePinTriggered = HIGH;
    bool statePinArmFailed = HIGH;
    bool statePinFaultPresent = HIGH;
    bool statePinAreaReady = LOW;
};

extern TexecomClass Texecom;  // make an instance for the user

#endif  // __TEXECOM_H_