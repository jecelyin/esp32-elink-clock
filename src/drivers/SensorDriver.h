#pragma once

#include "../config.h"
#include <Adafruit_SHT31.h>

class SensorDriver {
public:
  SensorDriver();
  bool init();
  bool readData(float &temp, float &hum);

private:
  Adafruit_SHT31 sht31;
};
