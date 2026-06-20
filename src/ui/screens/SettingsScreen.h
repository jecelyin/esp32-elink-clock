#pragma once

#include "../../drivers/BatteryDriver.h"
#include "../../managers/ConfigManager.h"
#include "../../utils/HardwareCheck.h"
#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"

class SettingsScreen : public Screen {
public:
  SettingsScreen(ConfigManager *config, StatusBar *statusBar,
                 BatteryDriver *battery)
      : config(config), statusBar(statusBar), battery(battery) {}

  void draw(DisplayDriver *display) override {
    BatteryInfo info = battery->readInfo();
    display->display.setFullWindow();
    display->display.firstPage();
    do {
      drawPage(display, info);
    } while (display->display.nextPage());
  }

  bool handleInput(UIKey key) override {
    if (key == UI_KEY_ENTER) {
      runManualHardwareCheck();
    } else if (key == UI_KEY_LEFT) { // Back
      uiManager->switchScreen(SCREEN_MENU);
    }
    return false;
  }

  bool onLongPress() override {
    // 关键逻辑：UIManager 会把 ENTER 长按单独分发到 onLongPress()，
    // 如果设置页不显式处理，这个手势不会复用 handleInput() 里的返回菜单逻辑。
    if (uiManager)
      uiManager->switchScreen(SCREEN_MENU);
    return false;
  }

private:
  void drawPage(DisplayDriver *displayDrv, const BatteryInfo &info) {
    displayDrv->display.fillScreen(GxEPD_WHITE);
    statusBar->draw(displayDrv, true);
    drawHeader(displayDrv);
    drawBasicSettings(displayDrv);
    drawBatteryInfo(displayDrv, info);
    drawHardwareHint(displayDrv);
  }

  void drawHeader(DisplayDriver *displayDrv) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
    u8g2.setFont(u8g2_font_helvB18_tf);
    u8g2.setCursor(120, 45);
    u8g2.print("SETTINGS");
  }

  void drawBasicSettings(DisplayDriver *displayDrv) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    u8g2.setFont(u8g2_font_helvB10_tf);
    u8g2.setCursor(24, 72);
    u8g2.print("WiFi: ");
    printLimited(u8g2, config->config.wifi_ssid.c_str(), 26);
    u8g2.setCursor(24, 92);
    u8g2.print("Volume: ");
    u8g2.print(config->config.volume);
    u8g2.setCursor(150, 92);
    u8g2.print("Format: ");
    u8g2.print(config->config.time_format_24 ? "24H" : "12H");
  }

  void drawBatteryInfo(DisplayDriver *displayDrv, const BatteryInfo &info) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    u8g2.setFont(u8g2_font_helvB10_tf);
    printBatterySummary(u8g2, info, 116);
    printBatteryMeasurements(u8g2, info, 136);
    printBatteryRuntime(u8g2, info, 156);
    printBatteryConfig(u8g2, info, 176);
    printBatteryRegisters(u8g2, info, 196);
    printBatteryState(u8g2, info, 216);
  }

  void drawHardwareHint(DisplayDriver *displayDrv) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    u8g2.setFont(u8g2_font_helvB12_tf);
    u8g2.setCursor(24, 254);
    u8g2.print("> Hardware Check");
  }

  void printBatterySummary(U8G2_FOR_ADAFRUIT_GFX &u8g2,
                           const BatteryInfo &info, int y) {
    u8g2.setCursor(24, y);
    u8g2.print("Battery: ");
    u8g2.print(getSourceName(info));
    u8g2.print("  Gauge: ");
    u8g2.print(info.gaugeOnline ? "OK" : "FAIL");
  }

  void printBatteryMeasurements(U8G2_FOR_ADAFRUIT_GFX &u8g2,
                                const BatteryInfo &info, int y) {
    u8g2.setCursor(24, y);
    u8g2.print("SOC: ");
    printSoc(u8g2, info);
    u8g2.print("  Voltage: ");
    u8g2.print(info.voltage, 3);
    u8g2.print("V");
  }

  void printBatteryRuntime(U8G2_FOR_ADAFRUIT_GFX &u8g2,
                           const BatteryInfo &info, int y) {
    u8g2.setCursor(24, y);
    u8g2.print("RRT: ");
    printGaugeValue(u8g2, info, info.remainingMinutes, "min");
    u8g2.print("  Alert: ");
    printGaugeBool(u8g2, info, info.alert);
  }

  void printBatteryConfig(U8G2_FOR_ADAFRUIT_GFX &u8g2,
                          const BatteryInfo &info, int y) {
    u8g2.setCursor(24, y);
    u8g2.print("ATHD: ");
    printGaugeValue(u8g2, info, info.alertThreshold, "%");
    u8g2.print("  UFG: ");
    printGaugeBool(u8g2, info, info.profileUpdated);
  }

  void printBatteryRegisters(U8G2_FOR_ADAFRUIT_GFX &u8g2,
                             const BatteryInfo &info, int y) {
    u8g2.setCursor(24, y);
    u8g2.print("Version: ");
    printGaugeHex(u8g2, info, info.version);
    u8g2.print("  Config: ");
    printGaugeHex(u8g2, info, info.config);
    u8g2.print("  Mode: ");
    printGaugeHex(u8g2, info, info.mode);
  }

  void printBatteryState(U8G2_FOR_ADAFRUIT_GFX &u8g2,
                         const BatteryInfo &info, int y) {
    u8g2.setCursor(24, y);
    u8g2.print("State: ");
    printGaugeState(u8g2, info);
    u8g2.print("  Level: ");
    printLevel(u8g2, info.levelPercent);
  }

  void printLimited(U8G2_FOR_ADAFRUIT_GFX &u8g2, const char *text,
                    uint8_t limit) {
    if (text == nullptr) {
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

  void printLevel(U8G2_FOR_ADAFRUIT_GFX &u8g2, int level) {
    if (level < 0) {
      u8g2.print("--%");
      return;
    }

    u8g2.print(level);
    u8g2.print("%");
  }

  void printGaugeValue(U8G2_FOR_ADAFRUIT_GFX &u8g2, const BatteryInfo &info,
                       uint16_t value, const char *unit) {
    if (info.source != BATTERY_SOURCE_CW2015) {
      u8g2.print("--");
      return;
    }

    u8g2.print(value);
    u8g2.print(unit);
  }

  void printGaugeBool(U8G2_FOR_ADAFRUIT_GFX &u8g2, const BatteryInfo &info,
                      bool value) {
    if (info.source != BATTERY_SOURCE_CW2015) {
      u8g2.print("--");
      return;
    }

    u8g2.print(value ? "YES" : "NO");
  }

  void printGaugeState(U8G2_FOR_ADAFRUIT_GFX &u8g2,
                       const BatteryInfo &info) {
    if (info.source != BATTERY_SOURCE_CW2015) {
      u8g2.print("--");
      return;
    }

    u8g2.print(info.sleeping ? "Sleep" : "Awake");
  }

  void printGaugeHex(U8G2_FOR_ADAFRUIT_GFX &u8g2, const BatteryInfo &info,
                     uint8_t value) {
    if (info.source != BATTERY_SOURCE_CW2015) {
      u8g2.print("--");
      return;
    }

    char buffer[5];
    snprintf(buffer, sizeof(buffer), "%02X", value);
    u8g2.print("0x");
    u8g2.print(buffer);
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
};
