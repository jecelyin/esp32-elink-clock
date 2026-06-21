#pragma once

#include <Arduino.h>

struct LunarDate {
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  bool isLeapMonth = false;
  bool valid = false;
};

struct LunarFestivalCountdown {
  String name = "";
  uint16_t days = 0;
  bool valid = false;
};

class LunarCalendar {
public:
  static LunarDate fromSolar(uint16_t year, uint8_t month, uint8_t day);
  static String getDayLabel(uint16_t year, uint8_t month, uint8_t day);
  static String getFullDateLabel(uint16_t year, uint8_t month, uint8_t day);
  static LunarFestivalCountdown
  getNextFestivalCountdown(uint16_t year, uint8_t month, uint8_t day);
  static String getSolarFestivalLabel(uint16_t year, uint8_t month,
                                      uint8_t day);
  static String getYearLabel(uint16_t year, uint8_t month, uint8_t day);

private:
  static const uint16_t BASE_YEAR = 1900;
  static const uint8_t BASE_DAY = 31;

  static const uint32_t LUNAR_INFO[];
  static const uint16_t LUNAR_YEAR_COUNT;

  static bool isGregorianLeapYear(uint16_t year);
  static bool isSupportedYear(uint16_t year);
  static int32_t getSolarOffset(uint16_t year, uint8_t month, uint8_t day);
  static uint8_t getGregorianMonthDays(uint16_t year, uint8_t month);
  static uint8_t getLeapMonth(uint16_t lunarYear);
  static uint8_t getLeapMonthDays(uint16_t lunarYear);
  static uint8_t getLunarMonthDays(uint16_t lunarYear, uint8_t lunarMonth);
  static uint16_t getLunarYearDays(uint16_t lunarYear);
  static void advanceSolarDate(uint16_t &year, uint8_t &month, uint8_t &day);
  static String getCountdownFestival(const LunarDate &date, uint16_t solarYear,
                                     uint8_t solarMonth, uint8_t solarDay);
  static uint8_t getGregorianWeekday(uint16_t year, uint8_t month,
                                     uint8_t day);
  static const char *getLunarDayName(uint8_t lunarDay);
  static const char *getLunarFestival(const LunarDate &date);
  static const char *getLunarMonthName(uint8_t lunarMonth);
  static bool isNthWeekdayOfMonth(uint16_t year, uint8_t month, uint8_t day,
                                  uint8_t weekday, uint8_t nth);
};
