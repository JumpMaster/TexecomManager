// Copyright 2019 Kevin Cooper

#include "texecom.h"
#include "mqtt.h"
#include "papertrail.h"
#include "Particle.h"
#include "secrets.h"
#include "TimeAlarms.h"
#include "DiagnosticsHelperRK.h"

// Stubs
void mqttCallback(char* topic, byte* payload, unsigned int length);
void sendTriggeredMessage(uint8_t triggeredZone);
void alarmCallback(TexecomClass::ALARM_STATE state, uint8_t flags);
void zoneCallback(uint8_t zone, uint8_t state);
void publishAlarmState(TexecomClass::ALARM_STATE newState);
void updateZoneState(uint8_t zone, uint8_t state);

MQTT mqttClient(mqttServer, 1883, mqttCallback);
uint32_t lastMqttConnectAttempt;
const int mqttConnectAtemptTimeout1 = 5000;
const int mqttConnectAtemptTimeout2 = 30000;
unsigned int mqttConnectionAttempts;
bool mqttStateConfirmed = true;

bool isDebug = false;
uint32_t resetTime = 0;
retained uint32_t lastHardResetTime;
retained int resetCount;
TexecomClass::ALARM_STATE alarmState;

#define WATCHDOG_TIMEOUT_MS 30*1000

#define WDT_RREN_REG 0x40010508
#define WDT_CRV_REG 0x40010504
#define WDT_REG 0x40010000
void WatchDoginitialize() {
    *(uint32_t *) WDT_RREN_REG = 0x00000001;
    *(uint32_t *) WDT_CRV_REG = (uint32_t) (WATCHDOG_TIMEOUT_MS * 32.768);
    *(uint32_t *) WDT_REG = 0x00000001;
}

#define WDT_RR0_REG 0x40010600
#define WDT_RELOAD 0x6E524635
void WatchDogpet() {
    *(uint32_t *) WDT_RR0_REG = WDT_RELOAD;
}

PapertrailLogHandler papertrailHandler(papertrailAddress, papertrailPort,
  "ArgonTexecom", System.deviceID(),
  LOG_LEVEL_NONE, {
  { "app", LOG_LEVEL_ALL }
  // TOO MUCH!!! { “system”, LOG_LEVEL_ALL },
  // TOO MUCH!!! { “comm”, LOG_LEVEL_ALL }
});

void alarmCallback(TexecomClass::ALARM_STATE state, uint8_t flags) {

    if (state != alarmState) {
        alarmState = state;
        Log.info("Alarm: %s", alarmStateStrings[state]);
    }

    char message[58];

    snprintf(message,
                sizeof(message),
                "{\"state\":\"%s\",\"ready\":%d,\"fault\":%d,\"arm_failed\":%d}",
                alarmStateStrings[state],
                (flags & TexecomClass::ALARM_READY) != 0,
                (flags & TexecomClass::ALARM_FAULT) != 0,
                (flags & TexecomClass::ALARM_ARM_FAILED) != 0);


    mqttClient.publish("home/security/alarm", message, true);
}

void zoneCallback(uint8_t zone, uint8_t state) {

    char attributesTopic[34];
    snprintf(attributesTopic, sizeof(attributesTopic), "home/security/zone/%03d", zone);
    char attributesMsg[46];
    
    snprintf(attributesMsg,
            sizeof(attributesMsg),
            "{\"active\":%d,\"tamper\":%d,\"fault\":%d,\"alarmed\":%d}",
            (state & TexecomClass::ZONE_ACTIVE) != 0,
            (state & TexecomClass::ZONE_TAMPER) != 0,
            (state & TexecomClass::ZONE_FAULT) != 0,
            (state & TexecomClass::ZONE_ALARMED) != 0);

    if (mqttClient.isConnected()) {
        mqttClient.publish(attributesTopic, attributesMsg, true);
    }
}

bool digitsOnly(const char *s) {
    while (*s) {
        if (isdigit(*s++) == 0) return false;
    }
    return true;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = '\0';

    if (strcmp(topic, "home/security/alarm/set") == 0) {

        const char *action = strtok(p, ":");
        const char *code = strtok(NULL, ":");
    
        if (strlen(code) >= 4 && digitsOnly(code)) {

            if (strcmp(code, "8463") == 0) { // 8463 == TIME
                Texecom.requestTimeSync();
            } else if (strcmp(code, "7962") == 0) { // 7962 == SYNC
                Texecom.requestZoneSync();
            } else {
                if (strncmp(action, "arm", 3) == 0) {
                    if (Texecom.isReady()) {
                        if (strcmp(action, "arm_away") == 0) {
                            Texecom.requestArm(code, TexecomClass::FULL_ARM);
                        } else if (
                                    strcmp(action, "arm_night") == 0 ||
                                    strcmp(action, "arm_home") == 0
                                ) {
                            Texecom.requestArm(code, TexecomClass::NIGHT_ARM);
                        }
                    } else {
                        const char *notReadyMessage = "Arm attempted while alarm is not ready";
                        Log.error(notReadyMessage);
                        mqttClient.publish("home/notification/low", notReadyMessage);
                    }
                } else if (strcmp(action, "disarm") == 0) {
                    Texecom.requestDisarm(code);
                }
            }
        } else {
            Log.error("Command received but code is < 4 char");
        }
        
    } else if (strcmp(topic, "home/security/alarm/state") == 0) {
        if (strcmp(alarmStateStrings[Texecom.getState()], p) == 0)
            mqttStateConfirmed = true;
        else
            Texecom.updateAlarmState();
    } else if (strcmp(topic, "utilities/isDST") == 0) {
        if (strcmp(p, "true") == 0)
            Time.beginDST();
        else
            Time.endDST();
        
        if (Time.isDST())
            Log.info("DST is active");
        else
            Log.info("DST is inactive");
    }
}

