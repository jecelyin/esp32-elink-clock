#include "HolidayCalendar.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

HolidayCalendar::HolidayCalendar() {
  cacheMutex = xSemaphoreCreateMutex();
  configMgr = nullptr;
}

HolidayCalendar::~HolidayCalendar() {
  if (cacheMutex != nullptr) {
    vSemaphoreDelete(cacheMutex);
  }
}

void HolidayCalendar::begin(ConfigManager *config) { configMgr = config; }

HolidayDayType HolidayCalendar::getDayType(const DateTime &date) {
  HolidayOverride holiday;
  if (tryResolveHoliday(date, holiday)) {
    // 法定日历中的放假和调休记录优先于星期规则，确保周末调休也能响铃。
    return holiday.isOffDay ? HOLIDAY_DAY_OFFDAY : HOLIDAY_DAY_MAKEUP_WORKDAY;
  }

  // 没有法定安排记录的普通日期，继续使用周末基础规则。
  return isWeekend(date) ? HOLIDAY_DAY_WEEKEND : HOLIDAY_DAY_WORKDAY;
}

bool HolidayCalendar::isWorkday(const DateTime &date) {
  HolidayDayType type = getDayType(date);
  return type == HOLIDAY_DAY_WORKDAY || type == HOLIDAY_DAY_MAKEUP_WORKDAY;
}

String HolidayCalendar::getHolidayName(const DateTime &date) {
  HolidayOverride holiday;
  if (tryResolveHoliday(date, holiday)) {
    return holiday.isOffDay ? holiday.name : "";
  }
  return "";
}

HolidayCountdown
HolidayCalendar::getNextHolidayCountdown(const DateTime &date,
                                         uint16_t maxDays) {
  HolidayCountdown countdown;
  DateTime cursor = date;
  for (uint16_t offset = 0; offset <= maxDays; ++offset) {
    HolidayOverride holiday;
    if (tryResolveHoliday(cursor, holiday) && holiday.isOffDay &&
        holiday.name.length() > 0) {
      countdown.name = holiday.name;
      countdown.days = offset;
      countdown.valid = true;
      return countdown;
    }
    advanceDate(cursor);
  }
  return countdown;
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
  if (now.month == 1 && currentYear > 2000) {
    syncYearIfNeeded(currentYear - 1);
  }
  if (now.month == 12 && now.day >= 25) {
    syncYearIfNeeded(currentYear + 1);
  }
}

void HolidayCalendar::resetFetchState() {
  if (cacheMutex == nullptr) {
    return;
  }

  xSemaphoreTake(cacheMutex, portMAX_DELAY);
  for (HolidayYearCache &cache : caches) {
    if (!cache.loaded) {
      cache.fetchFailed = false;
      cache.lastFetchAttemptMs = 0;
    }
  }
  xSemaphoreGive(cacheMutex);
}

void HolidayCalendar::ensureYearLoaded(uint16_t fullYear) {
  HolidayYearCache cache = getCacheSnapshot(fullYear);
  if (cache.year == fullYear) {
    return;
  }
  if (loadYearFromFile(fullYear)) {
    return;
  }

  // 关键逻辑：没有本地假期文件时也写入一个空缓存占位，
  // 避免月历绘制 31 个日期时反复打开同一个不存在的 SPIFFS 文件。
  HolidayYearCache missingCache;
  missingCache.year = fullYear;
  upsertCache(missingCache);
}

