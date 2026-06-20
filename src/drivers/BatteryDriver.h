#pragma once

#include <Arduino.h>

enum BatteryDataSource : uint8_t {
  BATTERY_SOURCE_NONE,
  BATTERY_SOURCE_CW2015,
  BATTERY_SOURCE_ADC
};

struct BatteryInfo {
  BatteryDataSource source = BATTERY_SOURCE_NONE;
  bool gaugeOnline = false;
  bool dataValid = false;
  int levelPercent = -1;
  float socPercent = 0.0f;
  float voltage = 0.0f;
  uint16_t remainingMinutes = 0;
  uint8_t version = 0;
  uint8_t mode = 0;
  uint8_t config = 0;
  uint8_t alertThreshold = 0;
  bool alert = false;
  bool profileUpdated = false;
  bool sleeping = false;
};

class BatteryDriver {
public:
  void begin();
  bool init();
  bool checkFuelGauge();
  BatteryInfo readInfo();
  int getBatteryLevel();
  const BatteryInfo &getLastInfo() const;

private:
  bool initFuelGaugeLocked(uint8_t &version, uint8_t &mode);
  bool readFuelGaugeInfo(BatteryInfo &info);
  bool readRegisterLocked(uint8_t reg, uint8_t &value);
  bool readRegistersLocked(uint8_t reg, uint8_t *buffer, uint8_t length);
  bool writeRegisterLocked(uint8_t reg, uint8_t value);
  void fillAdcFallback(BatteryInfo &info);
  float readAdcVoltage();
  int estimateLevelFromVoltage(float voltage) const;
  int clampLevel(int level) const;

  BatteryInfo lastInfo;
  bool initialized = false;
};
