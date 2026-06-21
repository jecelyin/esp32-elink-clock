#include "AlarmManager.h"
#include "../utils/WakeTiming.h"

namespace {
const char *kWeekNames[] = {"日", "一", "二", "三", "四", "五", "六"};
}

AlarmManager::AlarmManager() {
  ringing = false;
  prefsReady = false;
  lastCheck = 0;
}

void AlarmManager::begin() {
  prefsReady = prefs.begin("alarms", false);
  if (!prefsReady) {
    Serial.println("Alarm prefs open failed");
    return;
  }
  load();
}

bool AlarmManager::addAlarm(const AlarmConfig &alarm) {
  alarms.push_back(sanitizeAlarm(alarm));
  save();
  return true;
}

AlarmConfig AlarmManager::buildDefaultAlarm() const {
  AlarmConfig alarm;
  alarm.hour = 7;
  alarm.minute = 30;
  alarm.enabled = true;
  alarm.repeatType = ALARM_REPEAT_WORKDAY;
  alarm.weekMask = 0x3E;
  return alarm;
}

void AlarmManager::check(const DateTime &now) {
  if (millis() - lastCheck < CHECK_INTERVAL_MS || ringing) {
    return;
  }
  lastCheck = millis();

  uint32_t currentMinuteKey = toMinuteKey(now);
  for (size_t i = 0; i < alarms.size(); ++i) {
    AlarmConfig &alarm = alarms[i];
    if (!alarm.enabled || !matchesDate(alarm, now)) {
      continue;
    }
    if (alarm.hour != now.hour || alarm.minute != now.minute) {
      continue;
    }
    if (alarm.lastTriggeredMinuteKey == currentMinuteKey) {
      continue;
    }

    // 关键逻辑：用“分钟级时间戳”去重，确保同一分钟内多次唤醒、
    // 或者页面刷新重复调用 check() 时，不会把同一个闹钟连响多次。
    ringing = true;
    alarm.lastTriggeredMinuteKey = currentMinuteKey;
    return;
  }
}

String AlarmManager::getHolidayStatusText(uint16_t fullYear) const {
  return holidayCalendar.getStatusText(fullYear);
}

HolidayDayType AlarmManager::getHolidayDayType(const DateTime &date) {
  return holidayCalendar.getDayType(date);
}

String AlarmManager::getHolidayName(const DateTime &date) {
  return holidayCalendar.getHolidayName(date);
}

HolidayCountdown AlarmManager::getNextHolidayCountdown(const DateTime &date,
                                                       uint16_t maxDays) {
  return holidayCalendar.getNextHolidayCountdown(date, maxDays);
}

AlarmConfig AlarmManager::getAlarm(size_t index) const {
  if (!isIndexValid(index)) {
    return buildDefaultAlarm();
  }
  return alarms[index];
}

size_t AlarmManager::getAlarmCount() const { return alarms.size(); }

uint32_t AlarmManager::getNextWakeDelayMs(const DateTime &now, uint32_t nowMs) {
  if (!hasEnabledAlarms()) {
    return UINT32_MAX;
  }

  time_t nowTime = toTimeT(now);
  time_t bestWakeTime = 0;
  for (size_t i = 0; i < alarms.size(); ++i) {
    if (!alarms[i].enabled) {
      continue;
    }
    time_t candidateTime = findNextAlarmTime(alarms[i], nowTime);
    if (candidateTime == 0 || candidateTime <= nowTime) {
      continue;
    }
    if (bestWakeTime == 0 || candidateTime < bestWakeTime) {
      bestWakeTime = candidateTime;
    }
  }

  if (bestWakeTime == 0) {
    return UINT32_MAX;
  }

  uint32_t delaySeconds = static_cast<uint32_t>(bestWakeTime - nowTime);
  return (delaySeconds - 1) * 1000UL +
         WakeTiming::getMsUntilNextSecondBoundary(nowMs);
}

String AlarmManager::getRepeatText(const AlarmConfig &alarm) const {
  if (alarm.repeatType == ALARM_REPEAT_DAILY) {
    return "每天";
  }
  if (alarm.repeatType == ALARM_REPEAT_WORKDAY) {
    return "工作日";
  }
  return buildWeeklyText(alarm.weekMask);
}

bool AlarmManager::hasEnabledAlarms() const {
  for (size_t i = 0; i < alarms.size(); ++i) {
    if (alarms[i].enabled) {
      return true;
    }
  }
  return false;
}

bool AlarmManager::isRinging() const { return ringing; }

bool AlarmManager::removeAlarm(size_t index) {
  if (!isIndexValid(index)) {
    return false;
  }
  alarms.erase(alarms.begin() + index);
  save();
  return true;
}

