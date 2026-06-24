#pragma once

#include "../drivers/RtcDriver.h"
#include "ConfigManager.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

struct HolidayOverride {
  uint32_t dateKey;
  bool isOffDay;
  String name;
};

struct HolidayYearCache {
  uint16_t year = 0;
  bool loaded = false;
  bool fetchFailed = false;
  uint32_t lastFetchAttemptMs = 0;
  std::vector<HolidayOverride> overrides;
};

enum HolidayDayType {
  HOLIDAY_DAY_WORKDAY,
  HOLIDAY_DAY_WEEKEND,
  HOLIDAY_DAY_OFFDAY,
  HOLIDAY_DAY_MAKEUP_WORKDAY
};

struct HolidayCountdown {
  String name = "";
  uint16_t days = 0;
  bool valid = false;
};

class HolidayCalendar {
public:
  HolidayCalendar();
  ~HolidayCalendar();
  void begin(ConfigManager *config);

  HolidayDayType getDayType(const DateTime &date);
  String getHolidayName(const DateTime &date);
  HolidayCountdown getNextHolidayCountdown(const DateTime &date,
                                           uint16_t maxDays);
  bool isWorkday(const DateTime &date);
  String getStatusText(uint16_t fullYear) const;
  void updateIfNeeded(const DateTime &now);
  void resetFetchState();

private:
  static const uint32_t FETCH_RETRY_INTERVAL_MS = 21600000UL;

  mutable SemaphoreHandle_t cacheMutex;
  std::vector<HolidayYearCache> caches;
  ConfigManager *configMgr;

  void ensureYearLoaded(uint16_t fullYear);
  void advanceDate(DateTime &date) const;
  HolidayYearCache getCacheSnapshot(uint16_t fullYear) const;
  uint32_t getDateKey(const DateTime &date) const;
  uint32_t getDateKey(uint16_t fullYear, uint8_t month, uint8_t day) const;
  uint16_t getFullYear(const DateTime &date) const;
  uint8_t getMonthDays(uint16_t fullYear, uint8_t month) const;
  String getRemoteUrl(uint16_t fullYear) const;
  String getStoragePath(uint16_t fullYear) const;
  bool hasLoadedCache(const HolidayYearCache &cache) const;
  bool isLeapYear(uint16_t fullYear) const;
  bool isWeekend(const DateTime &date) const;
  bool loadYearFromFile(uint16_t fullYear);
  bool parseCacheDocument(const String &json, uint16_t fullYear,
                          HolidayYearCache &cache) const;
  bool readTextFile(const String &path, String &content) const;
  bool saveTextFile(const String &path, const String &content) const;
  void syncYearIfNeeded(uint16_t fullYear);
  bool shouldFetchYear(const HolidayYearCache &cache) const;
  bool tryFetchYear(uint16_t fullYear);
  bool tryFetchYearBody(uint16_t fullYear, String &body) const;
  bool tryResolveHoliday(const DateTime &date, HolidayOverride &holiday);
  bool tryResolveHolidayInCache(const HolidayYearCache &cache, uint32_t dateKey,
                                HolidayOverride &holiday) const;
  void upsertCache(const HolidayYearCache &cache);
};
