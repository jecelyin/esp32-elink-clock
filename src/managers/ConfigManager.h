#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct AppConfig {
  String wifi_ssid;
  String wifi_pass;
  uint8_t volume;
  bool time_format_24;
  // Add more config items as needed
};

class ConfigManager {
public:
  ConfigManager();
  void begin();
  void load();
  void save();

  AppConfig config;

private:
  Preferences prefs;
};
