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

        /*
        bool a1 = (lowByte & 0x1) != 0;
        bool a2 = (lowByte & 0x2) != 0;
        bool a3 = (lowByte & 0x4) != 0;
        bool a4 = (lowByte & 0x8) != 0;
        bool a5 = (lowByte & 0x10) != 0;
        bool a6 = (lowByte & 0x20) != 0;
        bool a7 = (lowByte & 0x40) != 0;
        bool a8 = (lowByte & 0x80) != 0;

        bool b1 = (HighByte & 0x1) != 0;
        bool b2 = (HighByte & 0x2) != 0;
        bool b3 = (HighByte & 0x4) != 0;
        bool b4 = (HighByte & 0x8) != 0;
        bool b5 = (HighByte & 0x10) != 0;
        bool b6 = (HighByte & 0x20) != 0;
        bool b7 = (HighByte & 0x40) != 0;
        bool b8 = (HighByte & 0x80) != 0;

        Log.info("Zone %d L - %d %d %d %d %d %d %d %d", (i/2)+9, a1, a2, a3, a4, a5, a6, a7, a8);
        Log.info("Zone %d H - %d %d %d %d %d %d %d %d", (i/2)+9, b1, b2, b3, b4, b5, b6, b7, b8);
        */


    }
    return true;
}

void SimpleHelper::simpleLogout() {
    
}