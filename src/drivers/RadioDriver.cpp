#include "RadioDriver.h"
#include "../managers/BusManager.h"

RadioDriver::RadioDriver() {}

bool RadioDriver::init() {
  pinMode(BIAS_CTR, OUTPUT);
  digitalWrite(BIAS_CTR, LOW);
  biasState = false;
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

char *RadioDriver::getFormattedFrequency() {
  BusManager::getInstance().requestI2C();
  return radio.formatCurrentFrequency('.');
}

uint16_t RadioDriver::getMinFrequency() {
  BusManager::getInstance().requestI2C();
  return radio.getMinimumFrequencyOfTheBand();
}

uint16_t RadioDriver::getMaxFrequency() {
  BusManager::getInstance().requestI2C();
  return radio.getMaximunFrequencyOfTheBand();
}

uint8_t RadioDriver::getSignalStrength() {
  BusManager::getInstance().requestI2C();
  // RSSI is usually 0-127 or similar. Let's map it to 0-4.
  // getRssi() returns current Signal Strength
  uint8_t rssi = radio.getRssi();
  if (rssi > 50)
    return 4;
  if (rssi > 40)
    return 3;
  if (rssi > 30)
    return 2;
  if (rssi > 15)
    return 1;
  return 0;
}

uint8_t RadioDriver::getRSSI() {
  BusManager::getInstance().requestI2C();
  return radio.getRssi();
}

bool RadioDriver::hasRdsInfo() {
  BusManager::getInstance().requestI2C();
  return radio.hasRdsInfo();
}

String RadioDriver::getRdsStationName() {
  BusManager::getInstance().requestI2C();
  char *s = radio.getRdsStationName();
  return s ? String(s) : String("");
}

String RadioDriver::getRdsProgramInformation() {
  BusManager::getInstance().requestI2C();
  char *s = radio.getRdsProgramInformation();
  return s ? String(s) : String("");
}

void RadioDriver::clearRds() {
  BusManager::getInstance().requestI2C();
  radio.clearRdsFifo();
  radio.clearRdsBuffer();
}

bool RadioDriver::isStereo() {
  BusManager::getInstance().requestI2C();
  return radio.isStereo();
}

void RadioDriver::setBias(bool on) {
  digitalWrite(BIAS_CTR, on ? HIGH : LOW);
  biasState = on;
}

bool RadioDriver::getBias() { return biasState; }
