#include "ConnectionManager.h"
#include <WiFiManager.h>

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600; // UTC+8 for China
const int daylightOffset_sec = 0;
WiFiManager wifiManager;

ConnectionManager::ConnectionManager() {}

void ConnectionManager::begin(ConfigManager *config, RtcDriver *rtc) {
  configMgr = config;
  rtcDriver = rtc;
}

void ConnectionManager::enableNetwork(bool enable) { networkEnabled = enable; }

void ConnectionManager::loop() {
  if (!networkEnabled)
    return;

  if (!firstConnectAttempted) {
    firstConnectAttempted = true;
    wifiManager.autoConnect();
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Sync time every hour
    if (millis() - lastSyncTime > 3600000 || lastSyncTime == 0) {
      syncTime();
    }
  }
}

bool ConnectionManager::isConnected() { return WiFi.status() == WL_CONNECTED; }

void ConnectionManager::startAP() {
  // WiFi.softAP("ESP32-Clock", "12345678");
}

void ConnectionManager::syncTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    DateTime dt;
    dt.second = timeinfo.tm_sec;
    dt.minute = timeinfo.tm_min;
    dt.hour = timeinfo.tm_hour;
    dt.day = timeinfo.tm_mday;
    dt.month = timeinfo.tm_mon + 1;
    dt.year = timeinfo.tm_year % 100;
    dt.week = timeinfo.tm_wday;

    rtcDriver->setTime(dt);
    lastSyncTime = millis();
    Serial.println("Time synced with NTP");
  }
}
