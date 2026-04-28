#include "RtcDriver.h"
#include "../utils/I2CBus.h"

namespace {
constexpr uint32_t RTC_CACHE_MS = 5000UL;
const DateTime RTC_DEFAULT_TIME = {0, 0, 0, 1, 1, 0, 6};
} // namespace

RtcDriver::RtcDriver() {}
bool RtcDriver::init() {
  I2CBus::begin();
  uint8_t flags = 0;
  if (!readRegister(RX8010_REG_FLAG, flags))
    return false;
  if (flags & RX8010_FLAG_VLF) {
    // 电压跌落后，RTC 内部时间可能已经失效，这里按推荐顺序清标志并回写默认值。
    if (!writeRegister(RX8010_REG_CTRL, 0x00) ||
        !writeRegister(RX8010_REG_FLAG, 0x00)) {
      return false;
    }
    DateTime dt = RTC_DEFAULT_TIME;
    setTime(dt);
    return false;
  }
  return true;
}
DateTime RtcDriver::getTime() {
  if (_hasCachedTime && _lastReadTime != 0 &&
      millis() - _lastReadTime < RTC_CACHE_MS) {
    return _cachedTime;
  }
  uint8_t buffer[7] = {0};
  if (!readBytes(RX8010_REG_SEC, buffer, sizeof(buffer)))
    return getFallbackTime();
  DateTime dt = RTC_DEFAULT_TIME;
  dt.second = bcdToDec(buffer[0] & 0x7F);
  dt.minute = bcdToDec(buffer[1] & 0x7F);
  dt.hour = bcdToDec(buffer[2] & 0x3F);
  dt.week = weekBitmaskToBin(buffer[3]);
  dt.day = bcdToDec(buffer[4] & 0x3F);
  dt.month = bcdToDec(buffer[5] & 0x1F);
  dt.year = bcdToDec(buffer[6]);
  markReadSuccess(dt);
  return dt;
}
bool RtcDriver::setTime(const DateTime &dt) {
  uint8_t ctrl = 0;
  if (!readRegister(RX8010_REG_CTRL, ctrl))
    return false;
  if (!writeRegister(RX8010_REG_CTRL, ctrl | RX8010_CTRL_STOP))
    return false;
  uint8_t buffer[7] = {decToBcd(dt.second), decToBcd(dt.minute),
                       decToBcd(dt.hour),   weekBinToBitmask(dt.week),
                       decToBcd(dt.day),    decToBcd(dt.month),
                       decToBcd(dt.year)};
  bool writeOk = writeBytes(RX8010_REG_SEC, buffer, sizeof(buffer));
  bool restartOk = writeRegister(RX8010_REG_CTRL, ctrl & ~RX8010_CTRL_STOP,
                                 true);
  if (writeOk && restartOk) {
    markReadSuccess(dt);
    return true;
  }
  return false;
}
void RtcDriver::setSoftwareTime(const DateTime &dt) {
  _cachedTime = dt;
  _hasCachedTime = true;
  _lastReadTime = millis();
  seedSoftwareClock(dt);
}
DateTime RtcDriver::getSoftwareTime() const {
  if (_hasSoftwareTime)
    return calculateSoftwareTime();
  if (_hasCachedTime)
    return _cachedTime;
  return RTC_DEFAULT_TIME;
}
void RtcDriver::setSecond(uint8_t seconds) {
  if (writeRegister(RX8010_REG_SEC, decToBcd(seconds)))
    _lastReadTime = 0;
}
uint8_t RtcDriver::getSecond() { return getTime().second; }
void RtcDriver::setMinute(uint8_t minutes) {
  if (writeRegister(RX8010_REG_MIN, decToBcd(minutes)))
    _lastReadTime = 0;
}
uint8_t RtcDriver::getMinute() { return getTime().minute; }
void RtcDriver::setHour(uint8_t hours) {
  if (writeRegister(RX8010_REG_HOUR, decToBcd(hours)))
    _lastReadTime = 0;
}
uint8_t RtcDriver::getHour() { return getTime().hour; }
void RtcDriver::setDay(uint8_t day) {
  if (writeRegister(RX8010_REG_DAY, decToBcd(day)))
    _lastReadTime = 0;
}
uint8_t RtcDriver::getDay() { return getTime().day; }
void RtcDriver::setWeek(uint8_t week) {
  if (writeRegister(RX8010_REG_WEEK, weekBinToBitmask(week)))
    _lastReadTime = 0;
}
uint8_t RtcDriver::getWeek() { return getTime().week; }
void RtcDriver::setMonth(uint8_t month) {
  if (writeRegister(RX8010_REG_MONTH, decToBcd(month)))
    _lastReadTime = 0;
}
uint8_t RtcDriver::getMonth() { return getTime().month; }
void RtcDriver::setYear(uint16_t year) {
  uint8_t y = (year >= 2000) ? (year - 2000) : year;
  if (writeRegister(RX8010_REG_YEAR, decToBcd(y)))
    _lastReadTime = 0;
}
uint16_t RtcDriver::getYear() { return getTime().year + 2000; }
time_t RtcDriver::getTimeAsTimeT() {
  DateTime dt = getTime();
  struct tm t = {0};
  t.tm_sec = dt.second;
  t.tm_min = dt.minute;
  t.tm_hour = dt.hour;
  t.tm_mday = dt.day;
  t.tm_mon = dt.month - 1;
  t.tm_year = dt.year + 100;
  return mktime(&t);
}
void RtcDriver::setTime(uint8_t hour, uint8_t minute, uint8_t second) {
  DateTime dt = getTime();
  dt.hour = hour;
  dt.minute = minute;
  dt.second = second;
  setTime(dt);
}
void RtcDriver::setDate(uint8_t day, uint8_t month, uint16_t year) {
  DateTime dt = getTime();
  dt.day = day;
  dt.month = month;
  dt.year = (year >= 2000) ? (year - 2000) : year;
  struct tm t = {0};
  t.tm_mday = day;
  t.tm_mon = month - 1;
  t.tm_year = dt.year + 100;
  mktime(&t);
  dt.week = t.tm_wday;
  setTime(dt);
}
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
