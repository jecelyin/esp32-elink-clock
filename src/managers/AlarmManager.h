#pragma once

#include "../drivers/RtcDriver.h"
#include "AlarmTypes.h"
#include "HolidayCalendar.h"
#include <Preferences.h>
#include <vector>

class AlarmManager {
public:
  AlarmManager();

  void begin();
  bool addAlarm(const AlarmConfig &alarm);
  AlarmConfig buildDefaultAlarm() const;
  void check(const DateTime &now);
  HolidayDayType getHolidayDayType(const DateTime &date);
  String getHolidayName(const DateTime &date);
  HolidayCountdown getNextHolidayCountdown(const DateTime &date,
                                           uint16_t maxDays);
  String getHolidayStatusText(uint16_t fullYear) const;
  AlarmConfig getAlarm(size_t index) const;
  size_t getAlarmCount() const;
  uint32_t getNextWakeDelayMs(const DateTime &now, uint32_t nowMs);
  String getRepeatText(const AlarmConfig &alarm) const;
  bool hasEnabledAlarms() const;
  bool isRinging() const;
  bool removeAlarm(size_t index);
  void snooze();
  void stop();
  bool toggleEnabled(size_t index);
  bool updateAlarm(size_t index, const AlarmConfig &alarm);
  void updateHolidayCache(const DateTime &now);

private:
  static const uint32_t CHECK_INTERVAL_MS = 1000UL;
  static const uint8_t LOOKAHEAD_DAYS = 31;

  std::vector<AlarmConfig> alarms;
  HolidayCalendar holidayCalendar;
  Preferences prefs;
  bool ringing;
  bool prefsReady;
  uint32_t lastCheck;

  AlarmRepeatType inferRepeatType(uint8_t weekMask) const;
  AlarmConfig sanitizeAlarm(const AlarmConfig &alarm) const;
  String buildWeeklyText(uint8_t weekMask) const;
  time_t findNextAlarmTime(const AlarmConfig &alarm, time_t nowTime);
  bool isIndexValid(size_t index) const;
  bool matchesDate(const AlarmConfig &alarm, const DateTime &date);
  DateTime toDateTime(time_t value) const;
  time_t toTimeT(const DateTime &date) const;
  uint32_t toMinuteKey(const DateTime &date) const;
  void load();
  void save();
};
