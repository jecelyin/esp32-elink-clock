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
  void enableNetwork(bool enable);

  bool hasPendingSync() const { return pendingSync; }
  DateTime getNtpTime() const { return ntpTime; }
  void clearPendingSync() { pendingSync = false; }

private:
  ConfigManager *configMgr;
  RtcDriver *rtcDriver;
  unsigned long lastSyncTime = 0;
  bool networkEnabled = false;
  bool firstConnectAttempted = false;
  bool pendingSync = false;
  DateTime ntpTime;
};
