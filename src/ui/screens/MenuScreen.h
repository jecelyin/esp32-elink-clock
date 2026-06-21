#pragma once

#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"

class MenuScreen : public Screen {
public:
  MenuScreen(StatusBar *statusBar) : statusBar(statusBar) {
    menuIndex = 0;
    lastMenuIndex = 0;
    items[0] =
        MenuItem{SCREEN_HOME, "Home", u8g2_font_open_iconic_embedded_4x_t, 'D'};
    items[1] = MenuItem{SCREEN_CALENDAR, "Calendar",
                        u8g2_font_open_iconic_embedded_4x_t, 'C'};
    items[2] = MenuItem{SCREEN_ALARM, "Alarm",
                        u8g2_font_open_iconic_embedded_4x_t, 'A'};
    items[3] =
        MenuItem{SCREEN_MUSIC, "Music", u8g2_font_open_iconic_play_4x_t, 'C'};
    items[4] = MenuItem{SCREEN_RADIO, "Radio",
                        u8g2_font_open_iconic_embedded_4x_t, 'F'};
    items[5] = MenuItem{SCREEN_WEATHER, "Weather",
                        u8g2_font_open_iconic_weather_4x_t, '@'};
    items[6] = MenuItem{SCREEN_SETTINGS, "Settings",
                        u8g2_font_open_iconic_embedded_4x_t, 'B'};
    items[7] = MenuItem{SCREEN_TIMER, "Timer",
                        u8g2_font_open_iconic_embedded_4x_t, 'E'};
    firstDraw = true;
  }

  void enter() override {
    firstDraw = true;
    lastMenuIndex = menuIndex;
  }

  void draw(DisplayDriver *display) override {
    if (firstDraw) {
      drawFull(display);
      firstDraw = false;
    } else {
      updateChangedItems(display);
    }

    lastMenuIndex = menuIndex;
  }

  bool handleInput(UIKey key) override {

    if (key == UI_KEY_LEFT) {
      moveSelection(-1);
      return true;
    } else if (key == UI_KEY_RIGHT) {
      moveSelection(1);
      return true;
    } else if (key == UI_KEY_ENTER) {
      uiManager->switchScreen(items[menuIndex].id);
      return false;
    }
    return false;
  }

private:
  static const int MENU_ITEM_COUNT = 8;
  static const int SCREEN_WIDTH = 400;

  struct MenuRect {
    int x;
    int y;
    int w;
    int h;
  };

  void drawFull(DisplayDriver *display) {
    display->display.setFullWindow();
    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);
      statusBar->draw(display, true);
      for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        drawMenuItem(display, i);
      }
    } while (display->display.nextPage());
    display->powerOff();
  }

  void updateChangedItems(DisplayDriver *display) {
    if (lastMenuIndex == menuIndex)
      return;

    MenuRect dirty = getDirtyRect();
    display->display.setPartialWindow(dirty.x, dirty.y, dirty.w, dirty.h);
    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);
      drawItemsInRect(display, dirty);
    } while (display->display.nextPage());
    display->powerOff();
  }

  void moveSelection(int delta) {
    lastMenuIndex = menuIndex;
    menuIndex = (menuIndex + delta + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
  }

  MenuRect getDirtyRect() {
    MenuRect dirty =
        mergeRects(getMenuItemRect(lastMenuIndex), getMenuItemRect(menuIndex));
    alignPartialRect(dirty);
    return dirty;
  }

  void drawItemsInRect(DisplayDriver *display, const MenuRect &dirty) {
    // 关键逻辑：合并后的局刷区域可能跨行，白底重绘会覆盖中间菜单项。
    // 因此不能只画新旧两个 item，必须把脏区域内相交的 item 全部补回。
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
      if (rectsIntersect(getMenuItemRect(i), dirty)) {
        drawMenuItem(display, i);
      }
    }
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

    bool isSelected = (i == menuIndex);

    if (isSelected) {
      display->display.fillRect(x - 10, y - 45, 100, 80, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
      display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    } else {
      display->display.drawRect(x - 10, y - 45, 100, 80, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
      display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    }

    int boxCenterX = x + 40;

    display->u8g2Fonts.setFont(items[i].font);
    int iconWidth =
        u8g2_GetGlyphWidth(&(display->u8g2Fonts.u8g2), items[i].icon);

    display->u8g2Fonts.drawGlyph(boxCenterX - (iconWidth / 2), y,
                                 items[i].icon);

    display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    int labelWidth = display->u8g2Fonts.getUTF8Width(items[i].label);

    display->u8g2Fonts.setCursor(boxCenterX - (labelWidth / 2), y + 22);
    display->u8g2Fonts.print(items[i].label);

    display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  }

  MenuRect getMenuItemRect(int index) {
    int col = index % 3;
    int row = index / 3;

    int center_x = 40 + col * 120;
    int center_y = 75 + row * 85;

    return MenuRect{center_x - 10, center_y - 45, 100, 80};
  }

  MenuRect mergeRects(const MenuRect &a, const MenuRect &b) {
    int left = min(a.x, b.x);
    int top = min(a.y, b.y);
    int right = max(a.x + a.w, b.x + b.w);
    int bottom = max(a.y + a.h, b.y + b.h);
    return MenuRect{left, top, right - left, bottom - top};
  }

  void alignPartialRect(MenuRect &rect) {
    // 关键逻辑：电子墨水屏局刷 X 坐标和宽度需要按 8 像素字节边界对齐。
    int alignedX = (rect.x / 8) * 8;
    int alignedRight = ((rect.x + rect.w + 7) / 8) * 8;
    rect.x = max(0, alignedX);
    rect.w = min(SCREEN_WIDTH, alignedRight) - rect.x;
  }

  bool rectsIntersect(const MenuRect &a, const MenuRect &b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h &&
           a.y + a.h > b.y;
  }

  struct MenuItem {
    ScreenState id;
    const char *label;
    const uint8_t *font;
    uint16_t icon;
  };
  MenuItem items[MENU_ITEM_COUNT];
  int menuIndex;
  int lastMenuIndex;
  StatusBar *statusBar;
  bool firstDraw;
};
