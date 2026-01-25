#pragma once

#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"

class MenuScreen : public Screen {
public:
  MenuScreen(StatusBar *statusBar) : statusBar(statusBar) {
    menuIndex = 0;
    lastMenuIndex = 0;
    items[0] = {SCREEN_HOME, "Home", u8g2_font_open_iconic_embedded_4x_t, 'D'};
    items[1] = {SCREEN_ALARM, "Alarm", u8g2_font_open_iconic_embedded_4x_t,
                'A'};
    items[2] = {SCREEN_MUSIC, "Music", u8g2_font_open_iconic_play_4x_t, 'C'};
    items[3] = {SCREEN_RADIO, "Radio", u8g2_font_open_iconic_embedded_4x_t,
                'F'};
    items[4] = {SCREEN_WEATHER, "Weather", u8g2_font_open_iconic_weather_4x_t,
                '@'};
    items[5] = {SCREEN_SETTINGS, "Settings",
                u8g2_font_open_iconic_embedded_4x_t, 'B'};
    items[6] = {SCREEN_TIMER, "Timer", u8g2_font_open_iconic_embedded_4x_t,
                'E'};
    items[7] = {SCREEN_NETWORK_CONFIG, "Config",
                u8g2_font_open_iconic_embedded_4x_t, 'F'};
    firstDraw = true;
  }

  void enter() override {
    firstDraw = true;
    lastMenuIndex = menuIndex;
  }

  void draw(DisplayDriver *display) override {
    Serial.printf(
        "MenuScreen::draw firstDraw=%d menuIndex=%d lastMenuIndex=%d\n",
        firstDraw, menuIndex, lastMenuIndex);
    if (firstDraw) {
      display->display.setFullWindow();
      display->display.firstPage();
      do {
        display->display.fillScreen(GxEPD_WHITE);
        statusBar->draw(display, true);
        for (int i = 0; i < 8; i++) {
          drawMenuItem(display, i);
        }
      } while (display->display.nextPage());
      display->powerOff();

      firstDraw = false;
    } else {
      Serial.println("Partial update start");
      // Partial update for the item that WAS selected (now unselected)
      updateItem(display, lastMenuIndex);

      // Partial update for the item that IS selected (now selected)
      updateItem(display, menuIndex);
      Serial.println("Partial update end");
    }

    // Update lastMenuIndex for next time
    lastMenuIndex = menuIndex;
  }

  bool onLongPress() override {
    uiManager->switchScreen(SCREEN_HOME);
    return false;
  }

  bool handleInput(UIKey key) override {

    if (key == UI_KEY_LEFT) { // Left (KEY_LEFT)
      lastMenuIndex = menuIndex;
      menuIndex--;
      if (menuIndex < 0)
        menuIndex = 7;
      Serial.printf("Input LEFT: new index=%d\n", menuIndex);
      return true;
    } else if (key == UI_KEY_RIGHT) { // Right (KEY_RIGHT)
      lastMenuIndex = menuIndex;
      menuIndex++;
      if (menuIndex > 7)
        menuIndex = 0;
      Serial.printf("Input RIGHT: new index=%d\n", menuIndex);
      return true;
    } else if (key == UI_KEY_ENTER) { // Select (KEY_ENTER Short)
      Serial.printf("Input ENTER: switch to screen id=%d\n",
                    items[menuIndex].id);
      uiManager->switchScreen(items[menuIndex].id);
      return false;
    }
    return false;
  }

private:
  void updateItem(DisplayDriver *display, int index) {
    int x, y, w, h;
    getMenuItemRect(index, x, y, w, h);

    // Align to byte boundaries (8 pixels) for x and w
    // This is often required for correct partial updates on E-Ink
    int x_start_aligned = (x / 8) * 8;
    int x_end = x + w;
    int x_end_aligned = ((x_end + 7) / 8) * 8;
    int w_aligned = x_end_aligned - x_start_aligned;

    Serial.printf("updateItem index=%d raw: %d,%d %dx%d aligned: %d,%d %dx%d\n",
                  index, x, y, w, h, x_start_aligned, y, w_aligned, h);

    display->display.setPartialWindow(x_start_aligned, y, w_aligned, h);

    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);
      drawMenuItem(display, index);
    } while (display->display.nextPage());
    display->powerOff();
  }

  void drawMenuItem(DisplayDriver *display, int i) {
    int x_start = 40;
    int y_start = 75;
    int x_gap = 120;
    int y_gap = 85;

    int col = i % 3;
    int row = i / 3;
    int x = x_start + col * x_gap;
    int y = y_start + row * y_gap;

    // Debug info
    bool isSelected = (i == menuIndex);
    Serial.printf("drawMenuItem i=%d menuIndex=%d isSelected=%d x=%d y=%d\n", i,
                  menuIndex, isSelected, x, y);

    // Draw Selection Box
    if (isSelected) {
      display->display.fillRect(x - 10, y - 45, 100, 80, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
      display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    } else {
      display->display.drawRect(x - 10, y - 45, 100, 80, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
      display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    }

    // Draw Icon
    // Box Specs: x start: x-10, width: 100.
    // Center X = (x-10) + (100/2) = x + 40.
    int boxCenterX = x + 40;

    // Draw Icon
    display->u8g2Fonts.setFont(items[i].font);
    int iconWidth =
        u8g2_GetGlyphWidth(&(display->u8g2Fonts.u8g2), items[i].icon);
    Serial.printf("Icon: code=%d width=%d\n", items[i].icon, iconWidth);

    display->u8g2Fonts.drawGlyph(boxCenterX - (iconWidth / 2), y,
                                 items[i].icon);

    // Draw Label
    display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    int labelWidth = display->u8g2Fonts.getUTF8Width(items[i].label);
    Serial.printf("Label: %s width=%d\n", items[i].label, labelWidth);

    display->u8g2Fonts.setCursor(boxCenterX - (labelWidth / 2), y + 22);
    display->u8g2Fonts.print(items[i].label);

    // Reset Colors
    display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  }

  void getMenuItemRect(int index, int &x, int &y, int &w, int &h) {
    int col = index % 3;
    int row = index / 3;

    int center_x = 40 + col * 120;
    int center_y = 75 + row * 85;

    x = center_x - 10;
    y = center_y - 45;
    w = 100;
    h = 80;
  }

  struct MenuItem {
    ScreenState id;
    const char *label;
    const uint8_t *font;
    uint16_t icon;
  };
  MenuItem items[8];
  int menuIndex;
  int lastMenuIndex;
  StatusBar *statusBar;
  bool firstDraw;
};
