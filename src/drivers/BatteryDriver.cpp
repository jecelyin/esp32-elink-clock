#include "BatteryDriver.h"
#include "../config.h"
#include "../utils/I2CBus.h"
#include <Wire.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

namespace {
constexpr uint8_t REG_VERSION = 0x00;
constexpr uint8_t REG_VCELL = 0x02;
constexpr uint8_t REG_SOC = 0x04;
constexpr uint8_t REG_RRT_ALERT = 0x06;
constexpr uint8_t REG_CONFIG = 0x08;
constexpr uint8_t REG_MODE = 0x0A;
constexpr uint8_t MODE_SLEEP_MASK = 0xC0;
constexpr uint8_t CONFIG_ATHD_MASK = 0xF8;
constexpr uint8_t CONFIG_UFG_MASK = 0x02;
constexpr float CW2015_VCELL_LSB_VOLTS = 0.000305f;
constexpr float DIVIDER_RATIO = 2.035f;

adc_atten_t getBatteryAdcAttenuation() {
  return ADC_ATTEN_DB_12;
}
} // namespace

void BatteryDriver::begin() {
  I2CBus::begin();
  init();
}

bool BatteryDriver::init() {
  I2CBus::Guard guard;
  if (!guard.isLocked())
    return false;

  uint8_t version = 0;
  uint8_t mode = 0;
  initialized = initFuelGaugeLocked(version, mode);
  return initialized;
}

bool BatteryDriver::checkFuelGauge() {
  BatteryInfo info;
  return readFuelGaugeInfo(info);
}

BatteryInfo BatteryDriver::readInfo() {
  BatteryInfo info;
  if (readFuelGaugeInfo(info)) {
    lastInfo = info;
    return lastInfo;
  }

  fillAdcFallback(info);
  lastInfo = info;
  return lastInfo;
}

int BatteryDriver::getBatteryLevel() {
  BatteryInfo info = readInfo();
  return info.levelPercent;
}

const BatteryInfo &BatteryDriver::getLastInfo() const { return lastInfo; }

bool BatteryDriver::initFuelGaugeLocked(uint8_t &version, uint8_t &mode) {
  if (!readRegisterLocked(REG_VERSION, version))
    return false;
  if (!readRegisterLocked(REG_MODE, mode))
    return false;

  if ((mode & MODE_SLEEP_MASK) != 0) {
    // 关键逻辑：CW2015 的 MODE 默认休眠位可能是 11；读取业务寄存器前
    // 必须显式写 00 唤醒，并同时清掉快启/POR 命令位的潜在残留。
    if (!writeRegisterLocked(REG_MODE, 0x00))
      return false;
    delay(20);
    if (!readRegisterLocked(REG_MODE, mode))
      return false;
  }

  initialized = true;
  return true;
}

bool BatteryDriver::readFuelGaugeInfo(BatteryInfo &info) {
  I2CBus::Guard guard;
  if (!guard.isLocked())
    return false;

  uint8_t version = 0;
  uint8_t mode = 0;
  uint8_t vcell[2] = {0};
  uint8_t soc[2] = {0};
  uint8_t rrt[2] = {0};
  uint8_t config = 0;

  if (!initFuelGaugeLocked(version, mode))
    return false;
  if (!readRegistersLocked(REG_VCELL, vcell, 2))
    return false;
  if (!readRegistersLocked(REG_SOC, soc, 2))
    return false;
  if (!readRegistersLocked(REG_RRT_ALERT, rrt, 2))
    return false;
  if (!readRegisterLocked(REG_CONFIG, config))
    return false;

  uint16_t rawVcell = ((uint16_t)vcell[0] << 8 | vcell[1]) & 0x3FFF;
  float socPercent = soc[0] + soc[1] / 256.0f;
  info.source = BATTERY_SOURCE_CW2015;
  info.gaugeOnline = true;
  info.dataValid = true;
  info.version = version;
  info.mode = mode;
  info.config = config;
  info.voltage = rawVcell * CW2015_VCELL_LSB_VOLTS;
  info.socPercent = socPercent;
  info.levelPercent = clampLevel((int)(socPercent + 0.5f));
  info.alert = (rrt[0] & 0x80) != 0;
  info.remainingMinutes = ((uint16_t)(rrt[0] & 0x1F) << 8) | rrt[1];
  info.alertThreshold = (config & CONFIG_ATHD_MASK) >> 3;
  info.profileUpdated = (config & CONFIG_UFG_MASK) != 0;
  info.sleeping = (mode & MODE_SLEEP_MASK) != 0;
  return true;
}

bool BatteryDriver::readRegisterLocked(uint8_t reg, uint8_t &value) {
  return readRegistersLocked(reg, &value, 1);
}

bool BatteryDriver::readRegistersLocked(uint8_t reg, uint8_t *buffer,
                                        uint8_t length) {
  Wire.beginTransmission(CW2015_I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0)
    return false;

  size_t received = Wire.requestFrom((uint8_t)CW2015_I2C_ADDR, length);
  if (received != length) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  for (uint8_t i = 0; i < length; ++i) {
    buffer[i] = Wire.read();
  }
  return true;
}

bool BatteryDriver::writeRegisterLocked(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(CW2015_I2C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

void BatteryDriver::fillAdcFallback(BatteryInfo &info) {
  // 关键逻辑：CW2015 不在线时仍保留旧 ADC 分压估算，设置页会明确标记
  // 数据来源，避免把降级读数误认为 fuel gauge 的精确 SOC。
  info.source = BATTERY_SOURCE_ADC;
  info.gaugeOnline = false;
  info.dataValid = true;
  info.voltage = readAdcVoltage();
  info.levelPercent = estimateLevelFromVoltage(info.voltage);
  info.socPercent = info.levelPercent;
}

float BatteryDriver::readAdcVoltage() {
#if BAT_ADC == 34
#define BAT_ADC_CHANNEL ADC1_CHANNEL_6
  adc_atten_t attenuation = getBatteryAdcAttenuation();
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(BAT_ADC_CHANNEL, attenuation);

  esp_adc_cal_characteristics_t adcChars;
  esp_adc_cal_characterize(ADC_UNIT_1, attenuation, ADC_WIDTH_BIT_12, 1100,
                           &adcChars);

  adc1_get_raw(BAT_ADC_CHANNEL);
  delayMicroseconds(50);

  uint32_t raw = 0;
  for (int i = 0; i < 16; i++) {
    raw += adc1_get_raw(BAT_ADC_CHANNEL);
    delayMicroseconds(30);
  }
  raw /= 16;

  uint32_t voltageMv = esp_adc_cal_raw_to_voltage(raw, &adcChars);
  float adcVoltage = voltageMv / 1000.0f;
  float batteryVoltage = adcVoltage * DIVIDER_RATIO;
  Serial.printf("BAT raw=%lu, adc=%.3fV, bat=%.3fV\n", raw, adcVoltage,
                batteryVoltage);
  return batteryVoltage;
#else
#error "BAT_ADC pin must be GPIO34 (ADC1_CHANNEL_6)"
#endif
}

int BatteryDriver::estimateLevelFromVoltage(float voltage) const {
  if (voltage >= 4.2f)
    return 100;
  if (voltage <= 3.3f)
    return 0;

  return clampLevel((int)((voltage - 3.3f) / (4.2f - 3.3f) * 100.0f));
}

int BatteryDriver::clampLevel(int level) const {
  if (level < 0)
    return 0;
  if (level > 100)
    return 100;
  return level;
}
