#include "RtcDriver.h"
#include "../managers/BusManager.h"

RtcDriver::RtcDriver() {}

bool RtcDriver::init() {
  BusManager::getInstance().requestI2C();
  // Wire.begin(); // Handled by BusManager

  // Check Voltage Low Flag (VLF) [cite: 1278]
  // Register 0x1E, Bit 1. If 1, data is invalid (power loss).
  uint8_t flags = readRegister(RX8010_REG_FLAG);

  if (flags & RX8010_FLAG_VLF) {
    // Power was lost. Initialization required.
    // Recommended sequence from Datasheet Section 13.7 Ex.1 [cite: 1330]

    // 1. Initialize Control Registers
    writeRegister(RX8010_REG_CTRL, 0x00); // Clear STOP, TEST, etc.
    writeRegister(RX8010_REG_FLAG, 0x00); // Clear VLF and other flags

    // 2. You might want to set a default time here, e.g., 2000-01-01
    DateTime dt = {0, 0, 0, 1, 1, 0, 6}; // Sat Jan 1st 2000
    setTime(dt);

    return false; // Returns false to indicate time was lost/reset
  }

  return true; // Time is valid
}

DateTime RtcDriver::getTime() {
  if (millis() - _lastReadTime < 5000 && _lastReadTime != 0) {
    return _cachedTime;
  }

  DateTime dt;

  // Burst read from 0x10 to 0x16 [cite: 507]
  BusManager::getInstance().requestI2C();
  Wire.beginTransmission(RX8010_I2C_ADDR);
  Wire.write(RX8010_REG_SEC);
  Wire.endTransmission();

  Wire.requestFrom(RX8010_I2C_ADDR, 7);

  if (Wire.available() == 7) {
    // Masking 0x7F ensures we ignore the Read-Only '0' bits defined in
    // datasheet
    dt.second = bcdToDec(Wire.read() & 0x7F);
    dt.minute = bcdToDec(Wire.read() & 0x7F);
    dt.hour = bcdToDec(Wire.read() & 0x3F); // 24-hour format usually

    uint8_t weekRaw = Wire.read();
    dt.week = weekBitmaskToBin(weekRaw); // Convert 0x01/0x02... to 0-6

    dt.day = bcdToDec(Wire.read() & 0x3F);
    dt.month = bcdToDec(Wire.read() & 0x1F);
    dt.year = bcdToDec(Wire.read()); // 0-99

    _cachedTime = dt;
    _lastReadTime = millis();
  }

  return dt;
}

void RtcDriver::setTime(DateTime dt) {
  // Procedure based on Datasheet Section 13.7 Ex.2 [cite: 1363]

  // 1. Set STOP bit to "1" to prevent timer update during writing [cite: 1374]
  uint8_t ctrl = readRegister(RX8010_REG_CTRL);
  writeRegister(RX8010_REG_CTRL, ctrl | RX8010_CTRL_STOP);

  // 2. Write time data
  BusManager::getInstance().requestI2C();
  Wire.beginTransmission(RX8010_I2C_ADDR);
  Wire.write(RX8010_REG_SEC);

  Wire.write(decToBcd(dt.second));
  Wire.write(decToBcd(dt.minute));
  Wire.write(decToBcd(dt.hour));
  Wire.write(weekBinToBitmask(dt.week)); // Convert 0-6 to 0x01/0x02...
  Wire.write(decToBcd(dt.day));
  Wire.write(decToBcd(dt.month));
  Wire.write(decToBcd(dt.year));

  Wire.endTransmission();

  // 3. Clear STOP bit to "0" to restart clock [cite: 1390]
  writeRegister(RX8010_REG_CTRL, ctrl & ~RX8010_CTRL_STOP);

  // Invalidate cache after setTime
  _lastReadTime = 0;
}

// --- Individual Getters/Setters ---
// Optimized: All getters use getTime() which is burst-read and cached.

void RtcDriver::setSecond(uint8_t seconds) {
  writeRegister(RX8010_REG_SEC, decToBcd(seconds));
  _lastReadTime = 0; // Invalidate cache
}

uint8_t RtcDriver::getSecond() { return getTime().second; }

void RtcDriver::setMinute(uint8_t minutes) {
  writeRegister(RX8010_REG_MIN, decToBcd(minutes));
  _lastReadTime = 0; // Invalidate cache
}

uint8_t RtcDriver::getMinute() { return getTime().minute; }

void RtcDriver::setHour(uint8_t hours) {
  writeRegister(RX8010_REG_HOUR, decToBcd(hours));
  _lastReadTime = 0; // Invalidate cache
}

uint8_t RtcDriver::getHour() { return getTime().hour; }

void RtcDriver::setDay(uint8_t day) {
  writeRegister(RX8010_REG_DAY, decToBcd(day));
  _lastReadTime = 0; // Invalidate cache
}

