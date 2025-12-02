#include "WeatherManager.h"
#include <WiFi.h>

// Placeholder API Key and URL - User needs to replace this
const char *weather_api_url =
    "http://api.openweathermap.org/data/2.5/"
    "weather?q=Beijing&appid=YOUR_API_KEY&units=metric";
// For forecast, we might need another endpoint or OneCall API

WeatherManager::WeatherManager() {
  data.city = "--";
  data.weather = "--";
  data.temp = 0;
  data.humidity = 0;
}

void WeatherManager::begin(ConfigManager *config) { configMgr = config; }

void WeatherManager::update() {
  if (millis() - lastUpdate > 1800000 ||
      lastUpdate == 0) { // Update every 30 mins
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(weather_api_url);
      int httpCode = http.GET();
      if (httpCode > 0) {
        String payload = http.getString();
        parseWeather(payload);
        lastUpdate = millis();
      }
      http.end();
    }
  }
}

void WeatherManager::parseWeather(String json) {
  JsonDocument doc;
  deserializeJson(doc, json);

  data.temp = doc["main"]["temp"];
  data.humidity = doc["main"]["humidity"];
  const char *w = doc["weather"][0]["main"];
  data.weather = String(w);
  const char *c = doc["name"];
  data.city = String(c);

  // Forecast parsing would go here if using OneCall or similar
  // For now, just placeholder
  data.forecast_weather = "Sunny";
  data.forecast_temp_high = 25;
  data.forecast_temp_low = 15;
}
