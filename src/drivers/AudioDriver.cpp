#include "AudioDriver.h"
#include "../managers/BusManager.h"

AudioDriver::AudioDriver() {}

bool AudioDriver::init() {
  digitalWrite(AMP_EN, LOW);
  // Initialize ES8311 Codec
  // initES8311();

  // Initialize I2S Audio
  audio.setPinout(I2S_BCK, I2S_WS, I2S_DOUT);
  audio.setVolume(10); // Default volume
  return true;
}

void AudioDriver::setVolume(uint8_t vol) {
  audio.setVolume(vol);
  // Also update ES8311 volume if needed, but I2S digital volume is usually
  // enough writeES8311(0x32, vol);
}

void AudioDriver::play(const char *path) { audio.connecttoFS(SPIFFS, path); }

void AudioDriver::stop() { audio.stopSong(); }

void AudioDriver::pause() { audio.pauseResume(); }

void AudioDriver::resume() { audio.pauseResume(); }

void AudioDriver::loop() { audio.loop(); }

bool AudioDriver::isPlaying() { return audio.isRunning(); }

void AudioDriver::initES8311() {
  BusManager::getInstance().requestI2C();
  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ES8311 not found!");
    return;
  }

  // Basic ES8311 Init Sequence
  writeES8311(0x00, 0x80); // Reset
  delay(10);
  writeES8311(0x00, 0x00); // Wake up

  // Clock Config
  writeES8311(0x01, 0x30); // MCLK / 1
  writeES8311(0x02, 0x00); // MCLK / 1

  // DAC Config
  writeES8311(0x14, 0x1A); // DAC Volume (0x1A = 0dB)
  writeES8311(0x12, 0x00); // DAC Power On
  writeES8311(0x13, 0x00); // System Power On

  // Enable I2S
  writeES8311(0x09, 0x0C); // I2S Mode, 16bit
  writeES8311(0x0A, 0x00); // I2S Format

  // Unmute
  writeES8311(0x32, 0xBF); // Max Volume
}

void AudioDriver::writeES8311(uint8_t reg, uint8_t val) {
  BusManager::getInstance().requestI2C();
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}
