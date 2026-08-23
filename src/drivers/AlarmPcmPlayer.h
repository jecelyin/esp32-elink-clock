#pragma once

#include <Arduino.h>
#include <FS.h>

class AlarmPcmPlayer {
public:
  bool begin(fs::FS &fs, const char *path);
  void loop();
  void stop();
  bool isPlaying() const;

private:
  bool parseWav();
  bool installI2S();
  bool restartData();
  bool readExact(uint8_t *buffer, size_t length);

  File file;
  uint32_t sampleRate = 0;
  uint32_t dataOffset = 0;
  uint32_t dataSize = 0;
  uint32_t dataRemaining = 0;
  uint32_t sourceBytesPlayed = 0;
  uint32_t loopCount = 0;
  bool active = false;
  bool i2sInstalled = false;
  bool firstWriteLogged = false;

  static constexpr size_t MONO_SAMPLES_PER_CHUNK = 512;
  int16_t monoBuffer[MONO_SAMPLES_PER_CHUNK];
  int16_t stereoBuffer[MONO_SAMPLES_PER_CHUNK * 2];
};
