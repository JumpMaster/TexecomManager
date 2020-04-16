//  TimeAlarms.h - Arduino Time alarms header for use with Time library

#ifndef TimeAlarms_h
#define TimeAlarms_h

#include "Particle.h"

#define timeDayOfWeek_t int
/* Useful Constants */
#define SECS_PER_MIN  (60L)
#define SECS_PER_HOUR (3600L)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24L)
#define DAYS_PER_WEEK (7L)
#define SECS_PER_WEEK (SECS_PER_DAY * DAYS_PER_WEEK)
#define SECS_PER_YEAR (SECS_PER_WEEK * 52L)
#define SECS_YR_2000  (946684800L) // the time at the start of y2k
#define ALARM_BUFFER 10UL;

/* Useful Macros for getting elapsed time */
#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)  
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN) 
#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)
#define dayOfWeek(_time_)  ((( _time_ / SECS_PER_DAY + 4)  % DAYS_PER_WEEK)+1) // 1 = Sunday
#define elapsedDays(_time_) ( _time_ / SECS_PER_DAY)  // this is number of days since Jan 1 1970
#define elapsedSecsToday(_time_)  (_time_ % SECS_PER_DAY)   // the number of seconds since last midnight 
// The following macros are used in calculating alarms and assume the clock is set to a date later than Jan 1 1971
// Always set the correct time before settting alarms
#define previousMidnight(_time_) (( _time_ / SECS_PER_DAY) * SECS_PER_DAY)  // time at the start of the given day
#define nextMidnight(_time_) ( previousMidnight(_time_)  + SECS_PER_DAY )   // time at the end of the given day 
#define elapsedSecsThisWeek(_time_)  (elapsedSecsToday(_time_) +  ((dayOfWeek(_time_)-1) * SECS_PER_DAY) )   // note that week starts on day 1
#define previousSunday(_time_)  (_time_ - elapsedSecsThisWeek(_time_))      // time at the start of the week for the given time
#define nextSunday(_time_) ( previousSunday(_time_)+SECS_PER_WEEK)          // time at the end of the week for the given time

/* Useful Macros for converting elapsed time to a time_t */
#define minutesToTime_t ((M)) ( (M) * SECS_PER_MIN)  
#define hoursToTime_t   ((H)) ( (H) * SECS_PER_HOUR)  
#define daysToTime_t    ((D)) ( (D) * SECS_PER_DAY) // fixed on Jul 22 2011
#define weeksToTime_t   ((W)) ( (W) * SECS_PER_WEEK)   

#define dowSunday 1
#define dowMonday 2
#define dowTuesday 3
#define dowWednesday 4
#define dowThursday 5
#define dowFriday 6
#define dowSaturday 7

#if !defined(dtNBR_ALARMS )
#if defined(__AVR__)
#define dtNBR_ALARMS 6   // max is 255
#elif defined(ESP8266)
#define dtNBR_ALARMS 20  // for esp8266 chip - max is 255
#else
#define dtNBR_ALARMS 12  // assume non-AVR has more memory
#endif
#endif

// #define USE_SPECIALIST_METHODS  // define this for testing

typedef enum {
  dtMillisecond,
  dtSecond,
  dtMinute,
  dtHour,
  dtDay
} dtUnits_t;

typedef struct {
  uint8_t alarmType      :4 ;  // enumeration of daily/weekly (in future:
                               // biweekly/semimonthly/monthly/annual)
                               // note that the current API only supports daily
                               // or weekly alarm periods
  uint8_t isEnabled      :1 ;  // the timer is only actioned if isEnabled is true
  uint8_t isOneShot      :1 ;  // the timer will be de-allocated after trigger is processed
} AlarmMode_t;

// new time based alarms should be added just before dtLastAlarmType
typedef enum {
  dtNotAllocated,
  dtTimer,
  dtExplicitAlarm,
  dtDailyAlarm,
  dtWeeklyAlarm,
  dtLastAlarmType
} dtAlarmPeriod_t ; // in future: dtBiweekly, dtMonthly, dtAnnual

// macro to return true if the given type is a time based alarm, false if timer or not allocated
#define dtIsAlarm(_type_)  (_type_ >= dtExplicitAlarm && _type_ < dtLastAlarmType)
#define dtUseAbsoluteValue(_type_)  (_type_ == dtTimer || _type_ == dtExplicitAlarm)

typedef uint8_t AlarmID_t;
typedef AlarmID_t AlarmId;  // Arduino friendly name

#define dtINVALID_ALARM_ID 255
#define dtINVALID_TIME     (time_t)(-1)
#define AlarmHMS(_hr_, _min_, _sec_) (_hr_ * SECS_PER_HOUR + _min_ * SECS_PER_MIN + _sec_)

typedef void (*OnTick_t)();  // alarm callback function typedef

// class defining an alarm instance, only used by dtAlarmsClass
class AlarmClass
{
public:
  AlarmClass();
  OnTick_t onTickHandler;
  void updateNextTrigger();
  time_t value;
  time_t nextTrigger;
  AlarmMode_t Mode;
};

// class containing the collection of alarms
class TimeAlarmsClass
{
private:
  AlarmClass Alarm[dtNBR_ALARMS];
  void serviceAlarms();
  bool isServicing;
  bool firstRun = true;
  long nextAlarmAllowed = 0;
  uint8_t servicedAlarmId; // the alarm currently being serviced
  AlarmID_t create(time_t value, OnTick_t onTickHandler, uint8_t isOneShot, dtAlarmPeriod_t alarmType);

public:
  TimeAlarmsClass();
  // functions to create alarms and timers

