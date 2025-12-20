#include "RadioDriver.h"
#include "../managers/BusManager.h"

RadioDriver::RadioDriver() {}

bool RadioDriver::init() {
  digitalWrite(BIAS_CTR, LOW);
  biasState = false;
  return true;
}

void RadioDriver::setup() {
  BusManager::getInstance().requestI2C();
  radio.setup(CLOCK_32K, OSCILLATOR_TYPE_ACTIVE);
  radio.setLnaPortSel(3); // Improve sensitivity
  radio.setAFC(true);     // Automatic Frequency Control
  radio.setMute(false);
  radio.setVolume(15);
  radio.setMono(false);
}

void RadioDriver::powerDown() {
  BusManager::getInstance().requestI2C();
  radio.powerDown();
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
  uint16_t freq = radio.getFrequency();
  return freq;
}

char *RadioDriver::getFormattedFrequency() {
  BusManager::getInstance().requestI2C();
  char *f = radio.formatCurrentFrequency('.');
  return f;
}

uint16_t RadioDriver::getMinFrequency() {
  BusManager::getInstance().requestI2C();
  uint16_t f = radio.getMinimumFrequencyOfTheBand();
  return f;
}

uint16_t RadioDriver::getMaxFrequency() {
  BusManager::getInstance().requestI2C();
  uint16_t f = radio.getMaximunFrequencyOfTheBand();
  return f;
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
  uint8_t rssi = radio.getRssi();
  return rssi;
}

bool RadioDriver::hasRdsInfo() {
  BusManager::getInstance().requestI2C();
  bool has = radio.hasRdsInfo();
  return has;
}

String RadioDriver::getRdsStationName() {
  BusManager::getInstance().requestI2C();
  char *s = radio.getRdsStationName();
  String res = s ? String(s) : String("");
  return res;
}

String RadioDriver::getRdsProgramInformation() {
  BusManager::getInstance().requestI2C();
  char *s = radio.getRdsProgramInformation();
  String res = s ? String(s) : String("");
  return res;
}

void RadioDriver::clearRds() {
  BusManager::getInstance().requestI2C();
  radio.clearRdsFifo();
  radio.clearRdsBuffer();
}

bool RadioDriver::isStereo() {
  BusManager::getInstance().requestI2C();
  bool s = radio.isStereo();
  return s;
}

void RadioDriver::setBias(bool on) {
  digitalWrite(BIAS_CTR, on ? HIGH : LOW);
  biasState = on;
}

bool RadioDriver::getBias() { return biasState; }
