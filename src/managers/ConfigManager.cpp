#include "ConfigManager.h"

ConfigManager::ConfigManager() {
  // Defaults
  config.volume = 10;
  config.time_format_24 = true;
}

void ConfigManager::begin() {
  prefs.begin("clock_cfg", false);
  load();
}

void ConfigManager::load() {
  config.wifi_ssid = prefs.getString("ssid", "");
  config.wifi_pass = prefs.getString("pass", "");
  config.volume = prefs.getUChar("vol", 10);
  config.time_format_24 = prefs.getBool("fmt24", true);
}

void ConfigManager::save() {
  prefs.putString("ssid", config.wifi_ssid);
  prefs.putString("pass", config.wifi_pass);
  prefs.putUChar("vol", config.volume);
  prefs.putBool("fmt24", config.time_format_24);
}
