#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <time.h>

// I2C Slave Address (7-bit)
// Datasheet Section 13.8.4, Page 35: Address is 0110010 (0x32)
#define RX8010_I2C_ADDR 0x32

// Register Definitions (Datasheet Section 12.2, Page 12)
#define RX8010_REG_SEC 0x10
#define RX8010_REG_MIN 0x11
#define RX8010_REG_HOUR 0x12
#define RX8010_REG_WEEK 0x13
#define RX8010_REG_DAY 0x14
#define RX8010_REG_MONTH 0x15
#define RX8010_REG_YEAR 0x16
#define RX8010_REG_FLAG 0x1E
#define RX8010_REG_CTRL 0x1F

// Flag Bits
#define RX8010_FLAG_VLF 0x02  // Voltage Low Flag (Bit 1 of Flag Register)
#define RX8010_CTRL_STOP 0x40 // Stop Bit (Bit 6 of Control Register)

struct DateTime {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t day;
  uint8_t month;
  uint8_t year; // 0-99
  uint8_t week; // 0 (Sunday) to 6 (Saturday)
};

class RtcDriver {
public:
  RtcDriver();

  // Initializes I2C and checks for voltage loss
  bool init();

  // Core Time Functions
  DateTime getTime();
  bool setTime(const DateTime &dt);
  void setSoftwareTime(const DateTime &dt);
  DateTime getSoftwareTime() const;

  // Individual Getters/Setters
  void setSecond(uint8_t seconds);
  uint8_t getSecond();

  void setMinute(uint8_t minutes);
  uint8_t getMinute();

  void setHour(uint8_t hours);
  uint8_t getHour();

  void setDay(uint8_t day);
  uint8_t getDay();

  void setWeek(uint8_t week);
  uint8_t getWeek();

  void setMonth(uint8_t month);
  uint8_t getMonth();

  void setYear(uint16_t year);
  uint16_t getYear();

  // Utilities
  time_t getTimeAsTimeT();
  void setTime(uint8_t hour, uint8_t minute, uint8_t second);
  void setDate(uint8_t day, uint8_t month, uint16_t year);
  void setDateTime(const char *date,
                   const char *time); // Supports __DATE__, __TIME__

private:
  bool canRetryAccess() const;
  uint8_t decToBcd(uint8_t val);
  uint8_t bcdToDec(uint8_t val);
  void drainWire();
  DateTime getFallbackTime() const;
  DateTime calculateSoftwareTime() const;
  DateTime fromTimeT(time_t value) const;
  uint32_t getRetryDelay() const;
  time_t toTimeT(const DateTime &dt) const;
  bool readBytes(uint8_t reg, uint8_t *buffer, size_t length);
  bool readRegister(uint8_t reg, uint8_t &value);
  bool recoverAfterFailedAttempt(uint8_t attempt);
  void seedSoftwareClock(const DateTime &dt);
  bool tryReadBytes(uint8_t reg, uint8_t *buffer, size_t length);
  bool tryWriteBytes(uint8_t reg, const uint8_t *buffer, size_t length);
  bool writeBytes(uint8_t reg, const uint8_t *buffer, size_t length,
                  bool ignoreRetry = false);
  bool writeRegister(uint8_t reg, uint8_t val, bool ignoreRetry = false);
  void markBusFailure(const char *operation, int code);
  void markReadSuccess(const DateTime &dt);

  // RX8010 specific: The Week register is a bitmask, not a number 0-6.
  // Datasheet Section 13.1.2, Page 15
  uint8_t weekBinToBitmask(uint8_t week0to6);
  uint8_t weekBitmaskToBin(uint8_t bitmask);

  DateTime _cachedTime = {0, 0, 0, 1, 1, 0, 6};
  DateTime _softBaseTime = {0, 0, 0, 1, 1, 0, 6};
  bool _hasCachedTime = false;
  bool _hasSoftwareTime = false;
  uint8_t _failureCount = 0;
  uint32_t _lastReadTime = 0;
  uint32_t _softBaseMillis = 0;
  uint32_t _lastFailureTime = 0;
  uint32_t _lastErrorLogTime = 0;
};
