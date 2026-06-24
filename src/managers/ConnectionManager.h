#pragma once

#include "../drivers/RtcDriver.h"
#include "ConfigManager.h"
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <time.h>

class ConnectionManager {
public:
  ConnectionManager();
  void begin(ConfigManager *config, RtcDriver *rtc);
  void loop();
  bool isConnected();
  void startAP();
  void startSystemAP();
  void syncTime();
  void flushPendingRtcSync();
  void enableNetwork(bool enable);
  void startScheduledSyncIfDue(uint32_t now);
  uint32_t getNextScheduledWorkDelayMs(uint32_t now) const;
  bool isNetworkEnabled() const;
  bool isSyncComplete();
  bool isSystemPortalActive() const;
  bool isConfigPortalActive() const;

private:
  void beginAutoConnect();
  void configurePortal(bool manual);
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
  unsigned long lastNetworkPowerOnTime = 0;
  uint8_t rtcSyncFailCount = 0;
  bool networkEnabled = false;
  bool firstConnectAttempted = false;
  bool pendingSync = false;
  bool systemPortalActive = false;
  mutable SemaphoreHandle_t networkMutex = nullptr;
  DateTime ntpTime;
  void lockNetwork() const;
  void unlockNetwork() const;
};
