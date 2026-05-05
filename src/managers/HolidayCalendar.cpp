#include "HolidayCalendar.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

HolidayCalendar::HolidayCalendar() {
  cacheMutex = xSemaphoreCreateMutex();
}

HolidayCalendar::~HolidayCalendar() {
  if (cacheMutex != nullptr) {
    vSemaphoreDelete(cacheMutex);
  }
}

bool HolidayCalendar::isWorkday(const DateTime &date) {
  uint16_t fullYear = getFullYear(date);
  ensureYearLoaded(fullYear);

  HolidayYearCache cache = getCacheSnapshot(fullYear);
  bool isWorkdayResult = false;
  if (hasLoadedCache(cache) &&
      tryResolveOverride(cache, date.month, date.day, isWorkdayResult)) {
    return isWorkdayResult;
  }

  // 关键逻辑：法定日历未就绪时必须回退到“周一到周五”的基础规则，
  // 否则工作日闹钟会因为缺少远端数据直接失效。
  return !isWeekend(date);
}

String HolidayCalendar::getStatusText(uint16_t fullYear) const {
  HolidayYearCache cache = getCacheSnapshot(fullYear);
  if (hasLoadedCache(cache)) {
    return String(fullYear) + "年法定日历已缓存";
  }
  if (cache.fetchFailed) {
    return String(fullYear) + "年法定日历未同步，暂按周一到周五";
  }
  return String(fullYear) + "年法定日历待同步，当前按周一到周五";
}

void HolidayCalendar::updateIfNeeded(const DateTime &now) {
  uint16_t currentYear = getFullYear(now);
  syncYearIfNeeded(currentYear);
  if (now.month == 12 && now.day >= 25) {
    syncYearIfNeeded(currentYear + 1);
  }
}

void HolidayCalendar::ensureYearLoaded(uint16_t fullYear) {
  HolidayYearCache cache = getCacheSnapshot(fullYear);
  if (hasLoadedCache(cache)) {
    return;
  }
  loadYearFromFile(fullYear);
}

HolidayYearCache HolidayCalendar::getCacheSnapshot(uint16_t fullYear) const {
  HolidayYearCache snapshot;
  if (cacheMutex == nullptr) {
    return snapshot;
  }

  xSemaphoreTake(cacheMutex, portMAX_DELAY);
  for (size_t i = 0; i < caches.size(); ++i) {
    if (caches[i].year == fullYear) {
      snapshot = caches[i];
      break;
    }
  }
  xSemaphoreGive(cacheMutex);
  return snapshot;
}

uint16_t HolidayCalendar::getFullYear(const DateTime &date) const {
  return static_cast<uint16_t>(date.year) + 2000;
}

uint16_t HolidayCalendar::getMonthDayKey(uint8_t month, uint8_t day) const {
  return static_cast<uint16_t>(month) * 100 + day;
}

String HolidayCalendar::getRemoteUrl(uint16_t fullYear) const {
  return "https://cdn.jsdelivr.net/gh/NateScarlet/holiday-cn@master/" +
         String(fullYear) + ".json";
}

String HolidayCalendar::getStoragePath(uint16_t fullYear) const {
  return "/holiday_" + String(fullYear) + ".json";
}

bool HolidayCalendar::hasLoadedCache(const HolidayYearCache &cache) const {
  return cache.loaded && !cache.overrides.empty();
}

bool HolidayCalendar::isWeekend(const DateTime &date) const {
  return date.week == 0 || date.week == 6;
}

bool HolidayCalendar::loadYearFromFile(uint16_t fullYear) {
  String content;
  if (!readTextFile(getStoragePath(fullYear), content)) {
    return false;
  }

  HolidayYearCache cache;
  if (!parseCacheDocument(content, fullYear, cache)) {
    return false;
  }

  upsertCache(cache);
  return true;
}

