#include "SensorDriver.h"
#include "../config.h"
#include "../utils/I2CBus.h"
#include <Arduino.h>

namespace {
constexpr uint32_t SENSOR_ERROR_LOG_INTERVAL_MS = 10000UL;
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
  return I2CBus::probeDevice(address);
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
