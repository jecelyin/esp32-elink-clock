#pragma once

#include "../../managers/AlarmManager.h"
#include "../../managers/BusManager.h"
#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"
#include <vector>

class AlarmScreen : public Screen {
public:
  AlarmScreen(AlarmManager *alarmMgr, StatusBar *statusBar)
      : alarmMgr(alarmMgr), statusBar(statusBar) {
    focusedControl = 0;
    isEditing = false;
    focusedEditControl = 0;
    isFirstDraw = true;
  }

  void init() override {
    isFirstDraw = true;
    isEditing = false;
    focusedControl = 0;
  }

  void enter() override {
    isFirstDraw = true;
    isEditing = false;
    focusedControl = 0;
  }

  void draw(DisplayDriver *display) override {
    if (isFirstDraw) {
      isFirstDraw = false;

      display->display.setFullWindow();
      display->display.firstPage();
      do {
        display->display.fillScreen(GxEPD_WHITE);
        statusBar->draw(display, true);

        if (isEditing) {
          drawModal(display, false);
        } else {
          drawAlarmList(display, false);
        }

        BusManager::getInstance().requestDisplay();
      } while (display->display.nextPage());
      display->powerOff();
      return;
    }
  }

  bool handleInput(UIKey key) override {
    if (isEditing) {
      return handleEditInput(key);
    } else {
      return handleListInput(key);
    }
  }

  bool onLongPress() override {
    if (!isEditing && focusedControl < (int)alarmMgr->alarms.size()) {
      startEditing();
      uiManager->getDisplayDriver()->display.setFullWindow();
      draw(uiManager->getDisplayDriver());
      return false;
    }
    return false;
  }

private:
  AlarmManager *alarmMgr;
  StatusBar *statusBar;

  int focusedControl; // 0..N-1: Alarms, N: Back button
  bool isFirstDraw;

  bool isEditing;
  int focusedEditControl; // 0-6: Days, 7-9: Presets, 10: Save, 11: Cancel
  uint8_t tempDays;

  static const int ROW_H = 60;
  static const int START_Y = 28;

  void drawAlarmList(DisplayDriver *display, bool partial) {
    int n = alarmMgr->alarms.size();
    for (int i = 0; i < n; i++) {
      drawAlarmRow(display, i, (focusedControl == i), partial);
    }
    drawBackRow(display, n, (focusedControl == n), partial);
  }

  void drawAlarmRow(DisplayDriver *display, int idx, bool focused,
                    bool partial) {
    int y = START_Y + idx * ROW_H;
    Alarm &alarm = alarmMgr->alarms[idx];

    if (partial) {
      display->display.setPartialWindow(0, y, 400, ROW_H);
      display->display.firstPage();
    }

    do {
      if (partial)
        display->display.fillRect(0, y, 400, ROW_H, GxEPD_WHITE);

      if (focused) {
        display->display.drawRect(4, y + 4, 392, ROW_H - 8, GxEPD_BLACK);
      }

      // Time
      display->u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      char buf[16];
      sprintf(buf, "%02d:%02d", alarm.hour, alarm.minute);
      display->u8g2Fonts.setCursor(25, y + 40);
      display->u8g2Fonts.setForegroundColor(alarm.enabled ? GxEPD_BLACK
                                                          : GxEPD_LIGHTGREY);
      display->u8g2Fonts.print(buf);

      // Repeat Text
      display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
      display->u8g2Fonts.setCursor(140, y + 38);
      display->u8g2Fonts.print(getRepeatText(alarm.days).c_str());

      // Toggle Switch
      int swW = 36, swH = 20, swX = 340, swY = y + (ROW_H - swH) / 2;
      display->display.drawRoundRect(swX, swY, swW, swH, 10, GxEPD_BLACK);
      if (alarm.enabled) {
        display->display.fillRoundRect(swX, swY, swW, swH, 10, GxEPD_BLACK);
        display->display.fillCircle(swX + swW - 10, swY + 10, 7, GxEPD_WHITE);
      } else {
        display->display.fillCircle(swX + 10, swY + 10, 7, GxEPD_BLACK);
      }

      display->display.drawLine(10, y + ROW_H - 1, 390, y + ROW_H - 1,
                                GxEPD_BLACK);

      if (partial)
        BusManager::getInstance().requestDisplay();
    } while (partial && display->display.nextPage());

    if (partial)
      display->powerOff();
    display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  }

