#pragma once

#include "ConfigManager.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <functional>

struct WeatherData {
  String city;
  String weather;
  int temp;
  int humidity;
  const char* icon_str; // u8g2 icon character
  
  // Forecast (Tomorrow)
  String forecast_weather;
  int forecast_temp_high;
  int forecast_temp_low;
  int forecast_code; 
  const char* forecast_icon_str;
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
  const char* getIcon(int code);
  bool requestAPI(const char* url, std::function<void(JsonDocument&)> callback);
};
