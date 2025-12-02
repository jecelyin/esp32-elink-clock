#pragma once

#include "ConfigManager.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

struct WeatherData {
  String city;
  String weather;
  int temp;
  int humidity;
  String forecast_weather;
  int forecast_temp_high;
  int forecast_temp_low;
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
  void parseWeather(String json);
};
