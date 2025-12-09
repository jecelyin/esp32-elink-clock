#pragma once

#include "../../drivers/RadioDriver.h"
#include "../../managers/BusManager.h"
#include "../components/StatusBar.h"
#include "../Screen.h"
#include "../UIManager.h"

class RadioScreen : public Screen {
public:
  RadioScreen(RadioDriver *radio, StatusBar *statusBar) : radio(radio), statusBar(statusBar) {}

  void draw(DisplayDriver *display) override {
    display->display.setFullWindow();
    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);
      statusBar->draw(display);

      // Header
      display->u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      display->u8g2Fonts.setCursor(120, 40);
      display->u8g2Fonts.print("FM RADIO");

      // Icon
      display->u8g2Fonts.setFont(u8g2_font_open_iconic_embedded_4x_t);
      display->u8g2Fonts.setCursor(20, 60);
      display->u8g2Fonts.print("R"); // Assuming R is radio or similar

      // Frequency
      display->u8g2Fonts.setFont(u8g2_font_logisoso42_tn);
      display->u8g2Fonts.setCursor(100, 150);
      uint16_t freq = radio->getFrequency();
      display->u8g2Fonts.print(freq / 100.0, 1);

      display->u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      display->u8g2Fonts.setCursor(250, 150);
      display->u8g2Fonts.print("MHz");

      // Controls
      display->u8g2Fonts.setFont(u8g2_font_helvB12_tf);
      display->u8g2Fonts.setCursor(50, 250);
      display->u8g2Fonts.print("[<] Seek Down   [>] Seek Up");

      BusManager::getInstance().requestDisplay();
    } while (display->display.nextPage());
  }

  void handleInput(int key) override {
    if (key == 1) { // Menu/Back
      uiManager->switchScreen(SCREEN_MENU);
    } else if (key == 2) { // Left
      radio->seekDown();
      // Trigger redraw needed? UIManager loop handles it
    } else if (key == 3) { // Right
      radio->seekUp();
    }
  }

private:
  RadioDriver *radio;
  StatusBar *statusBar;
};
