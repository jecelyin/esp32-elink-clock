#include "SensorDriver.h"

SensorDriver::SensorDriver() {}

bool SensorDriver::init() {
  return sht31.begin(SHT30_I2C_ADDR);
}

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
