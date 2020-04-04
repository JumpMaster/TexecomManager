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
    t.tm_isdst = Time.isDST();             // Is DST on? 1 = yes, 0 = no, -1 = unknown
        
    uint32_t alarmTime = mktime(&t);
    uint32_t localTime = Time.local();
    Log.info("Time - Alarm:%ld Local:%ld", alarmTime, localTime);
    
    if (localTime+120 > alarmTime && localTime-120 < alarmTime) {
        return true; //processTask(SIMPLE_TIME_CHECK_OK);
    } else {
        return false; //processTask(SIMPLE_TIME_CHECK_OUT);
    }
}

void SimpleHelper::processReceivedZoneData(const char *message, uint8_t messageLength) {

    for (uint8_t i = 0; i < messageLength; i+=2) {
        char zoneLow = message[i];
        char zoneHigh = message[i+1];

        bool a1 = (zoneLow & 0x1) != 0;
        bool a2 = (zoneLow & 0x2) != 0;
        bool a3 = (zoneLow & 0x4) != 0;
        bool a4 = (zoneLow & 0x8) != 0;
        bool a5 = (zoneLow & 0x10) != 0;
        bool a6 = (zoneLow & 0x20) != 0;
        bool a7 = (zoneLow & 0x40) != 0;
        bool a8 = (zoneLow & 0x80) != 0;

        bool b1 = (zoneHigh & 0x1) != 0;
        bool b2 = (zoneHigh & 0x2) != 0;
        bool b3 = (zoneHigh & 0x4) != 0;
        bool b4 = (zoneHigh & 0x8) != 0;
        bool b5 = (zoneHigh & 0x10) != 0;
        bool b6 = (zoneHigh & 0x20) != 0;
        bool b7 = (zoneHigh & 0x40) != 0;
        bool b8 = (zoneHigh & 0x80) != 0;

        Log.info("Zone %d L - %d %d %d %d %d %d %d %d", (i/2)+9, a1, a2, a3, a4, a5, a6, a7, a8);
        Log.info("Zone %d H - %d %d %d %d %d %d %d %d", (i/2)+9, b1, b2, b3, b4, b5, b6, b7, b8);

    }
}