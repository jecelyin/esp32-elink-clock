#pragma once

#include <Adafruit_SHT31.h>

class SensorDriver {
public:
  SensorDriver();
  bool init();
  bool readData(float &temp, float &hum);

  // 硬件自检逻辑 (仅逻辑，不包含显示)
  bool checkDevice(uint8_t address);
private:
  bool initOnce();
  bool readSensorOnce(float &temp, float &hum);
  bool retryRead(float &temp, float &hum);
  bool useCachedData(float &temp, float &hum) const;
  void markReadSuccess(float temp, float hum);
  void logFailure(const char *operation);
private:
  Adafruit_SHT31 sht31;
  bool initialized = false;
  bool hasCachedData = false;
  float lastTemp = 0;
  float lastHum = 0;
  uint32_t lastErrorLogTime = 0;
};
