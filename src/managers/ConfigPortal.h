#pragma once

#include <Arduino.h>

namespace ConfigPortal {
// 关键逻辑：AP 名称、密码、二维码内容必须从同一处派生，避免屏幕显示
// 与 WiFiManager 实际开启的热点凭据再次失配。
constexpr const char *AP_SSID = "ESP32-Clock";
constexpr const char *AP_PASSWORD = "12345678";
constexpr const char *DEFAULT_GATEWAY = "192.168.4.1";

inline String buildWiFiQrPayload() {
  String payload = "WIFI:T:WPA;S:";
  payload += AP_SSID;
  payload += ";P:";
  payload += AP_PASSWORD;
  payload += ";;";
  return payload;
}
} // namespace ConfigPortal
