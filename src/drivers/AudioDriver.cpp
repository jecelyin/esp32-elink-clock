#include "AudioDriver.h"
#include "../utils/I2CBus.h"
#include <SD.h>

AudioDriver::AudioDriver() {}

AudioDriver::~AudioDriver() { end(); }

bool AudioDriver::init() {
  return ensureAudio();
}

void AudioDriver::end() {
  if (audio) {
    audio->stopSong();
    delete audio;
    audio = nullptr;
  }
  digitalWrite(AMP_EN, LOW);
  I2CBus::powerDownSharedBus();
}

void AudioDriver::setVolume(uint8_t vol) {
  currentVolume = vol;
  if (audio)
    audio->setVolume(currentVolume);
  // Also update ES8311 volume if needed, but I2S digital volume is usually
  // enough writeES8311(0x32, vol);
}

void AudioDriver::playFromFS(fs::FS &fs, const char *path) {
  if (!ensureAudio())
    return;

  digitalWrite(CODEC_EN, HIGH);
  digitalWrite(AMP_EN, HIGH);
  audio->connecttoFS(fs, path);
}

void AudioDriver::playFromSD(const char *path) { playFromFS(SD, path); }

void AudioDriver::stop() { end(); }

void AudioDriver::pause() {
  if (audio)
    audio->pauseResume();
}

void AudioDriver::resume() {
  if (audio)
    audio->pauseResume();
}

void AudioDriver::loop() {
  if (audio)
    audio->loop();
}

bool AudioDriver::isPlaying() { return audio && audio->isRunning(); }

uint32_t AudioDriver::getElapsed() {
  return audio ? audio->getAudioCurrentTime() : 0;
}

uint32_t AudioDriver::getDuration() {
  return audio ? audio->getAudioFileDuration() : 0;
}

bool AudioDriver::ensureAudio() {
  if (audio)
    return true;

  audio = new Audio();
  audio->setPinout(I2S_BCK, I2S_WS, I2S_DOUT);
  audio->setVolume(currentVolume);
  return true;
}

void AudioDriver::initES8311() {
  I2CBus::Guard guard;
  if (!guard.isLocked()) {
    Serial.println("ES8311 init skipped: I2C lock failed");
    return;
  }

  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ES8311 not found!");
    return;
  }

  // Basic ES8311 Init Sequence
  // 关键逻辑：初始化阶段已经持有 I2C 锁，后续寄存器写入必须复用当前锁，
  // 不能再次创建 Guard，否则会对同一把非递归互斥锁重复加锁并导致写入失败。
  writeES8311Locked(0x00, 0x80); // Reset
  delay(10);
  writeES8311Locked(0x00, 0x00); // Wake up

  // Clock Config
  writeES8311Locked(0x01, 0x30); // MCLK / 1
  writeES8311Locked(0x02, 0x00); // MCLK / 1

  // DAC Config
  writeES8311Locked(0x14, 0x1A); // DAC Volume (0x1A = 0dB)
  writeES8311Locked(0x12, 0x00); // DAC Power On
  writeES8311Locked(0x13, 0x00); // System Power On

  // Enable I2S
  writeES8311Locked(0x09, 0x0C); // I2S Mode, 16bit
  writeES8311Locked(0x0A, 0x00); // I2S Format

  // Unmute
  writeES8311Locked(0x32, 0xBF); // Max Volume
}

void AudioDriver::writeES8311(uint8_t reg, uint8_t val) {
  I2CBus::Guard guard;
  if (!guard.isLocked()) {
    Serial.println("ES8311 write skipped: I2C lock failed");
    return;
  }

  writeES8311Locked(reg, val);
}

void AudioDriver::writeES8311Locked(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}
