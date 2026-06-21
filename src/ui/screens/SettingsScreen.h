#pragma once

#include "../../drivers/BatteryDriver.h"
#include "../../managers/ConfigManager.h"
#include "../../managers/ConfigPortal.h"
#include "../../managers/ConnectionManager.h"
#include "../../utils/HardwareCheck.h"
#include "../../utils/SimpleQRCode.h"
#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"
#include <WiFi.h>

class SettingsScreen : public Screen {
public:
  SettingsScreen(ConfigManager *config, StatusBar *statusBar,
                 BatteryDriver *battery, ConnectionManager *conn)
      : config(config), statusBar(statusBar), battery(battery), conn(conn) {}

  void enter() override {
    // 关键逻辑：设置页已经承接原 Config 页职责，进入页面时必须主动打开
    // WiFiManager 门户，否则屏幕上的二维码和网关地址可能对应不上真实 AP。
    if (conn)
      conn->startAP();
  }

  void draw(DisplayDriver *display) override {
    BatteryInfo info = battery->readInfo();
    display->display.setFullWindow();
    display->display.firstPage();
    do {
      drawPage(display, info);
    } while (display->display.nextPage());
    display->powerOff();
  }

  bool handleInput(UIKey key) override {
    if (key == UI_KEY_ENTER) {
      runManualHardwareCheck();
    }
    return false;
  }

private:
  void drawPage(DisplayDriver *displayDrv, const BatteryInfo &info) {
    displayDrv->display.fillScreen(GxEPD_WHITE);
    statusBar->draw(displayDrv, true);
    drawHeader(displayDrv);
    drawPortalPanel(displayDrv);
    drawSettingsPanel(displayDrv, info);
    drawHardwareHint(displayDrv);
  }

