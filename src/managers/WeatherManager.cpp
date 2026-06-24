#include "WeatherManager.h"
#include "WeatherIcons.h"
#include "WeatherRequestHelper.h"
#include <WiFi.h>

namespace {
constexpr uint32_t WEATHER_UPDATE_INTERVAL_MS = 1800000UL;
constexpr uint32_t WEATHER_RETRY_INTERVAL_MS = 60000UL;
} // namespace

#define WEATHER_CITY_ID "101280112" // Nansha, Guangzhou

// QWeather (HeFeng) API URLs
// User specified host: ne4ewr7vn6.re.qweatherapi.com
// Current Weather: https://ne4ewr7vn6.re.qweatherapi.com/v7/weather/now
const char *current_weather_url =
    "https://ne4ewr7vn6.re.qweatherapi.com/v7/weather/"
    "now?location=" WEATHER_CITY_ID "&lang=zh&unit=m";

// Forecast: https://ne4ewr7vn6.re.qweatherapi.com/v7/weather/3d
const char *forecast_weather_url =
    "https://ne4ewr7vn6.re.qweatherapi.com/v7/weather/"
    "3d?location=" WEATHER_CITY_ID "&lang=zh&unit=m";

// Hourly: https://ne4ewr7vn6.re.qweatherapi.com/v7/weather/24h
const char *hourly_weather_url =
    "https://ne4ewr7vn6.re.qweatherapi.com/v7/weather/"
    "24h?location=" WEATHER_CITY_ID "&lang=zh&unit=m";

// Daily 7d: https://ne4ewr7vn6.re.qweatherapi.com/v7/weather/7d
const char *daily_weather_url =
    "https://ne4ewr7vn6.re.qweatherapi.com/v7/weather/"
    "7d?location=" WEATHER_CITY_ID "&lang=zh&unit=m";

// Warning: https://ne4ewr7vn6.re.qweatherapi.com/v7/warning/now
const char *warning_weather_url =
    "https://ne4ewr7vn6.re.qweatherapi.com/v7/warning/"
    "now?location=" WEATHER_CITY_ID "&lang=zh&unit=m";

WeatherManager::WeatherManager() {
  data.city = "南沙"; // Default city name
  data.weather = "--";
  data.temp = 0;
  data.humidity = 0;
  data.icon_str = resolveWeatherIcon(999);
  data.icon_code = 999;

  data.warning_title = "";
  data.warning_text = "Loading...";

  data.forecast_weather = "--";
  data.forecast_temp_high = 0;
  data.forecast_temp_low = 0;
  data.forecast_code = 999;
  data.forecast_icon_str = resolveWeatherIcon(999);
}

void WeatherManager::begin(ConfigManager *config) { configMgr = config; }

void WeatherManager::resetUpdateSchedule() {
  // 关键逻辑：API Token 变更后要立即清空节流状态，
  // 否则刚保存的新配置可能还会继续等待上一次失败请求的重试窗口。
  lastUpdate = 0;
  lastAttemptTime = 0;
}

void WeatherManager::update() {
  uint32_t now = millis();
  if (!canStartUpdate(now)) {
    return;
  }

  // 只在整批天气数据真正拉取成功后才刷新 lastUpdate，
  // 避免把失败请求误判成“天气已更新”，导致上层提前关掉 WiFi。
  updateInProgress = true;
  lastAttemptTime = now;
  bool success = updateWeatherBatch();
  updateInProgress = false;

  if (success) {
    lastUpdate = millis();
    Serial.println("Weather update completed");
  } else {
    Serial.println("Weather update failed");
  }
}

bool WeatherManager::canStartUpdate(uint32_t now) const {
  if (updateInProgress || WiFi.status() != WL_CONNECTED) {
    return false;
  }

  if (lastAttemptTime != 0 &&
      now - lastAttemptTime < WEATHER_RETRY_INTERVAL_MS) {
    return false;
  }

  return lastUpdate == 0 || now - lastUpdate >= WEATHER_UPDATE_INTERVAL_MS;
}

bool WeatherManager::updateWeatherBatch() {
  bool currentOk = fetchCurrentWeather();
  vTaskDelay(pdMS_TO_TICKS(200));
  bool forecastOk = fetchForecastWeather();
  vTaskDelay(pdMS_TO_TICKS(200));
  bool hourlyOk = fetchHourlyWeather();
  vTaskDelay(pdMS_TO_TICKS(200));
  bool dailyOk = fetchDailyWeather();
  vTaskDelay(pdMS_TO_TICKS(200));
  fetchWarning();
  return currentOk && forecastOk && hourlyOk && dailyOk;
}

bool WeatherManager::fetchCurrentWeather() {
  bool apiOk = false;
  String apiToken =
      configMgr == nullptr ? "" : configMgr->getWeatherApiToken();
  bool requestOk = requestWeatherApi(
      current_weather_url, "current weather", apiToken.c_str(),
                                     [this, &apiOk](JsonDocument &doc) {
    JsonObject now = doc["now"];
    String code = doc["code"];
    if (code == "200") {
      apiOk = true;
      data.temp = now["temp"].as<int>();
      data.weather = now["text"].as<String>();
      data.humidity = now["humidity"].as<int>();
      data.icon_code = now["icon"].as<int>();
      data.icon_str = resolveWeatherIcon(data.icon_code);

      // Parse obsTime: 2021-11-15T16:35+08:00 -> 11月15日 16:35
      String obsTime = now["obsTime"].as<String>();
      int tIndex = obsTime.indexOf('T');
      if (tIndex != -1) {
        String datePart = obsTime.substring(5, tIndex);              // 11-15
        String timePart = obsTime.substring(tIndex + 1, tIndex + 6); // 16:35
        int dashIndex = datePart.indexOf('-');
        if (dashIndex != -1) {
          data.obs_time = datePart.substring(0, dashIndex) + "月" +
                          datePart.substring(dashIndex + 1) + "日 " + timePart;
        } else {
          data.obs_time = obsTime;
        }
      }
    } else {
      Serial.print("API Error Code: ");
      Serial.println(code);
    }
  });
  return requestOk && apiOk;
}

