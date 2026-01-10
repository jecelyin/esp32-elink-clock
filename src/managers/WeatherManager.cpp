#include "WeatherManager.h"
#include <ArduinoUZlib.h>
#include <WiFi.h>

#define WEATHER_API_KEY "ba7d575ae307406f9455efd0b11abe18"
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
  data.icon_str = "\uf146"; // Default/Unknown
  data.icon_code = 999;

  data.warning_title = "";
  data.warning_text = "Loading...";

  data.forecast_weather = "--";
  data.forecast_temp_high = 0;
  data.forecast_temp_low = 0;
  data.forecast_code = 999;
  data.forecast_icon_str = "\uf146";
}

void WeatherManager::begin(ConfigManager *config) { configMgr = config; }

void WeatherManager::update() {
  if (millis() - lastUpdate > 1800000 ||
      lastUpdate == 0) { // Update every 30 mins
    if (WiFi.status() == WL_CONNECTED) {
      fetchCurrentWeather();
      vTaskDelay(pdMS_TO_TICKS(200));
      fetchForecastWeather();
      vTaskDelay(pdMS_TO_TICKS(200));
      fetchHourlyWeather();
      vTaskDelay(pdMS_TO_TICKS(200));
      fetchDailyWeather();
      vTaskDelay(pdMS_TO_TICKS(200));
      fetchWarning();
      lastUpdate = millis();
    }
  }
}

void WeatherManager::fetchCurrentWeather() {
  requestAPI(current_weather_url, [this](JsonDocument &doc) {
    JsonObject now = doc["now"];
    String code = doc["code"];
    if (code == "200") {
      data.temp = now["temp"].as<int>();
      data.weather = now["text"].as<String>();
      data.humidity = now["humidity"].as<int>();
      data.icon_code = now["icon"].as<int>();
      data.icon_str = getIcon(data.icon_code);

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
}

void WeatherManager::fetchForecastWeather() {
  requestAPI(forecast_weather_url, [this](JsonDocument &doc) {
    String code = doc["code"];
    if (code == "200") {
      JsonArray daily = doc["daily"];
      if (daily.size() >= 2) {
        JsonObject tomorrow = daily[1];

        data.forecast_weather = tomorrow["textDay"].as<String>();
        data.forecast_temp_high = tomorrow["tempMax"].as<int>();
        data.forecast_temp_low = tomorrow["tempMin"].as<int>();
        data.forecast_code = tomorrow["iconDay"].as<int>();
        data.forecast_icon_str = getIcon(data.forecast_code);
      }
    } else {
      Serial.print("API Error (Forecast) Code: ");
      Serial.println(code);
    }
  });
}

void WeatherManager::fetchHourlyWeather() {
  requestAPI(hourly_weather_url, [this](JsonDocument &doc) {
    String code = doc["code"];
    if (code == "200") {
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
        hData.icon_str = getIcon(hData.icon_code);
        data.hourly.push_back(hData);
      }
    }
  });
}

void WeatherManager::fetchDailyWeather() {
  requestAPI(daily_weather_url, [this](JsonDocument &doc) {
    String code = doc["code"];
    if (code == "200") {
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
        dData.icon_str = getIcon(dData.icon_code);
        data.daily.push_back(dData);
      }
    }
  });
}

void WeatherManager::fetchWarning() {
  requestAPI(warning_weather_url, [this](JsonDocument &doc) {
    // Note: The warning API response contains an "alerts" array, not "warning".
    // It also has a metadata.zeroResult flag.
    String code = doc["code"];
    if (code == "200") {
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
}

bool WeatherManager::requestAPI(const char *url,
                                std::function<void(JsonDocument &)> callback) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("X-QW-Api-Key", WEATHER_API_KEY);

  const char *headers[] = {"Content-Encoding"};
  http.collectHeaders(headers, 1);

  int httpCode = http.GET();
  bool success = false;

  if (httpCode > 0) {
    if (http.header("Content-Encoding").indexOf("gzip") > -1) {
      int len = http.getSize();
      if (len > 0) {
        uint8_t *response = (uint8_t *)malloc(len);
        http.getStream().readBytes(response, len);

        uint8_t *buffer = nullptr;
        uint32_t size = 0;
        // Decompress
        ArduinoUZlib::decompress(response, len, buffer, size);
        free(response);

        if (buffer) {
          JsonDocument doc;
          DeserializationError error = deserializeJson(doc, buffer, size);
          free(buffer);

          if (!error) {
            if (callback)
              callback(doc);
            success = true;
          } else {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
          }
        } else {
          Serial.println("Decompression failed (buffer null)");
        }
      }
    } else {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        if (callback)
          callback(doc);
        success = true;
      } else {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
      }
    }
  } else {
    Serial.printf("HTTP GET failed, error: %s\n",
                  http.errorToString(httpCode).c_str());
  }
  http.end();
  return success;
}

const char *WeatherManager::getIcon(int code) {
  switch (code) {
  case 100:
    return "\uf101";
  case 101:
    return "\uf102";
  case 102:
    return "\uf103";
  case 103:
    return "\uf104";
  case 104:
    return "\uf105";
  case 150:
    return "\uf106";
  case 151:
    return "\uf107";
  case 152:
    return "\uf108";
  case 153:
    return "\uf109";
  case 300:
    return "\uf10a";
  case 301:
    return "\uf10b";
  case 302:
    return "\uf10c";
  case 303:
    return "\uf10d";
  case 304:
    return "\uf10e";
  case 305:
    return "\uf10f";
  case 306:
    return "\uf110";
  case 307:
    return "\uf111";
  case 308:
    return "\uf112";
  case 309:
    return "\uf113";
  case 310:
    return "\uf114";
  case 311:
    return "\uf115";
  case 312:
    return "\uf116";
  case 313:
    return "\uf117";
  case 314:
    return "\uf118";
  case 315:
    return "\uf119";
  case 316:
    return "\uf11a";
  case 317:
    return "\uf11b";
  case 318:
    return "\uf11c";
  case 350:
    return "\uf11d";
  case 351:
    return "\uf11e";
  case 399:
    return "\uf11f";
  case 400:
    return "\uf120";
  case 401:
    return "\uf121";
  case 402:
    return "\uf122";
  case 403:
    return "\uf123";
  case 404:
    return "\uf124";
  case 405:
    return "\uf125";
  case 406:
    return "\uf126";
  case 407:
    return "\uf127";
  case 408:
    return "\uf128";
  case 409:
    return "\uf129";
  case 410:
    return "\uf12a";
  case 456:
    return "\uf12b";
  case 457:
    return "\uf12c";
  case 499:
    return "\uf12d";
  case 500:
    return "\uf12e";
  case 501:
    return "\uf12f";
  case 502:
    return "\uf130";
  case 503:
    return "\uf131";
  case 504:
    return "\uf132";
  case 507:
    return "\uf133";
  case 508:
    return "\uf134";
  case 509:
    return "\uf135";
  case 510:
    return "\uf136";
  case 511:
    return "\uf137";
  case 512:
    return "\uf138";
  case 513:
    return "\uf139";
  case 514:
    return "\uf13a";
  case 515:
    return "\uf13b";
  case 800:
    return "\uf13c";
  case 801:
    return "\uf13d";
  case 802:
    return "\uf13e";
  case 803:
    return "\uf13f";
  case 804:
    return "\uf140";
  case 805:
    return "\uf141";
  case 806:
    return "\uf142";
  case 807:
    return "\uf143";
  case 900:
    return "\uf144";
  case 901:
    return "\uf145";
  case 999:
  default:
    return "\uf146";
  }
}