uint8_t RtcDriver::getDay() { return getTime().day; }

void RtcDriver::setWeek(uint8_t week) {
  writeRegister(RX8010_REG_WEEK, weekBinToBitmask(week));
  _lastReadTime = 0; // Invalidate cache
}

uint8_t RtcDriver::getWeek() { return getTime().week; }

void RtcDriver::setMonth(uint8_t month) {
  writeRegister(RX8010_REG_MONTH, decToBcd(month));
  _lastReadTime = 0; // Invalidate cache
}

uint8_t RtcDriver::getMonth() { return getTime().month; }

void RtcDriver::setYear(uint16_t year) {
  // Stores last two digits. Expects year > 2000.
  uint8_t y = (year >= 2000) ? (year - 2000) : year;
  writeRegister(RX8010_REG_YEAR, decToBcd(y));
  _lastReadTime = 0; // Invalidate cache
}

uint16_t RtcDriver::getYear() { return getTime().year + 2000; }

// --- Utilities ---

time_t RtcDriver::getTimeAsTimeT() {
  DateTime dt = getTime();
  struct tm t = {0};
  t.tm_sec = dt.second;
  t.tm_min = dt.minute;
  t.tm_hour = dt.hour;
  t.tm_mday = dt.day;
  t.tm_mon = dt.month - 1; // struct tm is 0-11
  t.tm_year =
      dt.year + 100; // struct tm is years since 1900. RTC is years since 2000.
  return mktime(&t);
}

void RtcDriver::setTime(uint8_t hour, uint8_t minute, uint8_t second) {
  DateTime dt = getTime(); // Read current date to preserve it
  dt.hour = hour;
  dt.minute = minute;
  dt.second = second;
  setTime(dt);
}

void RtcDriver::setDate(uint8_t day, uint8_t month, uint16_t year) {
  DateTime dt = getTime(); // Read current time to preserve it
  dt.day = day;
  dt.month = month;
  dt.year = (year >= 2000) ? (year - 2000) : year;

  struct tm t = {0};
  t.tm_mday = day;
  t.tm_mon = month - 1;
  t.tm_year = dt.year + 100;
  mktime(&t); // Normalizes struct and finds tm_wday
  dt.week = t.tm_wday;

  setTime(dt);
}

// Parses compile time strings: __DATE__ "Mmm dd yyyy", __TIME__ "hh:mm:ss"
void RtcDriver::setDateTime(const char *date, const char *time) {
  char monthStr[4];
  int day, year, hour, minute, second;

  sscanf(date, "%s %d %d", monthStr, &day, &year);
  sscanf(time, "%d:%d:%d", &hour, &minute, &second);

  int month = 0;
  if (strcmp(monthStr, "Jan") == 0)
    month = 1;
  else if (strcmp(monthStr, "Feb") == 0)
    month = 2;
  else if (strcmp(monthStr, "Mar") == 0)
    month = 3;
  else if (strcmp(monthStr, "Apr") == 0)
    month = 4;
  else if (strcmp(monthStr, "May") == 0)
    month = 5;
  else if (strcmp(monthStr, "Jun") == 0)
    month = 6;
  else if (strcmp(monthStr, "Jul") == 0)
    month = 7;
  else if (strcmp(monthStr, "Aug") == 0)
    month = 8;
  else if (strcmp(monthStr, "Sep") == 0)
    month = 9;
  else if (strcmp(monthStr, "Oct") == 0)
    month = 10;
  else if (strcmp(monthStr, "Nov") == 0)
    month = 11;
  else if (strcmp(monthStr, "Dec") == 0)
    month = 12;

  setDate(day, month, year);
  setTime(hour, minute, second);
}

// --- Private Helpers ---

uint8_t RtcDriver::decToBcd(uint8_t val) {
  return ((val / 10 * 16) + (val % 10));
}

uint8_t RtcDriver::bcdToDec(uint8_t val) {
  return ((val / 16 * 10) + (val % 16));
}

uint8_t RtcDriver::readRegister(uint8_t reg) {
  BusManager::getInstance().requestI2C();
  Wire.beginTransmission(RX8010_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(RX8010_I2C_ADDR, 1);
  uint8_t val = Wire.read();
  return val;
}

void RtcDriver::writeRegister(uint8_t reg, uint8_t val) {
  BusManager::getInstance().requestI2C();
  Wire.beginTransmission(RX8010_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

// RX8010 uses a bitmask: Sun=0x01, Mon=0x02, Tue=0x04 ... Sat=0x40
uint8_t RtcDriver::weekBinToBitmask(uint8_t week0to6) {
  return (1 << week0to6);
}

uint8_t RtcDriver::weekBitmaskToBin(uint8_t bitmask) {
  for (uint8_t i = 0; i < 7; i++) {
    if ((bitmask >> i) & 0x01) {
      return i;
    }
  }
  return 0; // Default to Sunday if error
}