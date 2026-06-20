#include "ConfigManager.h"

ConfigManager::ConfigManager() {
  // Defaults
  config.volume = 10;
  config.time_format_24 = true;
  config.hw_checked = false;
  config.hw_check_version = 0;
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
  config.hw_checked = prefs.getBool("hwck", false);
  config.hw_check_version = prefs.getUChar("hwv", 0);
  for (int i = 0; i < 8; i++) {
    char key[16];
    snprintf(key, sizeof(key), "radio_p%d", i);
    config.radio_presets[i] = prefs.getUShort(key, 0);
  }
}

void ConfigManager::save() {
  prefs.putString("ssid", config.wifi_ssid);
  prefs.putString("pass", config.wifi_pass);
  prefs.putUChar("vol", config.volume);
  prefs.putBool("fmt24", config.time_format_24);
  prefs.putBool("hwck", config.hw_checked);
  prefs.putUChar("hwv", config.hw_check_version);
  for (int i = 0; i < 8; i++) {
    char key[16];
    snprintf(key, sizeof(key), "radio_p%d", i);
    prefs.putUShort(key, config.radio_presets[i]);
  }
}
