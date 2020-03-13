// Copyright 2019 Kevin Cooper

#include "texecom.h"
#include "mqtt.h"
#include "papertrail.h"
#include "Particle.h"
#include "secrets.h"

// Stubs
void mqttCallback(char* topic, byte* payload, unsigned int length);
void sendTriggeredMessage(uint8_t triggeredZone);
void alarmCallback(Texecom::CALLBACK_TYPE callbackType, uint8_t zone, uint8_t state, const char *message);
void updateAlarmState(Texecom::ALARM_STATE newState);
void updateZoneState(uint8_t zone, uint8_t state);

ApplicationWatchdog wd(60000, System.reset);

MQTT mqttClient(mqttServer, 1883, mqttCallback);
uint32_t lastMqttConnectAttempt;
const int mqttConnectAtemptTimeout1 = 5000;
const int mqttConnectAtemptTimeout2 = 30000;
unsigned int mqttConnectionAttempts;
bool mqttStateConfirmed = true;
Texecom texecom(alarmCallback);
uint32_t resetTime = 0;
bool isDebug = false;
retained uint32_t lastHardResetTime;
retained int resetCount;

PapertrailLogHandler papertrailHandler(papertrailAddress, papertrailPort,
  "ArgonTexecom", System.deviceID(),
  LOG_LEVEL_NONE, {
  { "app", LOG_LEVEL_ALL }
  // TOO MUCH!!! { “system”, LOG_LEVEL_ALL },
  // TOO MUCH!!! { “comm”, LOG_LEVEL_ALL }
});

void alarmCallback(Texecom::CALLBACK_TYPE callbackType, uint8_t zone, uint8_t state, const char *message) {
    if (callbackType == Texecom::ZONE_STATE_CHANGE) {
        updateZoneState(zone, state);
    } else if (callbackType == Texecom::ALARM_STATE_CHANGE) {
        updateAlarmState((Texecom::ALARM_STATE) state);
    } else if (callbackType == Texecom::ALARM_TRIGGERED) {
        sendTriggeredMessage(zone);
    } else if (callbackType == Texecom::SEND_MESSAGE) {
        if (mqttClient.isConnected()) {
            mqttClient.publish("home/notification/low", message);
        }
    } else if (callbackType == Texecom::ALARM_READY_CHANGE) {
        char readyPayload[14];
        snprintf(readyPayload, sizeof(readyPayload), "{\"ready\":\"%d\"}", state);
        mqttClient.publish("home/security/alarm/attributes", readyPayload, MQTT::QOS2, true);
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
            if (strncmp(action, "arm", 3) == 0) {
                if (texecom.isReady()) {
                    if (strcmp(action, "arm_away") == 0) {
                        texecom.arm(code, Texecom::FULL_ARM);
                    } else if (
                                strcmp(action, "arm_night") == 0 ||
                                strcmp(action, "arm_home") == 0
                            ) {
                        texecom.arm(code, Texecom::NIGHT_ARM);
                    }
                } else {
                    const char *notReadyMessage = "Arm attempted while alarm is not ready";
                    Log.error(notReadyMessage);
                    mqttClient.publish("home/notification/low", notReadyMessage);
                }
            } else if (strcmp(action, "disarm") == 0) {
                texecom.disarm(code);
            }
        } else {
            Log.error("Command received but code is < 4 char");
        }
        
    } else if (strcmp(topic, "home/security/alarm/state") == 0) {
        if (texecom.getState() != Texecom::UNKNOWN) {
            if (strcmp(alarmStateStrings[texecom.getState()], p) == 0)
                mqttStateConfirmed = true;
            else
                updateAlarmState(texecom.getState());
        }
    }
}

void sendTriggeredMessage(uint8_t triggeredZone) {
    char charData[100];
    snprintf(charData, sizeof(charData), "Triggered by zone %d", triggeredZone);
    Log.info(charData);

    if (triggeredZone >= 10 && mqttClient.isConnected()) {
        snprintf(charData, sizeof(charData), "Triggered by zone %s", alarmZoneStrings[triggeredZone-10]);
        mqttClient.publish("home/notification/high", charData);
    }
}

void updateAlarmState(Texecom::ALARM_STATE newState) {
    const char *charSource = "{\"alarm-state\":\"%s\"}";
    char charData[29];
    snprintf(charData, sizeof(charData), charSource, alarmStateStrings[newState]);
    Log.info(charData);

    if (newState == Texecom::DISARMED)
        mqttClient.publish("home/security/alarm/trigger", "", true);

    if (newState != Texecom::ARMED) {  // Update Home-Assistant
        mqttStateConfirmed = false;
        if (mqttClient.isConnected())
            mqttClient.publish("home/security/alarm/state", alarmStateStrings[newState], MQTT::QOS2, true);
    }
}

void updateZoneState(uint8_t zone, uint8_t state) {
    char mqttData[29];
    snprintf(mqttData, sizeof(mqttData), "home/security/zone/%03d/state", zone);

    char zoneState[2];
    zoneState[0] = '0' + state;
    zoneState[1] = '\0';

    if (mqttClient.isConnected())
        mqttClient.publish(mqttData, zoneState, true);
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
    
    texecom.setDebug(isDebug);
    
    return 0;
}

int setUDL(const char *data) {
    texecom.setUDLCode(data);
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
    } else {
        mqttConnectionAttempts++;
        Log.info("MQTT failed to connect");
    }
}

void random_seed_from_cloud(unsigned seed) {
    srand(seed);
}

int sendTest(const char* data) {
    texecom.sendTest(data);
    return 0;
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
    } else {
        resetCount = 0;
    }

    Particle.function("setDebug", setDebug);
    Particle.function("cloudReset", cloudReset);
    Particle.function("setUDL", setUDL);
    Particle.function("sendTest", sendTest);
    Particle.variable("isDebug", isDebug);
    Particle.variable("reset-time", resetTime);
    Particle.publishVitals(900);

    Log.info("Boot complete. Reset count = %d", resetCount);

    connectToMQTT();
    texecom.setup();

    uint32_t resetReasonData = System.resetReasonData();
    Particle.publish("pushover", String::format("ArgonAlarm: I am awake!: %d-%d", System.resetReason(), resetReasonData), PRIVATE);
}

void loop() {
    if (mqttClient.isConnected()) {
        mqttClient.loop();
    } else if ((mqttConnectionAttempts < 5 && millis() > (lastMqttConnectAttempt + mqttConnectAtemptTimeout1)) ||
                 millis() > (lastMqttConnectAttempt + mqttConnectAtemptTimeout2)) {
        connectToMQTT();
    }

    texecom.loop();
    wd.checkin();  // resets the AWDT count
}
