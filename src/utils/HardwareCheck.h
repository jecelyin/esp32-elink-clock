#pragma once

#include "../drivers/BatteryDriver.h"
#include <Arduino.h>

namespace HardwareCheck {

struct Device {
  const char *name;
  const char *shortName;
  uint8_t address;
  bool fuelGauge;
};

constexpr uint8_t DEVICE_COUNT = 5;
constexpr uint8_t REQUIRED_VERSION = 2;
extern const Device DEVICES[DEVICE_COUNT];

void setPowerEnabled(bool enabled);
bool checkDevice(const Device &device, BatteryDriver *battery);

} // namespace HardwareCheck
