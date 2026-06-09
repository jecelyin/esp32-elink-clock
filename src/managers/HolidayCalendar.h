#pragma once

#include "../drivers/RtcDriver.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

struct HolidayOverride {
  uint16_t monthDayKey;
  bool isOffDay;
};

struct HolidayYearCache {
  uint16_t year = 0;
  bool loaded = false;
  bool fetchFailed = false;
  uint32_t lastFetchAttemptMs = 0;
  std::vector<HolidayOverride> overrides;
};

class HolidayCalendar {
public:
  HolidayCalendar();
  ~HolidayCalendar();

  bool isWorkday(const DateTime &date);
  String getStatusText(uint16_t fullYear) const;
  void updateIfNeeded(const DateTime &now);

private:
  static const uint32_t FETCH_RETRY_INTERVAL_MS = 21600000UL;

  mutable SemaphoreHandle_t cacheMutex;
  std::vector<HolidayYearCache> caches;

  void ensureYearLoaded(uint16_t fullYear);
  HolidayYearCache getCacheSnapshot(uint16_t fullYear) const;
  uint16_t getFullYear(const DateTime &date) const;
  uint16_t getMonthDayKey(uint8_t month, uint8_t day) const;
  String getRemoteUrl(uint16_t fullYear) const;
  String getStoragePath(uint16_t fullYear) const;
  bool hasLoadedCache(const HolidayYearCache &cache) const;
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
  bool tryResolveOverride(const HolidayYearCache &cache, uint8_t month,
                          uint8_t day, bool &isWorkdayResult) const;
  void upsertCache(const HolidayYearCache &cache);
};
