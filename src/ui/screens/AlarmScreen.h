#pragma once

#include "../../managers/AlarmManager.h"
#include "../../managers/BusManager.h"
#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"

class AlarmScreen : public Screen {
public:
  AlarmScreen(AlarmManager *alarmMgr, StatusBar *statusBar)
      : alarmMgr(alarmMgr), statusBar(statusBar) {}

  void draw(DisplayDriver *display) override {
    display->display.setFullWindow();
    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);
      statusBar->draw(display, true);

      // Header
      display->u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      display->u8g2Fonts.setCursor(150, 40);
      display->u8g2Fonts.print("ALARM");

      // Icon
      display->u8g2Fonts.setFont(u8g2_font_open_iconic_embedded_4x_t);
      display->u8g2Fonts.setCursor(20, 60);
      display->u8g2Fonts.print("A");

      // List Alarms
      display->u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      int y = 100;
      for (const auto &alarm : alarmMgr->alarms) {
        display->u8g2Fonts.setCursor(80, y);
        char buf[32];
        sprintf(buf, "%02d:%02d %s", alarm.hour, alarm.minute,
                alarm.enabled ? "[ON]" : "[OFF]");
        display->u8g2Fonts.print(buf);
        y += 30;
      }

      // Instructions
      display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
      display->u8g2Fonts.setCursor(50, 280);
      display->u8g2Fonts.print("Press MENU to return");

      BusManager::getInstance().requestDisplay();
    } while (display->display.nextPage());
    display->powerOff();
  }

  bool handleInput(UIKey key) override {
    if (key == UI_KEY_ENTER) { // Menu/Back
      uiManager->switchScreen(SCREEN_MENU);
    }
    return false;
  }

private:
  AlarmManager *alarmMgr;
  StatusBar *statusBar;
};
