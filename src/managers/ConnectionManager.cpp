#include "ConnectionManager.h"
#include "ConfigPortal.h"
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
  if (networkMutex == nullptr) {
    networkMutex = xSemaphoreCreateRecursiveMutex();
  }
}

void ConnectionManager::enableNetwork(bool enable) {
  lockNetwork();
  if (networkEnabled == enable) {
    unlockNetwork();
    return;
  }

  if (enable) {
    powerOnNetwork();
    unlockNetwork();
    return;
  }

  powerOffNetwork();
  unlockNetwork();
}

void ConnectionManager::loop() {
  lockNetwork();
  if (!networkEnabled) {
    unlockNetwork();
    return;
  }
  bool portalActive = systemPortalActive;
  uint32_t lastSync = lastSyncTime;
  unlockNetwork();

  if (!portalActive) {
    beginAutoConnect();
    wifiManager.process();
  }

  if (WiFi.status() != WL_CONNECTED) {
    retryWiFiConnection();
    return;
  }

  bool shouldSync =
      millis() - lastSync > NTP_SYNC_INTERVAL_MS || lastSync == 0;
  if (shouldSync) {
    syncTime();
  }
}

bool ConnectionManager::isConnected() {
  lockNetwork();
  bool connected = WiFi.status() == WL_CONNECTED;
  unlockNetwork();
  return connected;
}

bool ConnectionManager::isSyncComplete() {
  lockNetwork();
  bool complete =
      (lastSyncTime > 0) && (WiFi.status() == WL_CONNECTED);
  unlockNetwork();
  return complete;
}

bool ConnectionManager::isConfigPortalActive() const {
  lockNetwork();
  bool active = wifiManager.getConfigPortalActive();
  unlockNetwork();
  return active;
}

bool ConnectionManager::isNetworkEnabled() const {
  lockNetwork();
  bool enabled = networkEnabled;
  unlockNetwork();
  return enabled;
}

bool ConnectionManager::isSystemPortalActive() const {
  lockNetwork();
  bool active = systemPortalActive;
  unlockNetwork();
  return active;
}

void ConnectionManager::lockNetwork() const {
  if (networkMutex != nullptr) {
    xSemaphoreTakeRecursive(networkMutex, portMAX_DELAY);
  }
}

void ConnectionManager::unlockNetwork() const {
  if (networkMutex != nullptr) {
    xSemaphoreGiveRecursive(networkMutex);
  }
}

void ConnectionManager::startAP() {
  lockNetwork();
  if (!networkEnabled)
    powerOnNetwork();

  systemPortalActive = false;
  WiFi.softAPdisconnect(true);
  configurePortal(true);
  if (wifiManager.getConfigPortalActive()) {
    unlockNetwork();
    return;
  }

  // 关键逻辑：设置页需要立即出现 ESP32-Clock 热点，不能走
  // autoConnect() 的“先连已保存 WiFi，失败后再开 AP”路径。
  firstConnectAttempted = true;
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  String password = ConfigPortal::getAPPassword();
  wifiManager.startConfigPortal(ConfigPortal::AP_SSID, password.c_str());
  Serial.println("WiFi Config Portal forced from Settings");
  unlockNetwork();
}

void ConnectionManager::startSystemAP() {
  lockNetwork();
  if (!networkEnabled)
    powerOnNetwork();

  stopPortalIfActive();
  firstConnectAttempted = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  String password = ConfigPortal::getAPPassword();
  systemPortalActive =
      WiFi.softAP(ConfigPortal::AP_SSID, password.c_str());
  if (!systemPortalActive) {
    Serial.println("System settings access point failed");
    powerOffNetwork();
    unlockNetwork();
    return;
  }
  Serial.println("System settings access point started");
  unlockNetwork();
}

void ConnectionManager::flushPendingRtcSync() {
  lockNetwork();
  if (!pendingSync || rtcDriver == nullptr) {
    unlockNetwork();
    return;
  }

  uint32_t now = millis();
  uint32_t retryInterval = getRtcSyncRetryInterval();
  if (lastRtcSyncAttempt != 0 &&
      now - lastRtcSyncAttempt < retryInterval) {
    unlockNetwork();
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
  unlockNetwork();
}

void ConnectionManager::startScheduledSyncIfDue(uint32_t now) {
  lockNetwork();
  bool enabled = networkEnabled;
  bool neverStarted = lastNetworkPowerOnTime == 0;
  bool intervalReached =
      !neverStarted &&
      now - lastNetworkPowerOnTime >= AUTO_NETWORK_SYNC_INTERVAL_MS;
  unlockNetwork();
  if (enabled) {
    return;
  }

  if (neverStarted || intervalReached) {
    enableNetwork(true);
  }
}

void ConnectionManager::beginAutoConnect() {
  if (firstConnectAttempted)
    return;

  firstConnectAttempted = true;
  configurePortal(false);

  String password = ConfigPortal::getAPPassword();
  if (wifiManager.autoConnect(ConfigPortal::AP_SSID, password.c_str())) {
    Serial.println("WiFi connected");
    return;
  }

  Serial.println("WiFi Config Portal started");
}

void ConnectionManager::configurePortal(bool manual) {
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(manual ? 0 : CONFIG_PORTAL_TIMEOUT_SEC);
  wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT_SEC);
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
  lockNetwork();
  if (lastNetworkPowerOnTime == 0 ||
      now - lastNetworkPowerOnTime >= AUTO_NETWORK_SYNC_INTERVAL_MS) {
    unlockNetwork();
    return 0;
  }

  uint32_t syncDelayMs =
      AUTO_NETWORK_SYNC_INTERVAL_MS - (now - lastNetworkPowerOnTime);
  if (!pendingSync) {
    unlockNetwork();
    return syncDelayMs;
  }

  uint32_t retryIntervalMs = getRtcSyncRetryInterval();
  if (lastRtcSyncAttempt == 0 || now - lastRtcSyncAttempt >= retryIntervalMs) {
    unlockNetwork();
    return 0;
  }

  uint32_t retryDelayMs = retryIntervalMs - (now - lastRtcSyncAttempt);
  uint32_t result =
      retryDelayMs < syncDelayMs ? retryDelayMs : syncDelayMs;
  unlockNetwork();
  return result;
}

void ConnectionManager::powerOffNetwork() {
  // 先确认门户真实处于激活状态，再调用关闭接口，避免 WiFiManager
  // 内部空指针解引用。
  stopPortalIfActive();
  WiFi.disconnect(true, false);
  WiFi.softAPdisconnect(true);
  esp_wifi_stop();
  WiFi.mode(WIFI_OFF);
  firstConnectAttempted = false;
  lastReconnectAttempt = 0;
  networkEnabled = false;
  systemPortalActive = false;
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

  uint32_t now = millis();
  // 关键逻辑：系统设置热点刚开启时也要立刻尝试 STA 回连，
  // 不能强制等首个 30 秒窗口，否则浏览器刚保存的新 API Key 无法及时生效。
  if (lastReconnectAttempt != 0 &&
      now - lastReconnectAttempt <= WIFI_RECONNECT_INTERVAL_MS)
    return;

  lastReconnectAttempt = now;
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
    lockNetwork();
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
    unlockNetwork();
    Serial.println("Time fetched from NTP, pending RTC update");
  }
}
