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
  void flushPendingRtcSync();
  void enableNetwork(bool enable);
  bool isNetworkEnabled() const { return networkEnabled; }
  bool isSyncComplete();

private:
  void beginAutoConnect();
  uint32_t getRtcSyncRetryInterval() const;
  void powerOffNetwork();
  void powerOnNetwork();
  void retryWiFiConnection();
  void stopPortalIfActive();

  ConfigManager *configMgr;
  RtcDriver *rtcDriver;
  unsigned long lastSyncTime = 0;
  unsigned long lastReconnectAttempt = 0;
  unsigned long lastRtcSyncAttempt = 0;
  uint8_t rtcSyncFailCount = 0;
  bool networkEnabled = false;
  bool firstConnectAttempted = false;
  bool pendingSync = false;
  DateTime ntpTime;
};
