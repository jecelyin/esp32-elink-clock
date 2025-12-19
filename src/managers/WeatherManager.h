#pragma once

#include "ConfigManager.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <functional>
#include <vector>

struct HourlyData {
  String time;
  int temp;
  int icon_code;
  const char *icon_str;
};

struct DailyData {
  String date;
  String day;
  int temp_max;
  int temp_min;
  int icon_code;
  const char *icon_str;
};

struct WeatherData {
  String city;
  String weather;
  String obs_time;
  int temp;
  int humidity;
  const char *icon_str; // u8g2 icon character
  int icon_code;

  String warning_title;
  String warning_text;

  std::vector<HourlyData> hourly;
  std::vector<DailyData> daily;

  // Forecast (Tomorrow) - Keep for compatibility if needed, but we'll use
  // daily[1]
  String forecast_weather;
  int forecast_temp_high;
  int forecast_temp_low;
  int forecast_code;
  const char *forecast_icon_str;
};

class WeatherManager {
public:
  WeatherManager();
  void begin(ConfigManager *config);
  void update();
  WeatherData data;

private:
  ConfigManager *configMgr;
  unsigned long lastUpdate = 0;

  void fetchCurrentWeather();
  void fetchForecastWeather();
  void fetchHourlyWeather();
  void fetchDailyWeather();
  void fetchWarning();
  const char *getIcon(int code);
  bool requestAPI(const char *url,
                  std::function<void(JsonDocument &)> callback);
};
