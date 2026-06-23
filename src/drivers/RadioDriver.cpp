#include "RadioDriver.h"
#include "../utils/I2CBus.h"

namespace {
constexpr uint32_t RADIO_I2C_LOCK_TIMEOUT_MS = 300UL;
constexpr uint32_t RADIO_I2C_ERROR_LOG_INTERVAL_MS = 3000UL;
constexpr uint16_t RADIO_DEFAULT_FREQUENCY = 8750;
constexpr uint16_t RADIO_MIN_FREQUENCY = 8750;
constexpr uint16_t RADIO_MAX_FREQUENCY = 10800;
constexpr uint16_t RADIO_SCAN_MIN_GAP = 30;
constexpr uint8_t RADIO_SCAN_MIN_RSSI = 8;
uint32_t g_lastRadioI2CErrorLogTime = 0;

void logRadioI2CFailure(const char *operation) {
  uint32_t now = millis();
  if (g_lastRadioI2CErrorLogTime != 0 &&
      now - g_lastRadioI2CErrorLogTime < RADIO_I2C_ERROR_LOG_INTERVAL_MS) {
    return;
  }
  // 关键逻辑：收音机仍然直接使用底层 Wire，如果不先拿统一总线锁，
  // 就会和 RTC/SHT30/ES8311 在另一核上抢总线，触发随机 timeout。
  Serial.printf("Radio I2C lock failed: %s\n", operation);
  g_lastRadioI2CErrorLogTime = now;
}

template <typename Callback>
bool runWithRadioBusLock(const char *operation, Callback callback) {
  I2CBus::Guard guard(RADIO_I2C_LOCK_TIMEOUT_MS);
  if (!guard.isLocked()) {
    logRadioI2CFailure(operation);
    return false;
  }
  callback();
  return true;
}

template <typename Value, typename Callback>
Value runWithRadioBusLock(const char *operation, Value fallback,
                          Callback callback) {
  I2CBus::Guard guard(RADIO_I2C_LOCK_TIMEOUT_MS);
  if (!guard.isLocked()) {
    logRadioI2CFailure(operation);
    return fallback;
  }
  return callback();
}

uint16_t normalizeFrequency(uint16_t freq) {
  if (freq < RADIO_MIN_FREQUENCY || freq > RADIO_MAX_FREQUENCY) {
    return RADIO_DEFAULT_FREQUENCY;
  }
  return freq;
}

bool isDistinctStation(uint16_t freq, uint16_t previousFreq) {
  return previousFreq == 0 || freq >= previousFreq + RADIO_SCAN_MIN_GAP;
}

bool isSeekStationUsable(uint16_t freq, const RDA5807M_Info &info,
                         uint16_t previousFreq) {
  return freq >= RADIO_MIN_FREQUENCY && freq <= RADIO_MAX_FREQUENCY &&
         info.rssi >= RADIO_SCAN_MIN_RSSI &&
         isDistinctStation(freq, previousFreq);
}

void logScanStart() {
#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Radio][scan] start min=%u max=%u minRssi=%u\n",
                RADIO_MIN_FREQUENCY, RADIO_MAX_FREQUENCY, RADIO_SCAN_MIN_RSSI);
#endif
}

void logScanSeek(bool seekOk, uint16_t freq, const RDA5807M_Info &info) {
#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Radio][scan] seek ok=%d freq=%u rssi=%u tuned=%d stereo=%d\n",
                seekOk, freq, info.rssi, info.tuned, info.stereo);
#endif
}

void logScanStation(uint8_t found, uint16_t freq, const RDA5807M_Info &info) {
#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Radio][scan] station #%u freq=%u rssi=%u stereo=%d\n",
                found, freq, info.rssi, info.stereo);
#endif
}

void logScanDone(uint8_t found, uint16_t tunedFreq) {
#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Radio][scan] done count=%u tuned=%u\n", found, tunedFreq);
#endif
}
} // namespace

static RDSParser *g_rdsParser = nullptr;
static void rdsCallback(uint16_t b1, uint16_t b2, uint16_t b3, uint16_t b4) {
  if (g_rdsParser)
    g_rdsParser->decode(b1, b2, b3, b4);
}

RadioDriver::RadioDriver() : lastFrequency(RADIO_DEFAULT_FREQUENCY) {
  g_rdsParser = &rdsParser;
}

bool RadioDriver::init() {
  lastFrequency = RADIO_DEFAULT_FREQUENCY;
  return true;
}