  // trigger once at the given time in the future
  AlarmID_t triggerOnce(time_t value, OnTick_t onTickHandler) {
    if (value <= 0) return dtINVALID_ALARM_ID;
    return create(value, onTickHandler, true, dtExplicitAlarm);
  }

  // trigger once at given time of day
  AlarmID_t alarmOnce(time_t value, OnTick_t onTickHandler) {
    if (value <= 0 || value > SECS_PER_DAY) return dtINVALID_ALARM_ID;
    return create(value, onTickHandler, true, dtDailyAlarm);
  }
  AlarmID_t alarmOnce(const uint8_t H, const uint8_t M, const uint8_t S, OnTick_t onTickHandler) {
    return alarmOnce(AlarmHMS(H,M,S), onTickHandler);
  }

  // trigger once on a given day and time
  AlarmID_t alarmOnce(const timeDayOfWeek_t DOW, const uint8_t H, const uint8_t M, const uint8_t S, OnTick_t onTickHandler) {
    time_t value = (DOW-1) * SECS_PER_DAY + AlarmHMS(H,M,S);
    if (value <= 0) return dtINVALID_ALARM_ID;
    return create(value, onTickHandler, true, dtWeeklyAlarm);
  }

  // trigger daily at given time of day
  AlarmID_t alarmRepeat(time_t value, OnTick_t onTickHandler) {
    if (value > SECS_PER_DAY) return dtINVALID_ALARM_ID;
    return create(value, onTickHandler, false, dtDailyAlarm);
  }
  AlarmID_t alarmRepeat(const int H, const int M, const int S, OnTick_t onTickHandler) {
    return alarmRepeat(AlarmHMS(H,M,S), onTickHandler);
  }

  // trigger weekly at a specific day and time
  AlarmID_t alarmRepeat(const timeDayOfWeek_t DOW, const uint8_t H, const uint8_t M, const uint8_t S, OnTick_t onTickHandler) {
    time_t value = (DOW-1) * SECS_PER_DAY + AlarmHMS(H,M,S);
    if (value <= 0) return dtINVALID_ALARM_ID;
    return create(value, onTickHandler, false, dtWeeklyAlarm);
  }

  // trigger once after the given number of seconds
  AlarmID_t timerOnce(time_t value, OnTick_t onTickHandler) {
    if (value <= 0) return dtINVALID_ALARM_ID;
    return create(value, onTickHandler, true, dtTimer);
  }
  AlarmID_t timerOnce(const uint8_t H, const uint8_t M, const uint8_t S, OnTick_t onTickHandler) {
    return timerOnce(AlarmHMS(H,M,S), onTickHandler);
  }

  // trigger at a regular interval
  AlarmID_t timerRepeat(time_t value, OnTick_t onTickHandler) {
    if (value <= 0) return dtINVALID_ALARM_ID;
    return create(value, onTickHandler, false, dtTimer);
  }
  AlarmID_t timerRepeat(const uint8_t H,  const uint8_t M,  const uint8_t S, OnTick_t onTickHandler) {
    return timerRepeat(AlarmHMS(H,M,S), onTickHandler);
  }

  // void delay(unsigned long ms);
  void loop();

  // utility methods
  uint8_t getDigitsNow( dtUnits_t Units) const;         // returns the current digit value for the given time unit
  void waitForDigits( uint8_t Digits, dtUnits_t Units);
  void waitForRollover(dtUnits_t Units);

  // low level methods
  void enable(AlarmID_t ID);                // enable the alarm to trigger
  void disable(AlarmID_t ID);               // prevent the alarm from triggering
  void completeTriggeredAlarm();
  AlarmID_t getTriggeredAlarmId() const;          // returns the currently triggered  alarm id
  bool getIsServicing() const;                    // returns isServicing
  void write(AlarmID_t ID, time_t value);   // write the value (and enable) the alarm with the given ID
  time_t read(AlarmID_t ID) const;                // return the value for the given timer
  dtAlarmPeriod_t readType(AlarmID_t ID) const;   // return the alarm type for the given alarm ID

  void free(AlarmID_t ID);                  // free the id to allow its reuse

#ifndef USE_SPECIALIST_METHODS
private:  // the following methods are for testing and are not documented as part of the standard library
#endif
  uint8_t count() const;                          // returns the number of allocated timers
  time_t getNextTrigger() const;                  // returns the time of the next scheduled alarm
  time_t getNextTrigger(AlarmID_t ID) const;      // returns the time of scheduled alarm
  bool isAllocated(AlarmID_t ID) const;           // returns true if this id is allocated
  bool isAlarm(AlarmID_t ID) const;               // returns true if id is for a time based alarm, false if its a timer or not allocated
};

extern TimeAlarmsClass Alarm;  // make an instance for the user

/*==============================================================================
 * MACROS
 *============================================================================*/

/* public */
#define waitUntilThisSecond(_val_) waitForDigits( _val_, dtSecond)
#define waitUntilThisMinute(_val_) waitForDigits( _val_, dtMinute)
#define waitUntilThisHour(_val_)   waitForDigits( _val_, dtHour)
#define waitUntilThisDay(_val_)    waitForDigits( _val_, dtDay)
#define waitMinuteRollover() waitForRollover(dtSecond)
#define waitHourRollover()   waitForRollover(dtMinute)
#define waitDayRollover()    waitForRollover(dtHour)


#endif /* TimeAlarms_h */
