#include "RadioDriver.h"

RadioDriver::RadioDriver() {}

bool RadioDriver::init() {
  radio.setup();
  radio.setVolume(5);
  radio.setMono(false);
  radio.setMute(false);
  return true;
}

void RadioDriver::setFrequency(uint16_t freq) { radio.setFrequency(freq); }

void RadioDriver::setVolume(uint8_t vol) { radio.setVolume(vol); }

void RadioDriver::mute(bool m) { radio.setMute(m); }

void RadioDriver::seekUp() { radio.seek(RDA_SEEK_WRAP, RDA_SEEK_UP); }

void RadioDriver::seekDown() { radio.seek(RDA_SEEK_WRAP, RDA_SEEK_DOWN); }

uint16_t RadioDriver::getFrequency() { return radio.getFrequency(); }
