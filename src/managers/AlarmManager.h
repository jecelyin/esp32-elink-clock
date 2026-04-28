#pragma once

#include "../drivers/RtcDriver.h"
#include <vector>

struct Alarm {
  uint8_t hour;
  uint8_t minute;
  bool enabled;
  uint8_t days; // Bitmask: Bit 0=Sun, 1=Mon, ... 6=Sat. 0x7F = Everyday
  uint32_t lastTriggeredMinuteKey;
};

class AlarmManager {
public:
  AlarmManager();
  void addAlarm(uint8_t h, uint8_t m, uint8_t days = 0x7F);
  void check(DateTime now);
  bool hasEnabledAlarms() const;
  uint32_t getNextWakeDelayMs(DateTime now, uint32_t nowMs) const;
  bool isRinging();
  void snooze();
  void stop();

  std::vector<Alarm> alarms;

private:
  bool ringing;
  unsigned long lastCheck;
};