  void drawBackRow(DisplayDriver *display, int idx, bool focused,
                   bool partial) {
    int y = START_Y + idx * ROW_H;
    if (partial) {
      display->display.setPartialWindow(0, y, 400, ROW_H);
      display->display.firstPage();
    }
    do {
      if (partial)
        display->display.fillRect(0, y, 400, ROW_H, GxEPD_WHITE);
      if (focused) {
        display->display.fillRect(100, y + 10, 200, 40, GxEPD_BLACK);
        display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
      } else {
        display->display.drawRect(100, y + 10, 200, 40, GxEPD_BLACK);
        display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
      }
      display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
      int tw = display->u8g2Fonts.getUTF8Width("返回主菜单");
      display->u8g2Fonts.setCursor(200 - tw / 2, y + 36);
      display->u8g2Fonts.print("返回主菜单");
      if (partial)
        BusManager::getInstance().requestDisplay();
    } while (partial && display->display.nextPage());
    if (partial)
      display->powerOff();
    display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  }

  void updateControlFocus(DisplayDriver *display, int oldIdx, int newIdx) {
    int n = alarmMgr->alarms.size();
    if (oldIdx == n)
      drawBackRow(display, oldIdx, false, true);
    else if (oldIdx >= 0 && oldIdx < n)
      drawAlarmRow(display, oldIdx, false, true);

    if (newIdx == n)
      drawBackRow(display, newIdx, true, true);
    else if (newIdx >= 0 && newIdx < n)
      drawAlarmRow(display, newIdx, true, true);
  }

  void startEditing() {
    isEditing = true;
    focusedEditControl = 0;
    tempDays = alarmMgr->alarms[focusedControl].days;
  }

  void drawModal(DisplayDriver *display, bool partial) {
    int mx = 20, my = 40, mw = 360, mh = 240;
    if (partial) {
      display->display.setPartialWindow(mx, my, mw + 1, mh + 1);
      display->display.firstPage();
    }
    do {
      display->display.fillRect(mx, my, mw, mh, GxEPD_WHITE);
      display->display.drawRect(mx, my, mw, mh, GxEPD_BLACK);
      display->u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      display->u8g2Fonts.setCursor(mx + 20, my + 30);
      char title[32];
      sprintf(title, "ALARM #%d REPEAT", focusedControl + 1);
      display->u8g2Fonts.print(title);
      display->display.drawLine(mx + 10, my + 40, mx + mw - 10, my + 40,
                                GxEPD_BLACK);

      int dayOrder[] = {1, 2, 3, 4, 5, 6, 0};
      const char *dayNames[] = {"日", "一", "二", "三", "四", "五", "六"};
      display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
      for (int i = 0; i < 7; i++) {
        int d = dayOrder[i], dx = mx + 20 + i * 47, dy = my + 60, dw = 40,
            dh = 40;
        bool isSelected = (tempDays & (1 << d));
        bool isFocused = (focusedEditControl == i);
        if (isSelected) {
          display->display.fillRect(dx, dy, dw, dh, GxEPD_BLACK);
          display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
        } else {
          display->display.drawRect(dx, dy, dw, dh, GxEPD_BLACK);
          display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        }
        if (isFocused)
          display->display.drawRect(dx - 3, dy - 3, dw + 6, dh + 6,
                                    GxEPD_BLACK);
        int tw = display->u8g2Fonts.getUTF8Width(dayNames[d]);
        display->u8g2Fonts.setCursor(dx + (dw - tw) / 2, dy + 28);
        display->u8g2Fonts.print(dayNames[d]);
      }
      display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);

      const char *presets[] = {"工作日", "每天", "一次"};
      for (int i = 0; i < 3; i++) {
        int px = mx + 20 + i * 115, py = my + 115, pw = 105, ph = 40;
        bool isFocused = (focusedEditControl == 7 + i);
        if (isFocused) {
          display->display.fillRect(px, py, pw, ph, GxEPD_BLACK);
          display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
        } else {
          display->display.drawRect(px, py, pw, ph, GxEPD_BLACK);
          display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        }
        int tw = display->u8g2Fonts.getUTF8Width(presets[i]);
        display->u8g2Fonts.setCursor(px + (pw - tw) / 2, py + 28);
        display->u8g2Fonts.print(presets[i]);
      }
      display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);

