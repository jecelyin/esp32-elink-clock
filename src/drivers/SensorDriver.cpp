#include "SensorDriver.h"
#include "../managers/BusManager.h"

// ESP32 ADC 参数
static const float ADC_MAX = 4095.0;
static const float ADC_REF = 3.3;

// 分压比（10k / (10k + 10k)）
static const float DIVIDER_RATIO = 2.0; // 读到的是 1/2，需要 ×2

SensorDriver::SensorDriver() {}

bool SensorDriver::init() {
  BusManager::getInstance().requestI2C();
  return sht31.begin(SHT30_I2C_ADDR);
}

#include <driver/adc.h>
#include <esp_adc_cal.h>

bool SensorDriver::readData(float &temp, float &hum) {
  BusManager::getInstance().requestI2C();
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    temp = t;
    hum = h;
    return true;
  }
  return false;
}

float SensorDriver::getBatteryVoltage() {
#if BAT_ADC == 34
#define BAT_ADC_CHANNEL ADC1_CHANNEL_6
#else
#error "BAT_ADC pin must be GPIO34 (ADC1_CHANNEL_6)"
#endif

  // ADC 配置
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(BAT_ADC_CHANNEL, ADC_ATTEN_DB_11);

  // -------- 高阻分压关键处理 --------

  // 丢弃第一次采样（给 ADC 采样电容充电）
  adc1_get_raw(BAT_ADC_CHANNEL);
  delayMicroseconds(50);

  uint32_t raw = 0;
  const int samples = 16; // 高阻建议 ≥16

  for (int i = 0; i < samples; i++) {
    raw += adc1_get_raw(BAT_ADC_CHANNEL);
    delayMicroseconds(30);
  }
  raw /= samples;

  // ADC 原始值 → ADC 引脚电压
  float adcVoltage = (raw / ADC_MAX) * ADC_REF;

  // 分压还原为电池电压
  float batteryVoltage = adcVoltage * DIVIDER_RATIO;

  Serial.printf("BAT raw=%lu, adc=%.3fV, bat=%.3fV\n", raw, adcVoltage,
                batteryVoltage);

  return batteryVoltage;
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
