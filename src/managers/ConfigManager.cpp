#include "ConfigManager.h"

ConfigManager::ConfigManager() {
  // Defaults
  config.volume = 10;
  config.time_format_24 = true;
  config.volume = 10;
  config.time_format_24 = true;
  for (int i = 0; i < 8; i++) {
    config.radio_presets[i] = 0;
  }
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
  for (int i = 0; i < 8; i++) {
    char key[16];
    sprintf(key, "radio_p%d", i);
    config.radio_presets[i] = prefs.getUShort(key, 0);
  }
}

void ConfigManager::save() {
  prefs.putString("ssid", config.wifi_ssid);
  prefs.putString("pass", config.wifi_pass);
  prefs.putUChar("vol", config.volume);
  prefs.putBool("fmt24", config.time_format_24);
  for (int i = 0; i < 8; i++) {
    char key[16];
    sprintf(key, "radio_p%d", i);
    prefs.putUShort(key, config.radio_presets[i]);
  }
}
