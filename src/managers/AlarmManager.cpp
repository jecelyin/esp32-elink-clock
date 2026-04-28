#include "AlarmManager.h"
#include "../utils/WakeTiming.h"

namespace {
time_t toTimeT(const DateTime &dt) {
  struct tm timeInfo = {0};
  timeInfo.tm_sec = dt.second;
  timeInfo.tm_min = dt.minute;
  timeInfo.tm_hour = dt.hour;
  timeInfo.tm_mday = dt.day;
  timeInfo.tm_mon = dt.month - 1;
  timeInfo.tm_year = dt.year + 100;
  timeInfo.tm_isdst = -1;
  return mktime(&timeInfo);
}

uint32_t toMinuteKey(const DateTime &dt) {
  return static_cast<uint32_t>(toTimeT(dt) / 60);
}
} // namespace

AlarmManager::AlarmManager() {
  ringing = false;
  lastCheck = 0;
}

void AlarmManager::addAlarm(uint8_t h, uint8_t m, uint8_t days) {
  Alarm a = {h, m, true, days, 0};
  alarms.push_back(a);
}

void AlarmManager::check(DateTime now) {
  if (millis() - lastCheck < 1000)
    return; // Check once per second
  lastCheck = millis();

  if (ringing)
    return; // Already ringing

  uint32_t currentMinuteKey = toMinuteKey(now);
  for (auto &alarm : alarms) {
    bool dayMatched = alarm.days & (1 << now.week);
    bool timeMatched =
        alarm.hour == now.hour && alarm.minute == now.minute;

    // 关键逻辑：用“分钟时间戳”去重，而不是依赖下一分钟再手动复位，
    // 这样主循环就可以直接睡到下一次真实闹钟时间，而不用每分钟试探性唤醒。
    if (alarm.enabled && dayMatched && timeMatched &&
        alarm.lastTriggeredMinuteKey != currentMinuteKey) {
      ringing = true;
      alarm.lastTriggeredMinuteKey = currentMinuteKey;
      return;
    }
  }
}

bool AlarmManager::hasEnabledAlarms() const {
  for (const auto &alarm : alarms) {
    if (alarm.enabled) {
      return true;
    }
  }
  return false;
}

uint32_t AlarmManager::getNextWakeDelayMs(DateTime now, uint32_t nowMs) const {
  if (!hasEnabledAlarms()) {
    return UINT32_MAX;
  }

  time_t nowTime = toTimeT(now);
  time_t bestWakeTime = 0;
  for (const auto &alarm : alarms) {
    if (!alarm.enabled) {
      continue;
    }

    for (uint8_t dayOffset = 0; dayOffset < 8; ++dayOffset) {
      uint8_t targetWeek = (now.week + dayOffset) % 7;
      if ((alarm.days & (1 << targetWeek)) == 0) {
        continue;
      }

      struct tm candidateInfo = {0};
      candidateInfo.tm_sec = 0;
      candidateInfo.tm_min = alarm.minute;
      candidateInfo.tm_hour = alarm.hour;
      candidateInfo.tm_mday = now.day + dayOffset;
      candidateInfo.tm_mon = now.month - 1;
      candidateInfo.tm_year = now.year + 100;
      candidateInfo.tm_isdst = -1;
      time_t candidateTime = mktime(&candidateInfo);
      if (candidateTime <= nowTime) {
        continue;
      }

      if (bestWakeTime == 0 || candidateTime < bestWakeTime) {
        bestWakeTime = candidateTime;
      }
      break;
    }
  }

  if (bestWakeTime == 0) {
    return UINT32_MAX;
  }

  uint32_t delaySeconds = static_cast<uint32_t>(bestWakeTime - nowTime);
  return (delaySeconds - 1) * 1000UL +
         WakeTiming::getMsUntilNextSecondBoundary(nowMs);
}

bool AlarmManager::isRinging() { return ringing; }

void AlarmManager::snooze() {
  ringing = false;
  // Add snooze logic (e.g. add a temp alarm 5 mins later)
}

void AlarmManager::stop() { ringing = false; }
