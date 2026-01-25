#include "RadioDriver.h"

static RDSParser *g_rdsParser = nullptr;
static void rdsCallback(uint16_t b1, uint16_t b2, uint16_t b3, uint16_t b4) {
  if (g_rdsParser)
    g_rdsParser->decode(b1, b2, b3, b4);
}

RadioDriver::RadioDriver() { g_rdsParser = &rdsParser; }

bool RadioDriver::init() {
  // digitalWrite(BIAS_CTR, LOW);
  biasState = false;
  return true;
}

void RadioDriver::setup() {
  // Enable information to the Serial port
  radio.debugEnable(true);

  // Initialize the Radio
  if (!radio.init()) {
    Serial.println("no radio chip found.");
    delay(4000);
    ESP.restart();
  };

  // Set all radio setting to the fixed values.
  radio.setBand(RDA5807M_BAND_FM);
  // radio.setFrequency(8930); // 89.3 MHz

  radio.setVolume(2);
  radio.setMono(false);
  radio.setMute(false);
  radio.attachReceiveRDS(rdsCallback);

  // Actually, lambdas without capture cannot be converted to function pointers
  // easily if state is needed. But RDA5807M's attachReceiveRDS takes a function
  // pointer. Let's use a static helper or a friend relationship. Simplest: just
  // use the instance since there's only one.
}

void RadioDriver::powerDown() { radio.term(); }

void RadioDriver::setFrequency(uint16_t freq) {
  rdsParser.reset();
  radio.setFrequency(freq);
}

void RadioDriver::setVolume(uint8_t vol) { radio.setVolume(vol); }

uint8_t RadioDriver::getVolume() { return radio.getVolume(); }

void RadioDriver::mute(bool m) { radio.setMute(m); }

void RadioDriver::seekUp() { radio.seekUp(false); }

void RadioDriver::seekDown() { radio.seekDown(false); }

uint16_t RadioDriver::getFrequency() { return radio.getFrequency(); }

void RadioDriver::getFormattedFrequency(char *s, uint8_t length) {
  uint16_t freq = radio.getFrequency();
  // Format as "101.1" etc.
  // freq is in 10kHz units (e.g. 10110) / 100 = 101.1
  sprintf(s, "%d.%d", freq / 100, (freq % 100) / 10);
}

uint16_t RadioDriver::getMinFrequency() {
  // FM Standard: 87.5 MHz = 8750
  return 8750;
}

uint16_t RadioDriver::getMaxFrequency() {
  // FM Standard: 108.0 MHz = 10800
  return 10800;
}

void RadioDriver::setBias(bool on) {
  // digitalWrite(BIAS_CTR, on ? HIGH : LOW);
  biasState = on;
}

bool RadioDriver::getBias() { return biasState; }

void RadioDriver::debugRadioInfo() {
  Serial.print("Frequency: ");
  Serial.print(radio.getFrequency());
  Serial.print(" MHz Radio:");
  radio.debugStatus();
}

void RadioDriver::getRadioInfo(RDA5807M_Info *info) {
  radio.getRadioInfo(info);
}

void RadioDriver::getAudioInfo(RDA5807M_AudioInfo *info) {
  radio.getAudioInfo(info);
}

void RadioDriver::checkRDS() { radio.checkRDS(); }
