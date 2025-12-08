#include "RadioDriver.h"
#include "../managers/BusManager.h"

RadioDriver::RadioDriver() {}

bool RadioDriver::init() {
  BusManager::getInstance().requestI2C();
  radio.setup();
  radio.setVolume(5);
  radio.setMono(false);
  radio.setMute(false);
  return true;
}

void RadioDriver::setFrequency(uint16_t freq) { 
  BusManager::getInstance().requestI2C();
  radio.setFrequency(freq); 
}

void RadioDriver::setVolume(uint8_t vol) { 
  BusManager::getInstance().requestI2C();
  radio.setVolume(vol); 
}

void RadioDriver::mute(bool m) { 
  BusManager::getInstance().requestI2C();
  radio.setMute(m); 
}

void RadioDriver::seekUp() { 
  BusManager::getInstance().requestI2C();
  radio.seek(RDA_SEEK_WRAP, RDA_SEEK_UP); 
}

void RadioDriver::seekDown() { 
  BusManager::getInstance().requestI2C();
  radio.seek(RDA_SEEK_WRAP, RDA_SEEK_DOWN); 
}

uint16_t RadioDriver::getFrequency() { 
  BusManager::getInstance().requestI2C();
  return radio.getFrequency(); 
}