  void drawHeader(DisplayDriver *displayDrv) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
    u8g2.setFont(u8g2_font_helvB18_tf);
    u8g2.setCursor(118, 45);
    u8g2.print("SETTINGS");
  }

  void drawPortalPanel(DisplayDriver *displayDrv) {
    auto &display = displayDrv->display;
    int panelX = 10;
    int panelY = 52;
    int panelW = 380;
    int panelH = 146;
    display.drawRect(panelX, panelY, panelW, panelH, GxEPD_BLACK);
    drawQrCode(displayDrv, panelX + 10, panelY + 10, 118);
    drawQrCaption(displayDrv, panelX + 24, panelY + 137);
    drawPortalDetails(displayDrv, panelX + 146, panelY + 24);
  }

  void drawQrCode(DisplayDriver *displayDrv, int x, int y, int boxSize) {
    SimpleQRCode qr;
    String payload = ConfigPortal::buildWiFiQrPayload();
    if (!qr.encodeText(payload.c_str())) {
      drawQrFallback(displayDrv, x, y, boxSize);
      return;
    }

    int quietModules = 4;
    int moduleSize = boxSize / (qr.size() + quietModules * 2);
    if (moduleSize <= 0) {
      drawQrFallback(displayDrv, x, y, boxSize);
      return;
    }
    drawQrModules(displayDrv, qr, x, y, boxSize, moduleSize);
  }

  void drawQrModules(DisplayDriver *displayDrv, const SimpleQRCode &qr, int x,
                     int y, int boxSize, int moduleSize) {
    auto &display = displayDrv->display;
    int quietModules = 4;
    int qrPixels = (qr.size() + quietModules * 2) * moduleSize;
    int originX = x + (boxSize - qrPixels) / 2 + quietModules * moduleSize;
    int originY = y + (boxSize - qrPixels) / 2 + quietModules * moduleSize;
    display.fillRect(x, y, boxSize, boxSize, GxEPD_WHITE);

    for (uint8_t row = 0; row < qr.size(); ++row) {
      for (uint8_t col = 0; col < qr.size(); ++col) {
        if (qr.isDark(col, row))
          fillQrModule(displayDrv, originX, originY, col, row, moduleSize);
      }
    }
  }

  void fillQrModule(DisplayDriver *displayDrv, int originX, int originY,
                    uint8_t col, uint8_t row, int moduleSize) {
    displayDrv->display.fillRect(originX + col * moduleSize,
                                 originY + row * moduleSize, moduleSize,
                                 moduleSize, GxEPD_BLACK);
  }

  void drawQrFallback(DisplayDriver *displayDrv, int x, int y, int boxSize) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    displayDrv->display.drawRect(x, y, boxSize, boxSize, GxEPD_BLACK);
    u8g2.setFont(u8g2_font_helvB10_tf);
    u8g2.setCursor(x + 36, y + boxSize / 2);
    u8g2.print("QR ERR");
  }

  void drawQrCaption(DisplayDriver *displayDrv, int x, int y) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setCursor(x, y);
    u8g2.print("SCAN WIFI QR");
  }

  void drawPortalDetails(DisplayDriver *displayDrv, int x, int y) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
    printLabel(u8g2, x, y, "AP SSID");
    printValue(u8g2, x, y + 24, ConfigPortal::AP_SSID);
    printLabel(u8g2, x, y + 54, "PASSWORD");
    drawPasswordBadge(displayDrv, x, y + 64);
    printLabel(u8g2, x, y + 102, "PORTAL IP");
    printGatewayValue(u8g2, x, y + 122);
  }

  void drawPasswordBadge(DisplayDriver *displayDrv, int x, int y) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    displayDrv->display.fillRect(x, y, 98, 22, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);
    u8g2.setFont(u8g2_font_helvB12_tf);
    u8g2.setCursor(x + 8, y + 16);
    u8g2.print(ConfigPortal::AP_PASSWORD);
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
  }

  void drawSettingsPanel(DisplayDriver *displayDrv, const BatteryInfo &info) {
    auto &display = displayDrv->display;
    auto &u8g2 = displayDrv->u8g2Fonts;
    display.drawLine(10, 210, 390, 210, GxEPD_BLACK);
    u8g2.setFont(u8g2_font_helvB10_tf);
    printSavedWifi(u8g2, 16, 230);
    printVolumeAndFormat(u8g2, 16, 250);
    printBatteryLine(u8g2, info, 16, 270);
  }

  void printSavedWifi(U8G2_FOR_ADAFRUIT_GFX &u8g2, int x, int y) {
    u8g2.setCursor(x, y);
    u8g2.print("Saved WiFi: ");
    printLimited(u8g2, config->config.wifi_ssid.c_str(), 24);
  }

  void printVolumeAndFormat(U8G2_FOR_ADAFRUIT_GFX &u8g2, int x, int y) {
    u8g2.setCursor(x, y);
    u8g2.print("Volume: ");
    u8g2.print(config->config.volume);
    u8g2.print("  Format: ");
    u8g2.print(config->config.time_format_24 ? "24H" : "12H");
  }

  void printBatteryLine(U8G2_FOR_ADAFRUIT_GFX &u8g2, const BatteryInfo &info,
                        int x, int y) {
    u8g2.setCursor(x, y);
    u8g2.print("Battery: ");
    u8g2.print(getSourceName(info));
    u8g2.print("  SOC: ");
    printSoc(u8g2, info);
    u8g2.print("  V: ");
    printVoltage(u8g2, info);
  }

  void drawHardwareHint(DisplayDriver *displayDrv) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    u8g2.setFont(u8g2_font_helvB12_tf);
    u8g2.setCursor(24, 294);
    u8g2.print("> Hardware Check");
  }

  void printLabel(U8G2_FOR_ADAFRUIT_GFX &u8g2, int x, int y,
                  const char *label) {
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setCursor(x, y);
    u8g2.print(label);
  }

  void printValue(U8G2_FOR_ADAFRUIT_GFX &u8g2, int x, int y,
                  const char *value) {
    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.setCursor(x, y);
    u8g2.print(value);
  }

  void printGatewayValue(U8G2_FOR_ADAFRUIT_GFX &u8g2, int x, int y) {
    String gateway = getGatewayIp();
    u8g2.setFont(u8g2_font_helvB12_tr);
    u8g2.setCursor(x, y);
    u8g2.print(gateway);
  }

  String getGatewayIp() {
    IPAddress ip = WiFi.softAPIP();
    if (ip == IPAddress(0, 0, 0, 0))
      return String(ConfigPortal::DEFAULT_GATEWAY);
    return ip.toString();
  }

  void printLimited(U8G2_FOR_ADAFRUIT_GFX &u8g2, const char *text,
                    uint8_t limit) {
    if (text == nullptr || text[0] == '\0') {
      u8g2.print("-");
      return;
    }

    for (uint8_t i = 0; text[i] != '\0' && i < limit; ++i) {
      u8g2.print(text[i]);
    }
  }

  void printSoc(U8G2_FOR_ADAFRUIT_GFX &u8g2, const BatteryInfo &info) {
    if (!info.dataValid) {
      u8g2.print("--");
      return;
    }

    u8g2.print(info.socPercent, info.source == BATTERY_SOURCE_CW2015 ? 2 : 0);
    u8g2.print("%");
  }

  void printVoltage(U8G2_FOR_ADAFRUIT_GFX &u8g2, const BatteryInfo &info) {
    if (!info.dataValid) {
      u8g2.print("--");
      return;
    }

    u8g2.print(info.voltage, 2);
  }

  const char *getSourceName(const BatteryInfo &info) const {
    if (info.source == BATTERY_SOURCE_CW2015)
      return "CW2015";
    if (info.source == BATTERY_SOURCE_ADC)
      return "ADC";
    return "NONE";
  }

  void runManualHardwareCheck() {
    DisplayDriver *displayDrv = uiManager->getDisplayDriver();
    displayDrv->showStatus("Manual Check...", 0);
    HardwareCheck::setPowerEnabled(true);
    bool allOk = runDeviceChecks(displayDrv);
    HardwareCheck::setPowerEnabled(false);
    finishManualHardwareCheck(displayDrv, allOk);
  }

  bool runDeviceChecks(DisplayDriver *displayDrv) {
    bool allOk = true;
    for (uint8_t i = 0; i < HardwareCheck::DEVICE_COUNT; i++) {
      const HardwareCheck::Device &device = HardwareCheck::DEVICES[i];
      bool ok = HardwareCheck::checkDevice(device, battery);
      allOk = ok && allOk;
      showDeviceCheckResult(displayDrv, device.shortName, ok, i + 1);
      delay(200);
    }
    return allOk;
  }

  void showDeviceCheckResult(DisplayDriver *displayDrv, const char *name,
                             bool ok, uint8_t line) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%s: %s", name, ok ? "OK" : "FAIL");
    displayDrv->showStatus(buffer, line);
  }

  void finishManualHardwareCheck(DisplayDriver *displayDrv, bool allOk) {
    if (allOk) {
      config->config.hw_checked = true;
      config->config.hw_check_version = HardwareCheck::REQUIRED_VERSION;
      config->save();
      displayDrv->showStatus("All Devices OK", 0);
    } else {
      displayDrv->showStatus("Check Failed!", 0);
    }
    delay(2000);
    draw(displayDrv);
  }

  ConfigManager *config;
  StatusBar *statusBar;
  BatteryDriver *battery;
  ConnectionManager *conn;
};
