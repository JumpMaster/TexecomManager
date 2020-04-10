#include "simplehelper.h"

SimpleHelper::SimpleHelper() {}

bool SimpleHelper::checkSimpleChecksum(const char *text, uint8_t length) {
    unsigned int a = 0;
    for (unsigned int i = 0; i < length; i++) {
        a += text[i];
    }
    char checksum = (a ^ 255) % 0x100;
    return checksum == text[length];
}

void SimpleHelper::sendSimpleMessage(const char *text, uint8_t length) {
    unsigned int a = 0;
    for (unsigned int i = 0; i < length; i++) {
        a += text[i];
        texSerial.write(text[i]);
        // Log.info("Message: %d", text[i]);
    }
    char checksum = (a ^ 255) % 0x100;
    texSerial.write(checksum);
    // Log.info("Message: %d", checksum);
}

bool SimpleHelper::processReceivedTime(const char *message) {
    struct tm t;
    t.tm_mday = message[0];     // Day of the month
    t.tm_mon = message[1]-1;    // Month, where 0 = Jan
    t.tm_year = message[2]+100; // Short Year + 100
    t.tm_hour = message[3];     // Hours
    t.tm_min = message[4];      // Minutes
    t.tm_sec = 0;               // Seconds
    t.tm_isdst = Time.isDST();  // Is DST on? 1 = yes, 0 = no, -1 = unknown
        
    uint32_t alarmTime = mktime(&t);
    uint32_t localTime = Time.local();
    Log.info("Time - Alarm:%ld Local:%ld", alarmTime, localTime);
    
    if (localTime+120 > alarmTime && localTime-120 < alarmTime) {
        return true;
    } else {
        return false;
    }
}

bool SimpleHelper::processReceivedZoneData(const char *message, uint8_t messageLength, uint8_t *zoneState) {

    for (uint8_t i = 0; i < messageLength; i+=2) {
        uint8_t lowByte = message[i];
        // uint8_t HighByte = message[i+1];
        zoneState[i/2] = lowByte;
    }
    return true;
}