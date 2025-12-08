#include "SensorDriver.h"
#include "../managers/BusManager.h"

SensorDriver::SensorDriver() {}

bool SensorDriver::init() {
  BusManager::getInstance().requestI2C();
  return sht31.begin(SHT30_I2C_ADDR);
}

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
