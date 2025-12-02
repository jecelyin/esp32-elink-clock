#pragma once

#include "../config.h"
#include <RDA5807.h>

class RadioDriver {
public:
  RadioDriver();
  bool init();
  void setFrequency(uint16_t freq); // freq in 10kHz (e.g. 10110 for 101.1 MHz)
  void setVolume(uint8_t vol);
  void mute(bool m);
  void seekUp();
  void seekDown();
  uint16_t getFrequency();

private:
  RDA5807 radio;
};
