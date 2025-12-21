#pragma once

#include "../config.h"
#include "RDA5807M.h"

class RadioDriver {
public:
  RadioDriver();
  bool init();
  void setup();
  void powerDown();
  void setFrequency(uint16_t freq); // freq in 10kHz (e.g. 10110 for 101.1 MHz)
  void setVolume(uint8_t vol);
  uint8_t getVolume();
  void mute(bool m);
  void seekUp();
  void seekDown();
  uint16_t getFrequency();
  void getFormattedFrequency(char *s, uint8_t length);
  uint16_t getMinFrequency();
  uint16_t getMaxFrequency();
  void setBias(bool on);
  bool getBias();
  void debugRadioInfo();
  void getRadioInfo(RADIO_INFO *info);
  void getAudioInfo(AUDIO_INFO *info);
private:
  RDA5807M radio;
  bool biasState;
};
