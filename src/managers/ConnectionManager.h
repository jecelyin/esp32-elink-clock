#pragma once

#include "../drivers/RtcDriver.h"
#include "ConfigManager.h"
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <time.h>

class ConnectionManager {
public:
  class NetworkGuard {
  public:
    explicit NetworkGuard(ConnectionManager *owner);
    ~NetworkGuard();

  private:
    ConnectionManager *owner;
  };

  ConnectionManager();
  void begin(ConfigManager *config, RtcDriver *rtc);
  void loop();
  void processPendingAction();
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
  bool isSystemPortalStarting() const;
  bool isConfigPortalActive() const;
  bool isConfigPortalStarting() const;

private:
  enum NetworkAction : uint8_t {
    NETWORK_ACTION_NONE,
    NETWORK_ACTION_ENABLE,
    NETWORK_ACTION_DISABLE,
    NETWORK_ACTION_CONFIG_PORTAL,
    NETWORK_ACTION_SYSTEM_PORTAL
  };

  void activateConfigPortal();
  void activateSystemPortal();
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
  bool currentSessionSynced = false;
  bool systemPortalActive = false;
  NetworkAction pendingAction = NETWORK_ACTION_NONE;
  mutable SemaphoreHandle_t networkMutex = nullptr;
  DateTime ntpTime;
  void lockNetwork() const;
  void unlockNetwork() const;
};
