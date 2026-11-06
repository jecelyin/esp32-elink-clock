#pragma once

#include <stdint.h>

struct AlarmSoundSettings {
  static constexpr uint8_t MAX_VOLUME = 21;
  static constexpr uint8_t MAX_FADE_IN_SECONDS = 60;

  uint8_t volume = MAX_VOLUME;
  uint8_t fadeInSeconds = 3;

  bool isValid() const {
    return volume <= MAX_VOLUME && fadeInSeconds <= MAX_FADE_IN_SECONDS;
  }
};