      const char *actions[] = {"保存", "取消"};
      for (int i = 0; i < 2; i++) {
        int ax = mx + 20 + i * 175, ay = my + 175, aw = 165, ah = 50;
        bool isFocused = (focusedEditControl == 10 + i);
        if (isFocused) {
          display->display.fillRect(ax, ay, aw, ah, GxEPD_BLACK);
          display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
        } else {
          display->display.drawRect(ax, ay, aw, ah, GxEPD_BLACK);
          display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        }
        display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
        int tw = display->u8g2Fonts.getUTF8Width(actions[i]);
        display->u8g2Fonts.setCursor(ax + (aw - tw) / 2, ay + 32);
        display->u8g2Fonts.print(actions[i]);
      }
      display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);

      if (partial)
        BusManager::getInstance().requestDisplay();
    } while (partial && display->display.nextPage());
    if (partial)
      display->powerOff();
  }

  bool handleListInput(UIKey key) {
    DisplayDriver *display = uiManager->getDisplayDriver();
    int n = alarmMgr->alarms.size();
    if (key == UI_KEY_LEFT) {
      int prev = focusedControl;
      focusedControl = (focusedControl - 1 + (n + 1)) % (n + 1);
      updateControlFocus(display, prev, focusedControl);
      return false;
    } else if (key == UI_KEY_RIGHT) {
      int prev = focusedControl;
      focusedControl = (focusedControl + 1) % (n + 1);
      updateControlFocus(display, prev, focusedControl);
      return false;
    } else if (key == UI_KEY_ENTER) {
      if (focusedControl == n) {
        uiManager->switchScreen(SCREEN_MENU);
        return false;
      }
      alarmMgr->alarms[focusedControl].enabled =
          !alarmMgr->alarms[focusedControl].enabled;
      drawAlarmRow(display, focusedControl, true, true);
      return false;
    }
    return false;
  }

  bool handleEditInput(UIKey key) {
    DisplayDriver *display = uiManager->getDisplayDriver();
    if (key == UI_KEY_LEFT) {
      focusedEditControl = (focusedEditControl - 1 + 12) % 12;
      drawModal(display, true);
      return false;
    } else if (key == UI_KEY_RIGHT) {
      focusedEditControl = (focusedEditControl + 1) % 12;
      drawModal(display, true);
      return false;
    } else if (key == UI_KEY_ENTER) {
      if (focusedEditControl < 7) {
        int dayOrder[] = {1, 2, 3, 4, 5, 6, 0};
        tempDays ^= (1 << dayOrder[focusedEditControl]);
        drawModal(display, true);
      } else if (focusedEditControl == 7) {
        tempDays = 0x3E;
        drawModal(display, true);
      } else if (focusedEditControl == 8) {
        tempDays = 0x7F;
        drawModal(display, true);
      } else if (focusedEditControl == 9) {
        tempDays = 0x00;
        drawModal(display, true);
      } else if (focusedEditControl == 10) {
        alarmMgr->alarms[focusedControl].days = tempDays;
        isEditing = false;
        isFirstDraw = true;
        draw(display);
      } else if (focusedEditControl == 11) {
        isEditing = false;
        isFirstDraw = true;
        draw(display);
      }
      return false;
    }
    return false;
  }

  String getRepeatText(uint8_t days) {
    if (days == 0x7F)
      return "每天";
    if (days == 0)
      return "一次";
    if (days == 0x3E)
      return "工作日";
    if (days == 0x41)
      return "周末";
    String res = "周";
    const char *names[] = {"日", "一", "二", "三", "四", "五", "六"};
    bool first = true;
    for (int i = 1; i < 7; i++) {
      if (days & (1 << i)) {
        if (!first)
          res += " ";
        res += names[i];
        first = false;
      }
    }
    if (days & (1 << 0)) {
      if (!first)
        res += " ";
      res += names[0];
    }
    return res;
  }
};
