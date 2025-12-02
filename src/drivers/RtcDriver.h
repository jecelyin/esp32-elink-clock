#pragma once

#include "../config.h"
#include <Arduino.h>
#include <Wire.h>

struct DateTime {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t day;
  uint8_t month;
  uint8_t year; // 0-99
  uint8_t week;
};

class RtcDriver {
public:
  RtcDriver();
  bool init();
  DateTime getTime();
  void setTime(DateTime dt);

private:
  uint8_t bcd2bin(uint8_t val);
  uint8_t bin2bcd(uint8_t val);
  void writeReg(uint8_t reg, uint8_t val);
  uint8_t readReg(uint8_t reg);

  const uint8_t I2C_ADDR = 0x32; // RX8010SJ Address
};
