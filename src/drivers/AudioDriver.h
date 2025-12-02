#pragma once

#include "../config.h"
#include <Audio.h>
#include <Wire.h>

class AudioDriver {
public:
  AudioDriver();
  bool init();
  void setVolume(uint8_t vol);
  void play(const char *path);
  void stop();
  void pause();
  void resume();
  void loop();
  bool isPlaying();

  Audio audio; // Public to allow callback access if needed

private:
  void initES8311();
  void writeES8311(uint8_t reg, uint8_t val);
  const uint8_t ES8311_ADDR = 0x18; // Standard address
};
