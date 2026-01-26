#include "SensorDriver.h"
#include "../config.h"
#include <Arduino.h>
#include <Wire.h>

// ESP32 ADC 参数
static const float ADC_MAX = 4095.0;
static const float ADC_REF = 3.3;

// 分压比（实测：电池4.07V时，分压点2.0V => 4.07 / 2.0 = 2.035）
static const float DIVIDER_RATIO = 2.035; // 读到的是分压，需要还原

SensorDriver::SensorDriver() {}

bool SensorDriver::init() { return sht31.begin(SHT30_I2C_ADDR); }

bool SensorDriver::checkDevice(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

#include <driver/adc.h>
#include <esp_adc_cal.h>

bool SensorDriver::readData(float &temp, float &hum) {
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
  adc1_config_channel_atten(BAT_ADC_CHANNEL, ADC_ATTEN_DB_12);

  // 使用 ESP32 内置校准数据
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100,
                           &adc_chars);

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

  // ADC 原始值 → ADC 引脚电压 (单位: mV)
  uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
  float adcVoltage = voltage_mv / 1000.0;

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

bool SensorDriver::checkHardware() {
  uint8_t addresses[] = {RX8010_I2C_ADDR, SHT30_I2C_ADDR, ES8311_ADDRESS,
                         M5807_ADDR_FULL_ACCESS};
  for (uint8_t addr : addresses) {
    if (!checkDevice(addr))
      return false;
  }
  return true;
}
