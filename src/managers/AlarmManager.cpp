#include "AlarmManager.h"

AlarmManager::AlarmManager() {
  ringing = false;
  lastCheck = 0;
}

void AlarmManager::addAlarm(uint8_t h, uint8_t m, uint8_t days) {
  Alarm a = {h, m, true, days, false};
  alarms.push_back(a);
}

void AlarmManager::check(DateTime now) {
  if (millis() - lastCheck < 1000)
    return; // Check once per second
  lastCheck = millis();

  if (ringing)
    return; // Already ringing

  for (auto &alarm : alarms) {
    if (alarm.enabled) {
      // Check day
      if (alarm.days & (1 << now.week)) {
        if (alarm.hour == now.hour && alarm.minute == now.minute &&
            now.second == 0) {
          ringing = true;
          alarm.triggered = true;
          // Trigger sound here or via callback
        }
      }
    }
  }
}

bool AlarmManager::isRinging() { return ringing; }

void AlarmManager::snooze() {
  ringing = false;
  // Add snooze logic (e.g. add a temp alarm 5 mins later)
}

void AlarmManager::stop() { ringing = false; }
