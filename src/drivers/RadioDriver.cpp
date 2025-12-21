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
  // Enable information to the Serial port
  radio.debugEnable(true);
  radio._wireDebug(false);

  // Set FM Options for Europe
  // radio.setup(RADIO_FMSPACING, RADIO_FMSPACING_100);   // for EUROPE
  // radio.setup(RADIO_DEEMPHASIS, RADIO_DEEMPHASIS_50);  // for EUROPE

  // Initialize the Radio
  if (!radio.initWire(Wire)) {
    Serial.println("no radio chip found.");
    delay(4000);
    ESP.restart();
  };

  // Set all radio setting to the fixed values.
  radio.setBandFrequency(RADIO_BAND_FM, 8930);

  radio.setVolume(2);
  radio.setMono(false);
  radio.setMute(false);
}

void RadioDriver::powerDown() {
  BusManager::getInstance().requestI2C();
  radio.term();
}

void RadioDriver::setFrequency(uint16_t freq) {
  BusManager::getInstance().requestI2C();
  radio.setFrequency(freq);
}

void RadioDriver::setVolume(uint8_t vol) {
  BusManager::getInstance().requestI2C();
  radio.setVolume(vol);
}

uint8_t RadioDriver::getVolume() {
  BusManager::getInstance().requestI2C();
  return radio.getVolume();
}

void RadioDriver::mute(bool m) {
  BusManager::getInstance().requestI2C();
  radio.setMute(m);
}

void RadioDriver::seekUp() {
  BusManager::getInstance().requestI2C();
  radio.seekUp(false);
}

void RadioDriver::seekDown() {
  BusManager::getInstance().requestI2C();
  radio.seekDown(false);
}

uint16_t RadioDriver::getFrequency() {
  BusManager::getInstance().requestI2C();
  uint16_t freq = radio.getFrequency();
  return freq;
}

void RadioDriver::getFormattedFrequency(char *s, uint8_t length) {
  BusManager::getInstance().requestI2C();
  radio.formatFrequency(s, length);
}

uint16_t RadioDriver::getMinFrequency() {
  BusManager::getInstance().requestI2C();
  uint16_t f = radio.getMinFrequency();
  return f;
}

uint16_t RadioDriver::getMaxFrequency() {
  BusManager::getInstance().requestI2C();
  uint16_t f = radio.getMaxFrequency();
  return f;
}


void RadioDriver::setBias(bool on) {
  digitalWrite(BIAS_CTR, on ? HIGH : LOW);
  biasState = on;
}

bool RadioDriver::getBias() { return biasState; }

void RadioDriver::debugRadioInfo() {
  BusManager::getInstance().requestI2C();
  Serial.print("Frequency: ");
  Serial.print(radio.getFrequency());
  Serial.print(" MHz Radio:");
  radio.debugRadioInfo();

  Serial.print("Audio:");
  radio.debugAudioInfo();
}

void RadioDriver::getRadioInfo(RADIO_INFO *info) {
  BusManager::getInstance().requestI2C();
  radio.getRadioInfo(info);
}

void RadioDriver::getAudioInfo(AUDIO_INFO *info) {
  BusManager::getInstance().requestI2C();
  radio.getAudioInfo(info);
}
