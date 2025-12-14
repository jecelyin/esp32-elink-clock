#include "SensorDriver.h"
#include "../managers/BusManager.h"

SensorDriver::SensorDriver() {}

bool SensorDriver::init() {
  BusManager::getInstance().requestI2C();
  return sht31.begin(SHT30_I2C_ADDR);
}

#include <driver/adc.h>

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
  // Use legacy ADC driver to avoid conflict with Audio library
  // Map BAT_ADC (34) to ADC1_CHANNEL_6
  #if BAT_ADC == 34
    #define BAT_ADC_CHANNEL ADC1_CHANNEL_6
  #else
    #error "BAT_ADC pin definition does not match expected ADC1_CHANNEL_6 (GPIO 34). Please update mapping."
  #endif

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(BAT_ADC_CHANNEL, ADC_ATTEN_DB_12);

  int raw = adc1_get_raw(BAT_ADC_CHANNEL);
  
  // Formula: reading * (3.3 / 4095.0) * 2
  return (raw * 3.3f / 4095.0f) * 2.0f;
}

int SensorDriver::getBatteryLevel() {
  float voltage = getBatteryVoltage();
  // Simple linear mapping: 3.0V = 0%, 4.2V = 100%
  if (voltage >= 4.2f) return 100;
  if (voltage <= 3.3f) return 0; // Cutoff usually around 3.2-3.3V under load
  
  return (int)((voltage - 3.3f) / (4.2f - 3.3f) * 100.0f);
}
