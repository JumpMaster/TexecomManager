// Copyright 2019 Kevin Cooper

#include "texecom.h"
#include "mqtt.h"
#include "papertrail.h"
#include "application.h"
#include "secrets.h"

ApplicationWatchdog wd(60000, System.reset);

MQTT mqttClient(mqttServer, 1883, mqttCallback);
uint32_t lastMqttConnectAttempt;
const int mqttConnectAtemptTimeout1 = 5000;
const int mqttConnectAtemptTimeout2 = 30000;
unsigned int mqttConnectionAttempts;

bool mqttStateConfirmed = true;

void alarmCallback(Texecom::CALLBACK_TYPE callbackType, int zone, int state);
void updateAlarmState(Texecom::ALARM_STATE newState);

Texecom alarm(alarmCallback);

uint32_t resetTime = 0;

char code[9];
uint32_t codeClearTimer;

PapertrailLogHandler papertrailHandler(papertrailAddress, papertrailPort, "Texecom");

void alarmCallback(Texecom::CALLBACK_TYPE callbackType, int zone, int state) {
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

    if (strcmp(topic, "home/security/alarm/code") == 0) {
        if (strlen(p) >= 4 && strlen(p) <= 8 && digitsOnly(p))
            snprintf(code, sizeof(code), p);
            codeClearTimer = millis() + 1000;

    } else if (strcmp(topic, "home/security/alarm/set") == 0) {
        if (strlen(code) >= 4) {
            if (strcmp(p, "arm_away") == 0) {
                alarm.fullArm(code);
            } else if (strcmp(p, "arm_night") == 0) {
                alarm.nightArm(code);
            } else if (strcmp(p, "disarm") == 0) {
                alarm.disarm(code);
            }
        } else {
            Log.info("Command received but code is empty");
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

void sendTriggeredMessage(int triggeredZone) {
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
            mqttClient.publish("home/security/alarm/state", alarmStateStrings[newState], MQTT::QOS1, true);
    }
}

void updateZoneState(int zone, int state) {
    char mqttData[29];
    snprintf(mqttData, sizeof(mqttData), "home/security/zone/%03d/state", zone);

    char zoneState[2];
    zoneState[0] = '0' + state;
    zoneState[1] = '\0';

    if (mqttClient.isConnected())
        mqttClient.publish(mqttData, zoneState, true);
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
    Particle.publish("pushover", String::format("Alarm: I am awake!: %d-%d", System.resetReason(), resetReasonData), PRIVATE);
}

void loop() {
    if (mqttClient.isConnected()) {
        mqttClient.loop();
    } else if ((mqttConnectionAttempts < 5 && millis() > (lastMqttConnectAttempt + mqttConnectAtemptTimeout1)) ||
                 millis() > (lastMqttConnectAttempt + mqttConnectAtemptTimeout2)) {
        connectToMQTT();
    }

    if (codeClearTimer > 0 && millis() > codeClearTimer) {
        codeClearTimer = 0;
        memset(code, 0, sizeof code);
    }

    alarm.loop();
    wd.checkin();  // resets the AWDT count
}