bool HolidayCalendar::parseCacheDocument(const String &json, uint16_t fullYear,
                                         HolidayYearCache &cache) const {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.printf("Holiday json parse failed: %s\n", error.c_str());
    return false;
  }

  if (doc["year"].as<uint16_t>() != fullYear) {
    Serial.printf("Holiday year mismatch: %u\n", fullYear);
    return false;
  }

  cache = HolidayYearCache();
  cache.year = fullYear;
  cache.loaded = true;
  JsonArray days = doc["days"].as<JsonArray>();
  for (JsonObject item : days) {
    String date = item["date"].as<String>();
    if (date.length() < 10) {
      continue;
    }
    HolidayOverride entry;
    entry.monthDayKey = static_cast<uint16_t>(date.substring(5, 7).toInt() *
                                                  100 +
                                              date.substring(8, 10).toInt());
    entry.isOffDay = item["isOffDay"].as<bool>();
    cache.overrides.push_back(entry);
  }
  return !cache.overrides.empty();
}

bool HolidayCalendar::readTextFile(const String &path, String &content) const {
  File file = SPIFFS.open(path, FILE_READ);
  if (!file) {
    return false;
  }

  content = file.readString();
  file.close();
  return content.length() > 0;
}

bool HolidayCalendar::saveTextFile(const String &path,
                                   const String &content) const {
  if (SPIFFS.exists(path)) {
    SPIFFS.remove(path);
  }

  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file) {
    return false;
  }

  size_t written = file.print(content);
  file.close();
  return written == static_cast<size_t>(content.length());
}

void HolidayCalendar::syncYearIfNeeded(uint16_t fullYear) {
  ensureYearLoaded(fullYear);
  HolidayYearCache cache = getCacheSnapshot(fullYear);
  if (!shouldFetchYear(cache)) {
    return;
  }
  tryFetchYear(fullYear);
}

bool HolidayCalendar::shouldFetchYear(const HolidayYearCache &cache) const {
  if (cache.loaded) {
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  return cache.lastFetchAttemptMs == 0 ||
         millis() - cache.lastFetchAttemptMs >= FETCH_RETRY_INTERVAL_MS;
}

bool HolidayCalendar::tryFetchYear(uint16_t fullYear) {
  HolidayYearCache cache = getCacheSnapshot(fullYear);
  cache.year = fullYear;
  cache.lastFetchAttemptMs = millis();
  cache.fetchFailed = false;
  upsertCache(cache);

  String body;
  if (!tryFetchYearBody(fullYear, body)) {
    cache.fetchFailed = true;
    upsertCache(cache);
    return false;
  }

  HolidayYearCache remoteCache;
  if (!parseCacheDocument(body, fullYear, remoteCache) ||
      !saveTextFile(getStoragePath(fullYear), body)) {
    cache.fetchFailed = true;
    upsertCache(cache);
    return false;
  }

  remoteCache.lastFetchAttemptMs = cache.lastFetchAttemptMs;
  upsertCache(remoteCache);
  return true;
}

bool HolidayCalendar::tryFetchYearBody(uint16_t fullYear, String &body) const {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, getRemoteUrl(fullYear))) {
    Serial.printf("Holiday request begin failed: %u\n", fullYear);
    return false;
  }

  http.setReuse(false);
  http.setTimeout(8000);
  http.addHeader("Accept-Encoding", "identity");
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Holiday request failed: %d\n", httpCode);
    http.end();
    return false;
  }

  body = http.getString();
  http.end();
  return body.length() > 0;
}

bool HolidayCalendar::tryResolveOverride(const HolidayYearCache &cache,
                                         uint8_t month, uint8_t day,
                                         bool &isWorkdayResult) const {
  uint16_t monthDayKey = getMonthDayKey(month, day);
  for (size_t i = 0; i < cache.overrides.size(); ++i) {
    if (cache.overrides[i].monthDayKey != monthDayKey) {
      continue;
    }
    isWorkdayResult = !cache.overrides[i].isOffDay;
    return true;
  }
  return false;
}

void HolidayCalendar::upsertCache(const HolidayYearCache &cache) {
  if (cacheMutex == nullptr) {
    return;
  }

  xSemaphoreTake(cacheMutex, portMAX_DELAY);
  for (size_t i = 0; i < caches.size(); ++i) {
    if (caches[i].year == cache.year) {
      caches[i] = cache;
      xSemaphoreGive(cacheMutex);
      return;
    }
  }
  caches.push_back(cache);
  xSemaphoreGive(cacheMutex);
}