void HolidayCalendar::advanceDate(DateTime &date) const {
  uint16_t fullYear = getFullYear(date);
  date.week = static_cast<uint8_t>((date.week + 1) % 7);
  date.day++;
  if (date.day <= getMonthDays(fullYear, date.month)) {
    return;
  }

  date.day = 1;
  date.month++;
  if (date.month <= 12) {
    return;
  }
  date.month = 1;
  date.year++;
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

uint32_t HolidayCalendar::getDateKey(const DateTime &date) const {
  return getDateKey(getFullYear(date), date.month, date.day);
}

uint32_t HolidayCalendar::getDateKey(uint16_t fullYear, uint8_t month,
                                     uint8_t day) const {
  return static_cast<uint32_t>(fullYear) * 10000UL +
         static_cast<uint32_t>(month) * 100UL + day;
}

uint16_t HolidayCalendar::getFullYear(const DateTime &date) const {
  return static_cast<uint16_t>(date.year) + 2000;
}

uint8_t HolidayCalendar::getMonthDays(uint16_t fullYear, uint8_t month) const {
  static const uint8_t days[] = {31, 28, 31, 30, 31, 30,
                                 31, 31, 30, 31, 30, 31};
  if (month == 2 && isLeapYear(fullYear)) {
    return 29;
  }
  return days[month - 1];
}

String HolidayCalendar::getRemoteUrl(uint16_t fullYear) const {
  return "https://v1.apizero.cn/api/holiday?year=" +
         String(fullYear);
}

String HolidayCalendar::getStoragePath(uint16_t fullYear) const {
  return "/holiday_apizero_" + String(fullYear) + ".json";
}

bool HolidayCalendar::hasLoadedCache(const HolidayYearCache &cache) const {
  return cache.loaded && !cache.overrides.empty();
}

bool HolidayCalendar::isLeapYear(uint16_t fullYear) const {
  return (fullYear % 4 == 0 && fullYear % 100 != 0) || fullYear % 400 == 0;
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

  if (!doc["code"].is<int>() || doc["code"].as<int>() != 0) {
    Serial.printf("Holiday API rejected year=%u code=%d message=%s\n",
                  fullYear, doc["code"].as<int>(), doc["msg"] | "missing code");
    return false;
  }

  JsonObject data = doc["data"].as<JsonObject>();
  if (data["year"].as<uint16_t>() != fullYear ||
      !data["list"].is<JsonArray>()) {
    Serial.printf("Holiday response year/list invalid: expected=%u actual=%u\n",
                  fullYear, data["year"].as<uint16_t>());
    return false;
  }

  cache = HolidayYearCache();
  cache.year = fullYear;
  cache.loaded = true;
  for (JsonObject item : data["list"].as<JsonArray>()) {
    String name = item["name"].as<String>();
    if (name.length() == 0 ||
        !appendDateOverrides(item["holiday"], name, true, cache) ||
        !appendDateOverrides(item["workday"], name, false, cache)) {
      Serial.printf("Holiday date arrays invalid: year=%u name=%s\n",
                    fullYear, name.c_str());
      return false;
    }
  }
  return !cache.overrides.empty();
}

bool HolidayCalendar::appendDateOverrides(JsonVariantConst dates,
                                         const String &name, bool isOffDay,
                                         HolidayYearCache &cache) const {
  if (!dates.is<JsonArrayConst>()) {
    return false;
  }
  for (JsonVariantConst value : dates.as<JsonArrayConst>()) {
    HolidayOverride entry;
    if (!parseDateKey(value, entry.dateKey)) {
      return false;
    }
    entry.isOffDay = isOffDay;
    entry.name = name;
    cache.overrides.push_back(entry);
  }
  return true;
}

bool HolidayCalendar::parseDateKey(JsonVariantConst value,
                                   uint32_t &dateKey) const {
  if (!value.is<const char *>()) {
    return false;
  }
  String date = value.as<String>();
  if (date.length() != 10 || date[4] != '-' || date[7] != '-') {
    return false;
  }
  for (uint8_t i = 0; i < 10; ++i) {
    if (i != 4 && i != 7 && (date[i] < '0' || date[i] > '9')) {
      return false;
    }
  }
  uint16_t year = date.substring(0, 4).toInt();
  uint8_t month = date.substring(5, 7).toInt();
  uint8_t day = date.substring(8, 10).toInt();
  if (year < 2000 || month < 1 || month > 12 || day < 1 ||
      day > getMonthDays(year, month)) {
    return false;
  }
  dateKey = getDateKey(year, month, day);
  return true;
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
  // 仅发送新服务格式的 Key，避免升级后把旧服务 Token 发给新提供方。
  String apiKey = configMgr == nullptr ? "" : configMgr->getHolidayApiToken();
  bool hasApiKey = apiKey.startsWith("sk_live_") ||
                   apiKey.startsWith("sk_test_") ||
                   apiKey.startsWith("sk_stag_");
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
  if (hasApiKey) {
    http.addHeader("Authorization", "Bearer " + apiKey);
  }
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

bool HolidayCalendar::tryResolveHoliday(const DateTime &date,
                                        HolidayOverride &holiday) {
  uint16_t fullYear = getFullYear(date);
  uint32_t dateKey = getDateKey(date);
  ensureYearLoaded(fullYear);
  HolidayYearCache cache = getCacheSnapshot(fullYear);
  if (hasLoadedCache(cache) &&
      tryResolveHolidayInCache(cache, dateKey, holiday)) {
    return true;
  }

  if (date.month != 1 || fullYear <= 2000) {
    return false;
  }

  // 假期安排可能跨年，查询 1 月时也检查上一年缓存中的跨年日期。
  ensureYearLoaded(fullYear - 1);
  HolidayYearCache previousCache = getCacheSnapshot(fullYear - 1);
  return hasLoadedCache(previousCache) &&
         tryResolveHolidayInCache(previousCache, dateKey, holiday);
}

bool HolidayCalendar::tryResolveHolidayInCache(
    const HolidayYearCache &cache, uint32_t dateKey,
    HolidayOverride &holiday) const {
  for (size_t i = 0; i < cache.overrides.size(); ++i) {
    if (cache.overrides[i].dateKey != dateKey) {
      continue;
    }
    holiday = cache.overrides[i];
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