bool WeatherManager::fetchForecastWeather() {
  bool apiOk = false;
  String apiToken =
      configMgr == nullptr ? "" : configMgr->getWeatherApiToken();
  bool requestOk = requestWeatherApi(
      forecast_weather_url, "forecast weather", apiToken.c_str(),
                                     [this, &apiOk](JsonDocument &doc) {
    String code = doc["code"];
    if (code == "200") {
      apiOk = true;
      JsonArray daily = doc["daily"];
      if (daily.size() >= 2) {
        JsonObject tomorrow = daily[1];

        data.forecast_weather = tomorrow["textDay"].as<String>();
        data.forecast_temp_high = tomorrow["tempMax"].as<int>();
        data.forecast_temp_low = tomorrow["tempMin"].as<int>();
        data.forecast_code = tomorrow["iconDay"].as<int>();
        data.forecast_icon_str = resolveWeatherIcon(data.forecast_code);
      }
    } else {
      Serial.print("API Error (Forecast) Code: ");
      Serial.println(code);
    }
  });
  return requestOk && apiOk;
}

bool WeatherManager::fetchHourlyWeather() {
  bool apiOk = false;
  String apiToken =
      configMgr == nullptr ? "" : configMgr->getWeatherApiToken();
  bool requestOk = requestWeatherApi(
      hourly_weather_url, "hourly weather", apiToken.c_str(),
                                     [this, &apiOk](JsonDocument &doc) {
    String code = doc["code"];
    if (code == "200") {
      apiOk = true;
      JsonArray hourlyItems = doc["hourly"];
      data.hourly.clear();
      // We only take the first 12 hours for the UI
      for (int i = 0; i < hourlyItems.size() && i < 12; i++) {
        JsonObject item = hourlyItems[i];
        HourlyData hData;
        String fxTime = item["fxTime"].as<String>();
        // Extract HH:00 from ISO time
        int tIndex = fxTime.indexOf('T');
        if (tIndex != -1) {
          hData.time = fxTime.substring(tIndex + 1, tIndex + 6);
        } else {
          hData.time = fxTime;
        }
        hData.temp = item["temp"].as<int>();
        hData.icon_code = item["icon"].as<int>();
        hData.icon_str = resolveWeatherIcon(hData.icon_code);
        data.hourly.push_back(hData);
      }
    }
  });
  return requestOk && apiOk;
}

bool WeatherManager::fetchDailyWeather() {
  bool apiOk = false;
  String apiToken =
      configMgr == nullptr ? "" : configMgr->getWeatherApiToken();
  bool requestOk = requestWeatherApi(
      daily_weather_url, "daily weather", apiToken.c_str(),
                                     [this, &apiOk](JsonDocument &doc) {
    String code = doc["code"];
    if (code == "200") {
      apiOk = true;
      JsonArray dailyItems = doc["daily"];
      data.daily.clear();
      for (int i = 0; i < dailyItems.size() && i < 7; i++) {
        JsonObject item = dailyItems[i];
        DailyData dData;
        dData.date = item["fxDate"].as<String>();

        // Simple day extraction or mapping if possible,
        // for now we'll just use the date or a placeholder
        dData.day = dData.date.substring(5); // MM-DD

        dData.temp_max = item["tempMax"].as<int>();
        dData.temp_min = item["tempMin"].as<int>();
        dData.icon_code = item["iconDay"].as<int>();
        dData.icon_str = resolveWeatherIcon(dData.icon_code);
        data.daily.push_back(dData);
      }
    }
  });
  return requestOk && apiOk;
}

bool WeatherManager::fetchWarning() {
  bool apiOk = false;
  String apiToken =
      configMgr == nullptr ? "" : configMgr->getWeatherApiToken();
  bool requestOk = requestWeatherApi(
      warning_weather_url, "weather warning", apiToken.c_str(),
                                     [this, &apiOk](JsonDocument &doc) {
    // Note: The warning API response contains an "alerts" array, not "warning".
    // It also has a metadata.zeroResult flag.
    String code = doc["code"];
    if (code == "200") {
      apiOk = true;
      JsonArray alerts = doc["alerts"];
      if (alerts.size() > 0) {
        JsonObject w = alerts[0];
        data.warning_title = w["title"].as<String>();
        data.warning_text = w["text"].as<String>();
      } else {
        // Check for metadata.zeroResult if available, otherwise assume no
        // warnings
        if (doc["metadata"].is<JsonObject>() &&
            doc["metadata"]["zeroResult"].as<bool>()) {
          data.warning_title = "";
          data.warning_text = "";
        } else {
          data.warning_title = "";
          data.warning_text = "";
        }
      }
    } else {
      data.warning_title = "";
      data.warning_text = "Warning info unavailable.";
    }
  });
  return requestOk && apiOk;
}
