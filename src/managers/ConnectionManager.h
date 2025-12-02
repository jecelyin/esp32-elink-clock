#pragma once

#include "../drivers/RtcDriver.h"
#include "ConfigManager.h"
#include <WiFi.h>
#include <time.h>

class ConnectionManager {
public:
  ConnectionManager();
  void begin(ConfigManager *config, RtcDriver *rtc);
  void loop();
  bool isConnected();
  void startAP();
  void syncTime();

private:
  ConfigManager *configMgr;
  RtcDriver *rtcDriver;
  unsigned long lastSyncTime = 0;
};
