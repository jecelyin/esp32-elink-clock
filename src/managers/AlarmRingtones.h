#pragma once

#include <Arduino.h>

namespace AlarmRingtones {

struct Entry {
  const char *value;
  const char *label;
};

static const Entry BUILTIN[] = {
    {"spiffs:/alarm.wav", "经典闹铃"},
    {"spiffs:/gentle-chime.wav", "轻柔风铃"},
    {"spiffs:/kanong.wav", "卡农"},
    {"spiffs:/gugu.wav", "布谷鸟"},
};

static const size_t BUILTIN_COUNT = sizeof(BUILTIN) / sizeof(BUILTIN[0]);
static const char *const DEFAULT_VALUE = BUILTIN[0].value;
static const char *const LEGACY_BUILTIN_VALUES[] = {
    "spiffs:/alarm.mp3",
    "spiffs:/gentle-chime.mp3",
    "spiffs:/kanong.mp3",
    "spiffs:/gugu.mp3",
};
static_assert(sizeof(LEGACY_BUILTIN_VALUES) /
                      sizeof(LEGACY_BUILTIN_VALUES[0]) ==
                  BUILTIN_COUNT,
              "Legacy and current ringtone tables must stay aligned");

inline String normalize(const String &value) {
  for (size_t i = 0; i < BUILTIN_COUNT; ++i) {
    if (value == LEGACY_BUILTIN_VALUES[i]) {
      return String(BUILTIN[i].value);
    }
  }
  return value;
}

inline bool isBuiltin(const String &value) {
  for (size_t i = 0; i < BUILTIN_COUNT; ++i) {
    if (value == BUILTIN[i].value) {
      return true;
    }
  }
  return false;
}

} // namespace AlarmRingtones
