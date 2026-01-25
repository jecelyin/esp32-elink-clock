#pragma once

#include "../../managers/BusManager.h"
#include "../../managers/ConfigManager.h"
#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"

class SettingsScreen : public Screen {
public:
  SettingsScreen(ConfigManager *config, StatusBar *statusBar,
                 SensorDriver *sensor)
      : config(config), statusBar(statusBar), sensor(sensor) {}

  void draw(DisplayDriver *display) override {
    display->display.setFullWindow();
    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);
      statusBar->draw(display, true);

      // Header
      display->u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      display->u8g2Fonts.setCursor(120, 40);
      display->u8g2Fonts.print("SETTINGS");

      // Icon
      display->u8g2Fonts.setFont(u8g2_font_open_iconic_embedded_4x_t);
      display->u8g2Fonts.setCursor(20, 60);
      display->u8g2Fonts.print("G");

      // Settings List
      display->u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      display->u8g2Fonts.setCursor(50, 120);
      display->u8g2Fonts.print("WiFi: ");
      display->u8g2Fonts.print(config->config.wifi_ssid);

      display->u8g2Fonts.setCursor(50, 150);
      display->u8g2Fonts.print("Volume: ");
      display->u8g2Fonts.print(config->config.volume);

      display->u8g2Fonts.setCursor(50, 180);
      display->u8g2Fonts.print("Format: ");
      display->u8g2Fonts.print(config->config.time_format_24 ? "24H" : "12H");

      display->u8g2Fonts.setCursor(50, 210);
      display->u8g2Fonts.print("> Hardware Check (Press Enter)");

      BusManager::getInstance().requestDisplay();
    } while (display->display.nextPage());
  }

  bool handleInput(UIKey key) override {
    if (key == UI_KEY_ENTER) {
      DisplayDriver *displayDrv = uiManager->getDisplayDriver();
      displayDrv->showStatus("Manual Check...", 0);

      uint8_t addresses[] = {RX8010_I2C_ADDR, SHT30_I2C_ADDR, ES8311_ADDRESS,
                             M5807_ADDR_FULL_ACCESS};
      const char *names[] = {"RTC", "Sensor", "Audio", "Radio"};

      bool allOk = true;
      for (int i = 0; i < 4; i++) {
        char buffer[32];
        if (sensor->checkDevice(addresses[i])) {
          sprintf(buffer, "%s: OK", names[i]);
        } else {
          sprintf(buffer, "%s: FAIL", names[i]);
          allOk = false;
        }
        displayDrv->showStatus(buffer, i + 1);
        delay(200);
      }

      if (allOk) {
        config->config.hw_checked = true;
        config->save();
        displayDrv->showStatus("All Devices OK", 0);
      } else {
        displayDrv->showStatus("Check Failed!", 0);
      }
      delay(2000);
      draw(displayDrv);              // Redraw settings after check
    } else if (key == UI_KEY_LEFT) { // Back
      uiManager->switchScreen(SCREEN_MENU);
    }
    return false;
  }

private:
  ConfigManager *config;
  StatusBar *statusBar;
  SensorDriver *sensor;
};