void RadioDriver::setup() {
  // Enable information to the Serial port
  radio.debugEnable(true);

  bool initOk = false;
  bool busReady = runWithRadioBusLock("setup", [&]() {
    // 关键逻辑：初始化过程里会连续读写多个寄存器，必须整段持锁，
    // 否则中途被 RTC/NTP 流程插入访问后，RDA5807M 状态机会错乱。
    initOk = radio.init();
    if (!initOk)
      return;

    radio.setBand(RDA5807M_BAND_FM);
    radio.setVolume(2);
    radio.setMono(false);
    radio.setMute(false);
    radio.attachReceiveRDS(rdsCallback);
    // 关键逻辑：RDA5807M 仅完成 init 还不足以让模拟音频链路进入稳定输出，
    // 如果这里不主动回写一次频点，首次进入页面时通常要先 seek 一次才会听到沙沙声。
    radio.setFrequency(lastFrequency);
  });

  if (!busReady) {
    return;
  }

  if (!initOk) {
    Serial.println("no radio chip found.");
    delay(4000);
    ESP.restart();
  }
}

void RadioDriver::powerDown() {
  runWithRadioBusLock("powerDown", [&]() { radio.term(); });
}

void RadioDriver::setFrequency(uint16_t freq) {
  lastFrequency = normalizeFrequency(freq);
  rdsParser.reset();
  runWithRadioBusLock("setFrequency",
                      [&]() { radio.setFrequency(lastFrequency); });
}

void RadioDriver::setVolume(uint8_t vol) {
  runWithRadioBusLock("setVolume", [&]() { radio.setVolume(vol); });
}

uint8_t RadioDriver::getVolume() { return radio.getVolume(); }

void RadioDriver::mute(bool m) {
  runWithRadioBusLock("mute", [&]() { radio.setMute(m); });
}

uint16_t RadioDriver::getFrequency() {
  uint16_t freq = runWithRadioBusLock("getFrequency", lastFrequency,
                                      [&]() { return radio.getFrequency(); });
  lastFrequency = normalizeFrequency(freq);
  return lastFrequency;
}

uint8_t RadioDriver::scanStations(uint16_t *stations, uint8_t maxStations) {
  if (stations == nullptr || maxStations == 0) {
    return 0;
  }

  uint8_t found = 0;
  uint16_t origin = getFrequency();
  logScanStart();
  setFrequency(RADIO_MIN_FREQUENCY);

  uint16_t lastSeekFreq = RADIO_MIN_FREQUENCY;
  uint16_t lastSavedFreq = 0;
  while (found < maxStations) {
    uint16_t freq = 0;
    RDA5807M_Info info{false, 0, 0, false, false, false, false};
    if (!seekNextScanStation(&freq, &info) || freq <= lastSeekFreq ||
        freq > RADIO_MAX_FREQUENCY) {
      break;
    }

    lastFrequency = normalizeFrequency(freq);
    lastSeekFreq = freq;
    if (!isSeekStationUsable(freq, info, lastSavedFreq)) {
      continue;
    }

    stations[found++] = freq;
    lastSavedFreq = freq;
    logScanStation(found, freq, info);
  }

  setFrequency(found > 0 ? stations[0] : origin);
  logScanDone(found, found > 0 ? stations[0] : origin);
  return found;
}

bool RadioDriver::seekNextScanStation(uint16_t *freq, RDA5807M_Info *info) {
  bool seekOk = false;
  bool busOk = runWithRadioBusLock("scanSeekUp", [&]() {
    seekOk = radio.seekUp(false);
    *freq = radio.getFrequency();
    radio.getRadioInfo(info);
  });

  logScanSeek(seekOk, *freq, *info);
  return busOk && seekOk;
}

void RadioDriver::getFormattedFrequency(char *s, uint8_t length) {
  uint16_t freq = getFrequency();
  // Format as "101.1" etc.
  // freq is in 10kHz units (e.g. 10110) / 100 = 101.1
  sprintf(s, "%d.%d", freq / 100, (freq % 100) / 10);
}

uint16_t RadioDriver::getMinFrequency() {
  return RADIO_MIN_FREQUENCY;
}

uint16_t RadioDriver::getMaxFrequency() {
  return RADIO_MAX_FREQUENCY;
}

void RadioDriver::debugRadioInfo() {
  runWithRadioBusLock("debugRadioInfo", [&]() {
    Serial.print("Frequency: ");
    Serial.print(radio.getFrequency());
    Serial.print(" MHz Radio:");
    radio.debugStatus();
  });
}

void RadioDriver::getRadioInfo(RDA5807M_Info *info) {
  if (info == nullptr)
    return;
  *info = RDA5807M_Info{false, 0, 0, false, false, false, false};
  runWithRadioBusLock("getRadioInfo", [&]() { radio.getRadioInfo(info); });
}

void RadioDriver::getAudioInfo(RDA5807M_AudioInfo *info) {
  if (info == nullptr)
    return;
  *info = RDA5807M_AudioInfo{0, false, false, false};
  runWithRadioBusLock("getAudioInfo", [&]() { radio.getAudioInfo(info); });
}

void RadioDriver::checkRDS() {
  runWithRadioBusLock("checkRDS", [&]() { radio.checkRDS(); });
}
