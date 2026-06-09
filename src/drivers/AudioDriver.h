#pragma once

#include "../config.h"
#include <Arduino.h>
#include <Audio.h>
#include <FS.h>
#include <Wire.h>

class AudioDriver {
public:
  AudioDriver();
  ~AudioDriver();
  bool init();
  void end();
  void setVolume(uint8_t vol);
  void playFromFS(fs::FS &fs, const char *path);
  void playFromSD(const char *path);
  void stop();
  void pause();
  void resume();
  void loop();
  bool isPlaying();
  uint32_t getElapsed();
  uint32_t getDuration();

private:
  bool ensureAudio();
  void initES8311();
  void writeES8311(uint8_t reg, uint8_t val);
  void writeES8311Locked(uint8_t reg, uint8_t val);
  Audio *audio = nullptr;
  uint8_t currentVolume = 10;
  const uint8_t ES8311_ADDR = 0x18; // Standard address
};
