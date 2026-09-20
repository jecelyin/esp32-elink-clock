#include "AlarmManager.h"
#include "../utils/WakeTiming.h"
#include <ArduinoJson.h>

namespace {
const char *kWeekNames[] = {"日", "一", "二", "三", "四", "五", "六"};
}

AlarmManager::AlarmManager() {
  ringing = false;
  prefsReady = false;
  lastCheck = 0;
  lastCheckedMinuteKey = 0;
  triggerSequence = 0;
}

void AlarmManager::begin(ConfigManager *config) {
  prefsReady = prefs.begin("alarms", false);
  if (!prefsReady) {
    Serial.println("Alarm prefs open failed");
    return;
  }
  holidayCalendar.begin(config);
  load();
}

bool AlarmManager::addAlarm(const AlarmConfig &alarm) {
  alarms.push_back(sanitizeAlarm(alarm));
  if (save()) {
    return true;
  }
  alarms.pop_back();
  return false;
}

void AlarmManager::addStartupTestAlarm(const DateTime &now,
                                       uint8_t delayMinutes) {
  if (delayMinutes == 0) {
    delayMinutes = 1;
  }

  uint16_t targetMinute =
      static_cast<uint16_t>(now.hour) * 60U + now.minute + delayMinutes;
  AlarmConfig alarm = buildDefaultAlarm();
  alarm.hour = (targetMinute / 60U) % 24U;
  alarm.minute = targetMinute % 60U;
  alarm.transient = true;
  alarms.push_back(alarm);

  Serial.printf(
      "[Alarm][test] one-shot alarm added for %02u:%02u "
      "(now=%02u:%02u:%02u, delay=%u min)\n",
      alarm.hour, alarm.minute, now.hour, now.minute, now.second,
      delayMinutes);
}

AlarmConfig AlarmManager::buildDefaultAlarm() const {
  AlarmConfig alarm;
  alarm.hour = 7;
  alarm.minute = 30;
  alarm.enabled = true;
  // 新建闹钟默认每天响，避免周末临时测试时因“工作日”默认值被静默跳过。
  alarm.repeatType = ALARM_REPEAT_DAILY;
  alarm.weekMask = 0x7F;
  return alarm;
}