int cloudReset(const char* data) {
    uint32_t rTime = millis() + 10000;
    Log.info("Cloud reset received");
    while (millis() < rTime)
        Particle.process();
    System.reset();
    return 0;
}

int setDebug(const char *data) {
    if (strcmp(data, "true") == 0) {
        isDebug = true;
    } else {
        isDebug = false;
    }
    
    Texecom.setDebug(isDebug);
    
    return 0;
}

int setUDL(const char *data) {
    Texecom.setUDLCode(data);
    return 0;
}

void connectToMQTT() {
    lastMqttConnectAttempt = millis();
    bool mqttConnected = mqttClient.connect(System.deviceID(), mqttUsername, mqttPassword);
    if (mqttConnected) {
        mqttConnectionAttempts = 0;
        Log.info("MQTT Connected");
        mqttClient.subscribe("home/security/alarm/set");
        mqttClient.subscribe("home/security/alarm/code");
        mqttClient.subscribe("home/security/alarm/state");
        mqttClient.subscribe("utilities/#");
    } else {
        mqttConnectionAttempts++;
        Log.info("MQTT failed to connect");
    }
}

uint32_t nextMetricsUpdate = 0;
void sendTelegrafMetrics() {
    if (millis() > nextMetricsUpdate) {
        nextMetricsUpdate = millis() + 30000;

        char buffer[150];
        snprintf(buffer, sizeof(buffer),
            "status,device=Texecom uptime=%d,resetReason=%d,firmware=\"%s\",memTotal=%ld,memFree=%ld",
            System.uptime(),
            System.resetReason(),
            System.version().c_str(),
            DiagnosticsHelper::getValue(DIAG_ID_SYSTEM_TOTAL_RAM),
            DiagnosticsHelper::getValue(DIAG_ID_SYSTEM_USED_RAM)
            );
        mqttClient.publish("telegraf/particle", buffer);
    }
}

void random_seed_from_cloud(unsigned seed) {
    srand(seed);
}

SYSTEM_THREAD(ENABLED)

void startupMacro() {
    System.enableFeature(FEATURE_RESET_INFO);
    System.enableFeature(FEATURE_RETAINED_MEMORY);
}
STARTUP(startupMacro());

void setup() {

    waitFor(Particle.connected, 30000);
    
    do {
        resetTime = Time.now();
        Particle.process();
    } while (resetTime < 1500000000 || millis() < 10000);
    
    if (System.resetReason() == RESET_REASON_PANIC) {
        if ((Time.now() - lastHardResetTime) < 120) {
            resetCount++;
        } else {
            resetCount = 1;
        }

        lastHardResetTime = Time.now();

        if (resetCount > 3) {
            System.enterSafeMode();
        }
    } else if (System.resetReason() == RESET_REASON_WATCHDOG) {
      Log.info("RESET BY WATCHDOG");
    } else {
        resetCount = 0;
    }

    Particle.function("setDebug", setDebug);
    Particle.function("cloudReset", cloudReset);
    Particle.function("setUDL", setUDL);

    Particle.variable("isDebug", isDebug);
    Particle.variable("reset-time", resetTime);
    Particle.publishVitals(900);

    WatchDoginitialize();

    Log.info("Boot complete. Reset count = %d", resetCount);

    connectToMQTT();

    Texecom.setAlarmCallback(alarmCallback);
    Texecom.setZoneCallback(zoneCallback);
    Texecom.setup();

    uint32_t resetReasonData = System.resetReasonData();
    Particle.publish("pushover", String::format("ArgonAlarm: I am awake!: %d-%d", System.resetReason(), resetReasonData), PRIVATE);
}

void loop() {
    if (mqttClient.isConnected()) {
        mqttClient.loop();
        sendTelegrafMetrics();
    } else if ((mqttConnectionAttempts < 5 && millis() > (lastMqttConnectAttempt + mqttConnectAtemptTimeout1)) ||
                 millis() > (lastMqttConnectAttempt + mqttConnectAtemptTimeout2)) {
        connectToMQTT();
    }

    Texecom.loop();

    WatchDogpet();
}