#pragma once

#include <Arduino.h>

namespace ConfigPortal {
// 关键逻辑：AP 名称、密码、二维码内容必须从同一处派生，避免屏幕显示
// 与 WiFiManager 实际开启的热点凭据再次失配。
constexpr const char *AP_SSID = "ESP32-Clock";
constexpr const char *DEFAULT_GATEWAY = "192.168.4.1";
constexpr uint16_t SYSTEM_WEB_PORT = 8080;

inline String getAPPassword() {
  char password[13];
  uint32_t chipId = static_cast<uint32_t>(ESP.getEfuseMac());
  snprintf(password, sizeof(password), "CLK%08X", chipId);
  return String(password);
}

inline String buildWiFiQrPayload() {
  String payload = "WIFI:T:WPA;S:";
  payload += AP_SSID;
  payload += ";P:";
  payload += getAPPassword();
  payload += ";;";
  return payload;
}

inline String buildSystemUrl(const String &ip) {
  return "http://" + ip + ":" + String(SYSTEM_WEB_PORT);
}
} // namespace ConfigPortal
