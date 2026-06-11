#include "SensorDriver.h"
#include "../config.h"
#include "../utils/I2CBus.h"
#include <Arduino.h>
#include <Wire.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_idf_version.h>

namespace {
// 分压比（实测：电池4.07V时，分压点2.0V => 4.07 / 2.0 = 2.035）
constexpr float DIVIDER_RATIO = 2.035f; // 读到的是分压，需要还原
constexpr uint32_t SENSOR_ERROR_LOG_INTERVAL_MS = 10000UL;

adc_atten_t getBatteryAdcAttenuation() {
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
  // IDF 5.x 将满量程电池采样档位命名为 12dB。
  return ADC_ATTEN_DB_12;
#else
  // IDF 4.x 仍使用 11dB 命名，但对应的就是旧核心可用的最高采样档位。
  return ADC_ATTEN_DB_11;
#endif
}
} // namespace

SensorDriver::SensorDriver() {}

bool SensorDriver::init() {
  I2CBus::begin();
  initialized = initOnce();
  if (!initialized) {
    logFailure("init");
    I2CBus::recover();
    delay(20);
    initialized = initOnce();
  }
  return initialized;
}

bool SensorDriver::initOnce() {
  I2CBus::Guard guard;
  if (!guard.isLocked())
    return false;
  return sht31.begin(SHT30_I2C_ADDR);
}

bool SensorDriver::checkDevice(uint8_t address) {
  I2CBus::Guard guard;
  if (!guard.isLocked())
    return false;
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool SensorDriver::readData(float &temp, float &hum) {
  if (!initialized && !init()) {
    return useCachedData(temp, hum);
  }

  if (readSensorOnce(temp, hum)) {
    markReadSuccess(temp, hum);
    return true;
  }

  logFailure("read");
  initialized = false;
  if (retryRead(temp, hum))
    return true;
  return useCachedData(temp, hum);
}

bool SensorDriver::readSensorOnce(float &temp, float &hum) {
  I2CBus::Guard guard;
  if (!guard.isLocked())
    return false;
  return sht31.readBoth(&temp, &hum) && !isnan(temp) && !isnan(hum);
}

bool SensorDriver::retryRead(float &temp, float &hum) {
  I2CBus::recover();
  delay(20);
  if (!init())
    return false;
  if (readSensorOnce(temp, hum)) {
    markReadSuccess(temp, hum);
    return true;
  }
  return false;
}

bool SensorDriver::useCachedData(float &temp, float &hum) const {
  if (!hasCachedData)
    return false;
  temp = lastTemp;
  hum = lastHum;
  return true;
}

void SensorDriver::markReadSuccess(float temp, float hum) {
  lastTemp = temp;
  lastHum = hum;
  hasCachedData = true;
  initialized = true;
}

void SensorDriver::logFailure(const char *operation) {
  uint32_t now = millis();
  if (lastErrorLogTime != 0 &&
      now - lastErrorLogTime < SENSOR_ERROR_LOG_INTERVAL_MS) {
    return;
  }
  Serial.printf("SHT30 %s failed\n", operation);
  lastErrorLogTime = now;
}

float SensorDriver::getBatteryVoltage() {
#if BAT_ADC == 34
  // 关键逻辑：ESP32-audioI2S 当前依赖 legacy I2S/ADC 栈，不能混用
  // adc_oneshot 新驱动，否则启动阶段会触发 ADC driver_ng 冲突并重启。
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
  float adcVoltage = voltageMv / 1000.0;
  float batteryVoltage = adcVoltage * DIVIDER_RATIO;

  Serial.printf("BAT raw=%lu, adc=%.3fV, bat=%.3fV\n", raw, adcVoltage,
                batteryVoltage);
  return batteryVoltage;
#else
#error "BAT_ADC pin must be GPIO34 (ADC1_CHANNEL_6)"
#endif
}

int SensorDriver::getBatteryLevel() {
  float voltage = getBatteryVoltage();
  // Simple linear mapping: 3.0V = 0%, 4.2V = 100%
  if (voltage >= 4.2f)
    return 100;
  if (voltage <= 3.3f)
    return 0; // Cutoff usually around 3.2-3.3V under load

  return (int)((voltage - 3.3f) / (4.2f - 3.3f) * 100.0f);
}

bool SensorDriver::checkHardware() {
  uint8_t addresses[] = {RX8010_I2C_ADDR, SHT30_I2C_ADDR, ES8311_ADDRESS,
                         M5807_ADDR_FULL_ACCESS};
  for (uint8_t addr : addresses) {
    if (!checkDevice(addr))
      return false;
  }
  return true;
}
