#include "ConnectionManager.h"
#include <esp_wifi.h>
#include <WiFiManager.h>

namespace {
constexpr uint32_t CONFIG_PORTAL_TIMEOUT_SEC = 120UL;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_SEC = 20UL;
constexpr uint32_t AUTO_NETWORK_SYNC_INTERVAL_MS = 3600000UL;
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 30000UL;
constexpr uint32_t NTP_SYNC_INTERVAL_MS = 3600000UL;
constexpr uint32_t RTC_SYNC_RETRY_INTERVAL_MS = 1000UL;
constexpr uint32_t RTC_SYNC_RETRY_MAX_INTERVAL_MS = 60000UL;
constexpr uint8_t RTC_SYNC_RETRY_MAX_SHIFT = 6;
} // namespace

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600; // UTC+8 for China
const int daylightOffset_sec = 0;
WiFiManager wifiManager;

ConnectionManager::ConnectionManager() {}

void ConnectionManager::begin(ConfigManager *config, RtcDriver *rtc) {
  configMgr = config;
  rtcDriver = rtc;
}

void ConnectionManager::enableNetwork(bool enable) {
  if (networkEnabled == enable)
    return;

  if (enable) {
    powerOnNetwork();
    return;
  }

  powerOffNetwork();
}

void ConnectionManager::loop() {
  if (!networkEnabled)
    return;

  beginAutoConnect();
  wifiManager.process();

  if (WiFi.status() != WL_CONNECTED) {
    retryWiFiConnection();
    return;
  }

  if (millis() - lastSyncTime > NTP_SYNC_INTERVAL_MS || lastSyncTime == 0) {
    syncTime();
  }
}

bool ConnectionManager::isConnected() { return WiFi.status() == WL_CONNECTED; }

bool ConnectionManager::isSyncComplete() {
  return (lastSyncTime > 0) && (WiFi.status() == WL_CONNECTED);
}

void ConnectionManager::startAP() {
  // WiFi.softAP("ESP32-Clock", "12345678");
}

void ConnectionManager::flushPendingRtcSync() {
  if (!pendingSync || rtcDriver == nullptr)
    return;

  uint32_t now = millis();
  uint32_t retryInterval = getRtcSyncRetryInterval();
  if (lastRtcSyncAttempt != 0 &&
      now - lastRtcSyncAttempt < retryInterval) {
    return;
  }

  lastRtcSyncAttempt = now;
  // 关键逻辑：只有 RTC 真正写成功后才清理 pending，避免一次 I2C 抖动
  // 把本轮 NTP 对时机会直接丢掉。
  DateTime rtcTime = rtcDriver->getSoftwareTime();
  if (rtcDriver->setTime(rtcTime)) {
    pendingSync = false;
    lastRtcSyncAttempt = 0;
    rtcSyncFailCount = 0;
    Serial.println("RTC updated from NTP");
  } else if (rtcSyncFailCount < RTC_SYNC_RETRY_MAX_SHIFT) {
    rtcSyncFailCount++;
  }
}

void ConnectionManager::startScheduledSyncIfDue(uint32_t now) {
  if (networkEnabled) {
    return;
  }

  bool neverStarted = lastNetworkPowerOnTime == 0;
  bool intervalReached =
      !neverStarted &&
      now - lastNetworkPowerOnTime >= AUTO_NETWORK_SYNC_INTERVAL_MS;
  if (neverStarted || intervalReached) {
    enableNetwork(true);
  }
}

void ConnectionManager::beginAutoConnect() {
  if (firstConnectAttempted)
    return;

  firstConnectAttempted = true;
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT_SEC);
  wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT_SEC);

  if (wifiManager.autoConnect("ESP32-Clock")) {
    Serial.println("WiFi connected");
    return;
  }

  Serial.println("WiFi Config Portal started");
}

uint32_t ConnectionManager::getRtcSyncRetryInterval() const {
  uint8_t shift = rtcSyncFailCount > RTC_SYNC_RETRY_MAX_SHIFT
                      ? RTC_SYNC_RETRY_MAX_SHIFT
                      : rtcSyncFailCount;
  uint32_t intervalMs = RTC_SYNC_RETRY_INTERVAL_MS << shift;
  return intervalMs > RTC_SYNC_RETRY_MAX_INTERVAL_MS
             ? RTC_SYNC_RETRY_MAX_INTERVAL_MS
             : intervalMs;
}

uint32_t ConnectionManager::getNextScheduledWorkDelayMs(uint32_t now) const {
  if (lastNetworkPowerOnTime == 0 ||
      now - lastNetworkPowerOnTime >= AUTO_NETWORK_SYNC_INTERVAL_MS) {
    return 0;
  }

  uint32_t syncDelayMs =
      AUTO_NETWORK_SYNC_INTERVAL_MS - (now - lastNetworkPowerOnTime);
  if (!pendingSync) {
    return syncDelayMs;
  }

  uint32_t retryIntervalMs = getRtcSyncRetryInterval();
  if (lastRtcSyncAttempt == 0 || now - lastRtcSyncAttempt >= retryIntervalMs) {
    return 0;
  }

  uint32_t retryDelayMs = retryIntervalMs - (now - lastRtcSyncAttempt);
  return retryDelayMs < syncDelayMs ? retryDelayMs : syncDelayMs;
}

void ConnectionManager::powerOffNetwork() {
  // 先确认门户真实处于激活状态，再调用关闭接口，避免 WiFiManager
  // 内部空指针解引用。
  stopPortalIfActive();
  WiFi.disconnect(true, false);
  esp_wifi_stop();
  WiFi.mode(WIFI_OFF);
  firstConnectAttempted = false;
  lastReconnectAttempt = 0;
  networkEnabled = false;
  Serial.println("WiFi Power Off to save energy");
}

void ConnectionManager::powerOnNetwork() {
  networkEnabled = true;
  firstConnectAttempted = false;
  lastReconnectAttempt = 0;
  lastNetworkPowerOnTime = millis();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  Serial.println("WiFi Power On for sync");
}

void ConnectionManager::retryWiFiConnection() {
  if (wifiManager.getConfigPortalActive() || wifiManager.getWebPortalActive())
    return;

  if (millis() - lastReconnectAttempt <= WIFI_RECONNECT_INTERVAL_MS)
    return;

  lastReconnectAttempt = millis();
  Serial.println("WiFi disconnected, attempting to reconnect...");
  WiFi.begin();
}

void ConnectionManager::stopPortalIfActive() {
  if (wifiManager.getConfigPortalActive()) {
    wifiManager.stopConfigPortal();
  }

  if (wifiManager.getWebPortalActive()) {
    wifiManager.stopWebPortal();
  }
}

void ConnectionManager::syncTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    ntpTime.second = timeinfo.tm_sec;
    ntpTime.minute = timeinfo.tm_min;
    ntpTime.hour = timeinfo.tm_hour;
    ntpTime.day = timeinfo.tm_mday;
    ntpTime.month = timeinfo.tm_mon + 1;
    ntpTime.year = timeinfo.tm_year % 100;
    ntpTime.week = timeinfo.tm_wday;

    // 关键逻辑：NTP 已经给出了可信时间，先刷新软件时钟供 UI 使用；
    // RX8010 如果因 I2C 超时暂时写失败，后台 pending 仍会继续补写硬件 RTC。
    if (rtcDriver != nullptr) {
      rtcDriver->setSoftwareTime(ntpTime);
    }
    pendingSync = true;
    lastRtcSyncAttempt = 0;
    lastSyncTime = millis();
    Serial.println("Time fetched from NTP, pending RTC update");
  }
}
