#pragma once

#include "../../managers/ConfigManager.h"
#include "../../managers/BusManager.h"
#include "../components/StatusBar.h"
#include "../Screen.h"
#include "../UIManager.h"

class SettingsScreen : public Screen {
public:
  SettingsScreen(ConfigManager *config, StatusBar *statusBar) : config(config), statusBar(statusBar) {}

  void draw(DisplayDriver *display) override {
    display->display.setFullWindow();
    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);
      statusBar->draw(display);

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

      BusManager::getInstance().requestDisplay();
    } while (display->display.nextPage());
  }

  void handleInput(int key) override {
    if (key == 1) { // Menu/Back
      uiManager->switchScreen(SCREEN_MENU);
    }
  }

private:
  ConfigManager *config;
  StatusBar *statusBar;
};
