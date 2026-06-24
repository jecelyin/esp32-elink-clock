#pragma once

#include <Arduino.h>

enum AlarmRepeatType : uint8_t {
  ALARM_REPEAT_DAILY = 0,
  ALARM_REPEAT_WEEKLY = 1,
  ALARM_REPEAT_WORKDAY = 2,
};

struct AlarmConfig {
  uint8_t hour = 7;
  uint8_t minute = 0;
  bool enabled = true;
  AlarmRepeatType repeatType = ALARM_REPEAT_DAILY;
  uint8_t weekMask = 0x7F;
  String ringtone = "spiffs:/alarm.mp3";
  uint32_t lastTriggeredMinuteKey = 0;
};