void AlarmManager::check(const DateTime &now) {
  if (millis() - lastCheck < CHECK_INTERVAL_MS) {
    return;
  }
  lastCheck = millis();

  uint32_t currentMinuteKey = toMinuteKey(now);
  uint32_t firstMinuteKey = currentMinuteKey;
  if (lastCheckedMinuteKey != 0 && currentMinuteKey > lastCheckedMinuteKey &&
      currentMinuteKey - lastCheckedMinuteKey <=
          MISSED_TRIGGER_GRACE_MINUTES) {
    firstMinuteKey = lastCheckedMinuteKey + 1;
  }
  lastCheckedMinuteKey = currentMinuteKey;

  // Check newest-to-oldest. If a display refresh or network operation delayed
  // the main loop across a minute boundary, a recently missed alarm still
  // rings, while an hours-old alarm never fires after boot or a clock jump.
  for (uint32_t minuteKey = currentMinuteKey;; --minuteKey) {
    DateTime candidate =
        minuteKey == currentMinuteKey
            ? now
            : toDateTime(static_cast<time_t>(minuteKey) * 60);
    for (size_t i = 0; i < alarms.size(); ++i) {
      AlarmConfig &alarm = alarms[i];
      if (!alarm.enabled || !matchesDate(alarm, candidate)) {
        continue;
      }
      if (alarm.hour != candidate.hour || alarm.minute != candidate.minute) {
        continue;
      }
      if (alarm.lastTriggeredMinuteKey == minuteKey) {
        continue;
      }

      // 关键逻辑：用“分钟级时间戳”去重，确保同一分钟内多次唤醒、
      // 或者页面刷新重复调用 check() 时，不会把同一个闹钟连响多次。
      bool replacingActiveAlarm = ringing;
      ringing = true;
      activeRingtone = alarm.ringtone;
      alarm.lastTriggeredMinuteKey = minuteKey;
      ++triggerSequence;
      // 启动测试闹钟只触发一次，避免设备连续运行时第二天重复响铃。
      if (alarm.transient) {
        alarm.enabled = false;
      }
      Serial.printf("[Alarm] triggered index=%u time=%02u:%02u ringtone=%s "
                    "sequence=%lu%s%s\n",
                    static_cast<unsigned>(i), candidate.hour,
                    candidate.minute, activeRingtone.c_str(),
                    static_cast<unsigned long>(triggerSequence),
                    replacingActiveAlarm ? " replacing-active" : "",
                    minuteKey == currentMinuteKey ? "" : " delayed");
      return;
    }
    if (minuteKey == firstMinuteKey) {
      break;
    }
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

uint32_t AlarmManager::getTriggerSequence() const { return triggerSequence; }

bool AlarmManager::removeAlarm(size_t index) {
  if (!isIndexValid(index)) {
    return false;
  }
  AlarmConfig removed = alarms[index];
  alarms.erase(alarms.begin() + index);
  if (save()) {
    return true;
  }
  alarms.insert(alarms.begin() + index, removed);
  return false;
}

void AlarmManager::snooze() {
  ringing = false;
  activeRingtone = "";
}

void AlarmManager::stop() {
  ringing = false;
  activeRingtone = "";
}

bool AlarmManager::toggleEnabled(size_t index) {
  if (!isIndexValid(index)) {
    return false;
  }
  alarms[index].enabled = !alarms[index].enabled;
  if (save()) {
    return true;
  }
  alarms[index].enabled = !alarms[index].enabled;
  return false;
}

bool AlarmManager::updateAlarm(size_t index, const AlarmConfig &alarm) {
  if (!isIndexValid(index)) {
    return false;
  }
  AlarmConfig previous = alarms[index];
  alarms[index] = sanitizeAlarm(alarm);
  if (save()) {
    return true;
  }
  alarms[index] = previous;
  return false;
}

void AlarmManager::updateHolidayCache(const DateTime &now) {
  holidayCalendar.updateIfNeeded(now);
}

String AlarmManager::getActiveRingtone() const {
  return activeRingtone.length() > 0 ? activeRingtone
                                    : AlarmRingtones::DEFAULT_VALUE;
}

String AlarmManager::getAlarmsJSON() const {
  JsonDocument doc;
  JsonArray items = doc.to<JsonArray>();
  for (size_t i = 0; i < alarms.size(); ++i) {
    // 启动测试闹钟不暴露给 Web 持久化表单，否则用户点击“保存”时
    // 会把本应一次性的闹钟写入 NVS。
    if (alarms[i].transient) {
      continue;
    }
    JsonObject item = items.add<JsonObject>();
    item["h"] = alarms[i].hour;
    item["m"] = alarms[i].minute;
    item["e"] = alarms[i].enabled;
    item["r"] = alarms[i].repeatType;
    item["w"] = alarms[i].weekMask;
    item["s"] = alarms[i].ringtone;
  }

  String json;
  serializeJson(doc, json);
  return json;
}

bool AlarmManager::saveAlarmsFromJSON(const String &json) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error || !doc.is<JsonArray>() || doc.size() > 20) {
    Serial.printf("Alarm config parse failed: %s\n", error.c_str());
    return false;
  }

  std::vector<AlarmConfig> updated;
  for (JsonObject item : doc.as<JsonArray>()) {
    AlarmConfig alarm;
    int hour = item["h"] | -1;
    int minute = item["m"] | -1;
    int repeat = item["r"] | -1;
    int weekMask = item["w"] | -1;
    String ringtone = item["s"] | AlarmRingtones::DEFAULT_VALUE;
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        repeat < ALARM_REPEAT_DAILY || repeat > ALARM_REPEAT_WORKDAY ||
        weekMask < 0 || weekMask > 0x7F ||
        (repeat == ALARM_REPEAT_WEEKLY && weekMask == 0) ||
        ringtone.length() > 96 || ringtone.indexOf("..") >= 0) {
      return false;
    }
    if (!ringtone.startsWith("sd:/") && !AlarmRingtones::isBuiltin(ringtone)) {
      return false;
    }
    alarm.hour = hour;
    alarm.minute = minute;
    alarm.enabled = item["e"] | true;
    alarm.repeatType = static_cast<AlarmRepeatType>(repeat);
    alarm.weekMask = static_cast<uint8_t>(weekMask);
    alarm.ringtone = ringtone;
    updated.push_back(sanitizeAlarm(alarm));
  }

  std::vector<AlarmConfig> previous = alarms;
  alarms = updated;
  if (save()) {
    return true;
  }
  alarms = previous;
  return false;
}

void AlarmManager::resetHolidayFetchState() {
  holidayCalendar.resetFetchState();
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
  clean.ringtone = AlarmRingtones::normalize(clean.ringtone);
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
  if (clean.ringtone.length() == 0) {
    clean.ringtone = AlarmRingtones::DEFAULT_VALUE;
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