void AlarmManager::snooze() { ringing = false; }

void AlarmManager::stop() { ringing = false; }

bool AlarmManager::toggleEnabled(size_t index) {
  if (!isIndexValid(index)) {
    return false;
  }
  alarms[index].enabled = !alarms[index].enabled;
  save();
  return true;
}

bool AlarmManager::updateAlarm(size_t index, const AlarmConfig &alarm) {
  if (!isIndexValid(index)) {
    return false;
  }
  alarms[index] = sanitizeAlarm(alarm);
  save();
  return true;
}

void AlarmManager::updateHolidayCache(const DateTime &now) {
  holidayCalendar.updateIfNeeded(now);
}

AlarmRepeatType AlarmManager::inferRepeatType(uint8_t weekMask) const {
  if (weekMask == 0x7F) {
    return ALARM_REPEAT_DAILY;
  }
  if (weekMask == 0x3E) {
    return ALARM_REPEAT_WORKDAY;
  }
  return ALARM_REPEAT_WEEKLY;
}

AlarmConfig AlarmManager::sanitizeAlarm(const AlarmConfig &alarm) const {
  AlarmConfig clean = alarm;
  clean.hour %= 24;
  clean.minute %= 60;
  clean.weekMask &= 0x7F;

  if (clean.repeatType == ALARM_REPEAT_DAILY) {
    clean.weekMask = 0x7F;
  } else if (clean.repeatType == ALARM_REPEAT_WORKDAY) {
    clean.weekMask = 0x3E;
  } else if (clean.weekMask == 0) {
    clean.weekMask = 0x02;
  }
  return clean;
}

String AlarmManager::buildWeeklyText(uint8_t weekMask) const {
  String text = "周";
  bool first = true;
  for (uint8_t day = 1; day < 7; ++day) {
    if ((weekMask & (1 << day)) == 0) {
      continue;
    }
    if (!first) {
      text += " ";
    }
    text += kWeekNames[day];
    first = false;
  }
  if (weekMask & 0x01) {
    if (!first) {
      text += " ";
    }
    text += kWeekNames[0];
  }
  return text;
}

time_t AlarmManager::findNextAlarmTime(const AlarmConfig &alarm,
                                       time_t nowTime) {
  // 关键逻辑：工作日闹钟会被春节、国庆这类长假整体顺延，
  // 只看未来 7 天会漏掉下一次真正的响铃时间，因此这里统一向后扫描 31 天。
  for (uint8_t dayOffset = 0; dayOffset <= LOOKAHEAD_DAYS; ++dayOffset) {
    time_t baseTime = nowTime + static_cast<time_t>(dayOffset) * 86400;
    DateTime candidate = toDateTime(baseTime);
    candidate.hour = alarm.hour;
    candidate.minute = alarm.minute;
    candidate.second = 0;

    if (!matchesDate(alarm, candidate)) {
      continue;
    }

    time_t candidateTime = toTimeT(candidate);
    if (candidateTime > nowTime) {
      return candidateTime;
    }
  }
  return 0;
}

bool AlarmManager::isIndexValid(size_t index) const { return index < alarms.size(); }

bool AlarmManager::matchesDate(const AlarmConfig &alarm, const DateTime &date) {
  if (alarm.repeatType == ALARM_REPEAT_DAILY) {
    return true;
  }
  if (alarm.repeatType == ALARM_REPEAT_WORKDAY) {
    return holidayCalendar.isWorkday(date);
  }
  return (alarm.weekMask & (1 << date.week)) != 0;
}

DateTime AlarmManager::toDateTime(time_t value) const {
  struct tm timeInfo = {0};
  localtime_r(&value, &timeInfo);

  DateTime date;
  date.second = timeInfo.tm_sec;
  date.minute = timeInfo.tm_min;
  date.hour = timeInfo.tm_hour;
  date.day = timeInfo.tm_mday;
  date.month = timeInfo.tm_mon + 1;
  date.year = timeInfo.tm_year % 100;
  date.week = timeInfo.tm_wday;
  return date;
}

time_t AlarmManager::toTimeT(const DateTime &date) const {
  struct tm timeInfo = {0};
  timeInfo.tm_sec = date.second;
  timeInfo.tm_min = date.minute;
  timeInfo.tm_hour = date.hour;
  timeInfo.tm_mday = date.day;
  timeInfo.tm_mon = date.month - 1;
  timeInfo.tm_year = date.year + 100;
  timeInfo.tm_isdst = -1;
  return mktime(&timeInfo);
}

uint32_t AlarmManager::toMinuteKey(const DateTime &date) const {
  return static_cast<uint32_t>(toTimeT(date) / 60);
}
