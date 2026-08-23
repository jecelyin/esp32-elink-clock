#pragma once

#include "../../drivers/BatteryDriver.h"
#include "../../managers/ConfigManager.h"
#include "../../managers/ConnectionManager.h"
#include "../../utils/SimpleQRCode.h"
#include "../Screen.h"
#include "../components/StatusBar.h"

class SettingsScreen : public Screen {
public:
  SettingsScreen(ConfigManager *config, StatusBar *statusBar,
                 BatteryDriver *battery, ConnectionManager *conn);

  void enter() override;
  void exit() override;
  void update() override;
  void draw(DisplayDriver *display) override;
  bool onInput(UIKey key) override;
  bool shouldDrawAfterInput() const override;

private:
  enum RefreshMode : uint8_t {
    REFRESH_FULL,
    REFRESH_NAVIGATION,
    REFRESH_CONTENT
  };

  enum MenuItem : uint8_t {
    MENU_HARDWARE = 0,
    MENU_NETWORK = 1,
    MENU_SYSTEM = 2,
    MENU_RESTART = 3,
    MENU_COUNT = 4
  };

  ConfigManager *config;
  StatusBar *statusBar;
  BatteryDriver *battery;
  ConnectionManager *conn;
  uint8_t selectedItem = MENU_HARDWARE;
  uint8_t previousSelectedItem = MENU_HARDWARE;
  bool redrawAfterInput = true;
  int8_t renderedPortalState = -1;
  int8_t portalStateForDraw = -1;
  RefreshMode refreshMode = REFRESH_FULL;

  void drawFull(DisplayDriver *display, const BatteryInfo &info);
  void drawNavigationPartial(DisplayDriver *display);
  void drawMenuItemPartial(DisplayDriver *display, uint8_t index);
  void drawContentPartial(DisplayDriver *display, const BatteryInfo &info);
  void drawPage(DisplayDriver *display, const BatteryInfo &info);
  void drawSidebar(DisplayDriver *display);
  void drawMenuItem(DisplayDriver *display, uint8_t index, int y);
  void drawContent(DisplayDriver *display, const BatteryInfo &info);
  void drawHardwareContent(DisplayDriver *display, const BatteryInfo &info);
  void drawNetworkContent(DisplayDriver *display);
  void drawSystemContent(DisplayDriver *display);
  void drawRestartContent(DisplayDriver *display);
  void drawPortalContent(DisplayDriver *display, const String &payload,
                         const char *title, const String &address,
                         const char *statusText);
  void drawQrCode(DisplayDriver *display, const String &payload, int x, int y,
                  int boxSize);
  void drawQrModules(DisplayDriver *display, const SimpleQRCode &qr, int x,
                     int y, int boxSize, int moduleSize);
  void drawQrFallback(DisplayDriver *display, int x, int y, int boxSize);
  void drawText(DisplayDriver *display, int x, int y, const char *text,
                const uint8_t *font);
  String getGatewayIp() const;
  int8_t getSelectedPortalState() const;
  void runManualHardwareCheck();
  bool runDeviceChecks(DisplayDriver *display);
  void showDeviceCheckResult(DisplayDriver *display, const char *name, bool ok,
                             uint8_t line);
  void finishManualHardwareCheck(DisplayDriver *display, bool allOk);
  void restartDevice();
};
