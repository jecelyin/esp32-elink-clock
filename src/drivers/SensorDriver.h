#pragma once

#include <Adafruit_SHT31.h>

class SensorDriver {
public:
  SensorDriver();
  bool init();
  bool readData(float &temp, float &hum);
  int getBatteryLevel();

  // 硬件自检逻辑 (仅逻辑，不包含显示)
  bool checkHardware();
  bool checkDevice(uint8_t address);
private:
  float getBatteryVoltage();
private:
  Adafruit_SHT31 sht31;
};
