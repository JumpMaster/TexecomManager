// Copyright 2019 Kevin Cooper

#include "texecom.h"
#include "mqtt.h"
#include "papertrail.h"
#include "Particle.h"
#include "secrets.h"

// Stubs
void mqttCallback(char* topic, byte* payload, unsigned int length);
void sendTriggeredMessage(uint8_t triggeredZone);
void alarmCallback(Texecom::CALLBACK_TYPE callbackType, uint8_t zone, uint8_t state);
void updateAlarmState(Texecom::ALARM_STATE newState);
void updateZoneState(uint8_t zone, uint8_t state);

ApplicationWatchdog wd(60000, System.reset);

MQTT mqttClient(mqttServer, 1883, mqttCallback);
uint32_t lastMqttConnectAttempt;
const int mqttConnectAtemptTimeout1 = 5000;
const int mqttConnectAtemptTimeout2 = 30000;
unsigned int mqttConnectionAttempts;
bool mqttStateConfirmed = true;
Texecom alarm(alarmCallback);
uint32_t resetTime = 0;
bool isDebug = false;

PapertrailLogHandler papertrailHandler(papertrailAddress, papertrailPort, "Texecom");

void alarmCallback(Texecom::CALLBACK_TYPE callbackType, uint8_t zone, uint8_t state) {
    if (callbackType == Texecom::ZONE_STATE_CHANGE) {
        updateZoneState(zone, state);
    } else if (callbackType == Texecom::ALARM_STATE_CHANGE) {
        updateAlarmState((Texecom::ALARM_STATE) state);
    } else if (callbackType == Texecom::ALARM_TRIGGERED) {
        sendTriggeredMessage(zone);
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
            if (strcmp(action, "arm_away") == 0) {
                alarm.arm(code, Texecom::FULL_ARM);
            } else if (
                        strcmp(action, "arm_night") == 0 ||
                        strcmp(action, "arm_home") == 0
                      ) {
                alarm.arm(code, Texecom::NIGHT_ARM);
            } else if (strcmp(action, "disarm") == 0) {
                alarm.disarm(code);
            }
        } else {
            Log.error("Command received but code is < 4 char");
        }
        
    } else if (strcmp(topic, "home/security/alarm/state") == 0) {
        if (alarm.getState() != Texecom::UNKNOWN) {
            if (strcmp(alarmStateStrings[alarm.getState()], p) == 0)
                mqttStateConfirmed = true;
            else
                updateAlarmState(alarm.getState());
        }
    }
}

void sendTriggeredMessage(uint8_t triggeredZone) {
    char charData[22];
    snprintf(charData, sizeof(charData), "Triggered by zone %d", triggeredZone);
    Log.info(charData);

    if (triggeredZone >= 10 && mqttClient.isConnected())
        mqttClient.publish("home/security/alarm/trigger", alarmZoneStrings[triggeredZone-10], true);
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

int setDebug(const char *data) {
    if (strcmp(data, "true") == 0) {
        isDebug = true;
    } else {
        isDebug = false;
    }
    
    alarm.setDebug(isDebug);
    
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

SYSTEM_THREAD(ENABLED)
STARTUP(System.enableFeature(FEATURE_RESET_INFO));
STARTUP(WiFi.selectAntenna(ANT_EXTERNAL));  // selects the u.FL antenna

void setup() {
    Particle.variable("reset-time", resetTime);
    waitUntil(WiFi.ready);
    connectToMQTT();

    uint32_t resetReasonData = System.resetReasonData();
    Particle.function("setDebug", setDebug);
    Particle.variable("isDebug", isDebug);
    Particle.publish("pushover", String::format("Alarm: I am awake!: %d-%d", System.resetReason(), resetReasonData), PRIVATE);
}

void loop() {
    if (mqttClient.isConnected()) {
        mqttClient.loop();
    } else if ((mqttConnectionAttempts < 5 && millis() > (lastMqttConnectAttempt + mqttConnectAtemptTimeout1)) ||
                 millis() > (lastMqttConnectAttempt + mqttConnectAtemptTimeout2)) {
        connectToMQTT();
    }

    alarm.loop();
    wd.checkin();  // resets the AWDT count
}
