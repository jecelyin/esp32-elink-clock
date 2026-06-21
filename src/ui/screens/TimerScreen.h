#pragma once

#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"
#include <Arduino.h>

enum TimerMode { MODE_1H, MODE_30M, MODE_10M };

class TimerScreen : public Screen {
public:
  TimerScreen(StatusBar *statusBar) : statusBar(statusBar) {
    timeLeft = 30 * 60;
    totalTime = 30 * 60;
    isActive = false;
    mode = MODE_30M;
    focusIndex = 4; // Default focus on Start/Pause
    isFirstDraw = true;
    lastUpdateMs = 0;

    // State tracking for partial updates
    lastTimeLeft = -1;
    lastIsActive = false;
    lastFocusIndex = -1;
    lastMode = MODE_30M;
  }

  void enter() override {
    isFirstDraw = true;
    lastTimeLeft = -1;
    lastFocusIndex = -1;
  }

  uint32_t getIdleSleepIntervalMs() const override {
    return isActive ? 1000UL : 3600000UL;
  }

  void update() override {
    if (!isActive)
      return;

    uint32_t now = millis();
    if (now - lastUpdateMs >= 1000) {
      lastUpdateMs = now;
      if (timeLeft > 0) {
        timeLeft--;
      } else {
        isActive = false;
      }
      // Redraw will be triggered by UIManager or we can call draw here
      // But UIManager usually calls draw if onInput returns true OR
      // if Screen::update calls a redraw.
      // In this project, screens tend to handle their own partials in update or
      // draw. Let's call draw(display) immediately for time updates if active.
      if (uiManager) {
        draw(uiManager->getDisplayDriver());
      }
    }
  }

  void draw(DisplayDriver *displayDrv) override {
    if (!displayDrv)
      return;
    this->displayDrv = displayDrv;

    if (isFirstDraw) {
      isFirstDraw = false;
      drawFull();
      updateLastState();
      return;
    }

    // Partial updates based on state changes
    if (timeLeft != lastTimeLeft) {
      updateTimeDisplay();
      updateProgressBar();
      lastTimeLeft = timeLeft;
    }

    if (isActive != lastIsActive) {
      updatePhrase();
      refreshButton(4); // Start/Pause label change
      if (!isActive) {
        // Redraw side buttons (+/-) which appear when paused
        refreshButton(5);
        refreshButton(6);
      }
      lastIsActive = isActive;
    }

    if (focusIndex != lastFocusIndex) {
      if (lastFocusIndex != -1)
        refreshButton(lastFocusIndex);
      refreshButton(focusIndex);
      lastFocusIndex = focusIndex;
    }

    if (mode != lastMode) {
      updatePhrase(); // Mode name change
      lastMode = mode;
    }
  }

  bool onInput(UIKey key) override {
    if (key == UI_KEY_LEFT) {
      focusIndex--;
      if (focusIndex < 0)
        focusIndex = 6;
      return true; // Trigger draw for focus change
    } else if (key == UI_KEY_RIGHT) {
      focusIndex++;
      if (focusIndex > 6)
        focusIndex = 0;
      return true; // Trigger draw for focus change
    } else if (key == UI_KEY_ENTER) {
      executeAction();
      return true; // Trigger draw for action
    }
    return false;
  }

private:
  struct PartialRect {
    int x;
    int y;
    int w;
    int h;
  };

  static const int SCREEN_W = 400;

  static PartialRect getTimeRect() { return PartialRect{0, 64, SCREEN_W, 106}; }

  static PartialRect getModeTextRect() {
    return PartialRect{0, 44, SCREEN_W, 24};
  }

  static PartialRect getStatusTextRect() {
    return PartialRect{0, 170, SCREEN_W, 30};
  }

  void updateLastState() {
    lastTimeLeft = timeLeft;
    lastIsActive = isActive;
    lastFocusIndex = focusIndex;
    lastMode = mode;
  }

  void drawFull() {
    Serial.println("TimerScreen: Full Render");
    displayDrv->display.setFullWindow();
    displayDrv->display.firstPage();
    do {
      displayDrv->display.fillScreen(GxEPD_WHITE);
      statusBar->draw(displayDrv, true);

      drawPhrase();
      drawLargeTime();
      drawProgressBar();

      for (int i = 0; i < 7; i++) {
        drawButton(i);
      }

    } while (displayDrv->display.nextPage());
    displayDrv->powerOff();
  }

  // --- Partial Update Methods ---

  void refreshPartialArea(const PartialRect &rect,
                          void (TimerScreen::*drawContent)()) {
    displayDrv->display.setPartialWindow(rect.x, rect.y, rect.w, rect.h);
    displayDrv->display.firstPage();
    do {
      displayDrv->display.fillRect(rect.x, rect.y, rect.w, rect.h,
                                   GxEPD_WHITE);
      (this->*drawContent)();
    } while (displayDrv->display.nextPage());
    displayDrv->powerOff();
  }

  void updateTimeDisplay() {
    // 关键逻辑：92 号数字基线在 160，顶部会高于旧窗口 y=80；
    // 同时窗口底部必须停在底部文案之前，避免每秒刷新擦掉“学习中”。
    PartialRect rect = getTimeRect();
    refreshPartialArea(rect, &TimerScreen::drawLargeTime);
  }

  void updateProgressBar() {
    // x: 16, w: 368, y: 205, h: 15
    displayDrv->display.setPartialWindow(16, 205, 368, 15);
    displayDrv->display.firstPage();
    do {
      displayDrv->display.fillRect(16, 205, 368, 15, GxEPD_WHITE);
      drawProgressBar();
    } while (displayDrv->display.nextPage());
    displayDrv->powerOff();
  }

  void updatePhrase() {
    // 关键逻辑：顶部模式文案和底部状态文案不能用一个跨越时间数字的
    // 大窗口刷新，否则开始/暂停时会把中间的大数字清掉一截。
    PartialRect modeRect = getModeTextRect();
    PartialRect statusRect = getStatusTextRect();
    refreshPartialArea(modeRect, &TimerScreen::drawModeText);
    refreshPartialArea(statusRect, &TimerScreen::drawStatusText);
  }

  void refreshButton(int index) {
    int x, y, w, h;
    getButtonRect(index, x, y, w, h);

    // Align x and w to 8 pixels
    int ax = (x / 8) * 8;
    int aw = ((x + w + 7) / 8) * 8 - ax;

    displayDrv->display.setPartialWindow(ax, y, aw, h);
    displayDrv->display.firstPage();
    do {
      displayDrv->display.fillRect(ax, y, aw, h, GxEPD_WHITE);
      drawButton(index);
    } while (displayDrv->display.nextPage());
    displayDrv->powerOff();
  }

  // --- Drawing Primitives ---

  void drawPhrase() {
    drawModeText();
    drawStatusText();
  }

  void drawModeText() {
    auto &u8g2 = displayDrv->u8g2Fonts;
    u8g2.setFont(u8g2_font_helvB10_tf);
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);

    bool isBreak = (mode == MODE_10M);
    const char *modeText = isBreak ? "课间休息" : "深度专注";
    int tw = u8g2.getUTF8Width(modeText);
    u8g2.setCursor((SCREEN_W - tw) / 2, 60);
    u8g2.print(modeText);
  }

  void drawStatusText() {
    auto &u8g2 = displayDrv->u8g2Fonts;
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);

    bool isBreak = (mode == MODE_10M);
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    const char *phrase =
        isActive ? (isBreak ? "放松大脑..." : "沉浸学习中...") : "准备好了吗？";
    int pw = u8g2.getUTF8Width(phrase);
    u8g2.setCursor((SCREEN_W - pw) / 2, 185);
    u8g2.print(phrase);
  }

  void drawLargeTime() {
    auto &u8g2 = displayDrv->u8g2Fonts;
    char timeStr[12];
    int hrs = timeLeft / 3600;
    int mins = (timeLeft % 3600) / 60;
    int secs = timeLeft % 60;

    if (hrs > 0) {
      snprintf(timeStr, sizeof(timeStr), "%d:%02d:%02d", hrs, mins, secs);
      u8g2.setFont(u8g2_font_logisoso62_tn);
    } else {
      snprintf(timeStr, sizeof(timeStr), "%02d:%02d", mins, secs);
      u8g2.setFont(u8g2_font_logisoso92_tn);
    }

    int tw = u8g2.getUTF8Width(timeStr);
    u8g2.setCursor((SCREEN_W - tw) / 2, 160);
    u8g2.print(timeStr);
  }

  void drawProgressBar() {
    int x = 20, y = 210, w = 360, h = 6;
    displayDrv->display.drawRect(x, y, w, h, GxEPD_BLACK);
    if (totalTime > 0) {
      int progressW = (w - 4) * (totalTime - timeLeft) / totalTime;
      progressW = constrain(progressW, 0, w - 4);
      displayDrv->display.fillRect(x + 2, y + 2, progressW, h - 4, GxEPD_BLACK);
    }
  }

  void drawButton(int index) {
    if (isActive && (index == 5 || index == 6))
      return; // Hide adjustment buttons when active

    int x, y, w, h;
    getButtonRect(index, x, y, w, h);
    bool isFocused = (focusIndex == index);

    if (isFocused) {
      displayDrv->display.fillRect(x, y, w, h, GxEPD_BLACK);
      displayDrv->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
      displayDrv->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    } else {
      displayDrv->display.drawRect(x, y, w, h, GxEPD_BLACK);
      displayDrv->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
      displayDrv->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    }

    displayDrv->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    const char *label = "";
    switch (index) {
    case 0:
      label = "1H";
      break;
    case 1:
      label = "30M";
      break;
    case 2:
      label = "10M";
      break;
    case 3:
      label = "RESET";
      break;
    case 4:
      label = isActive ? "PAUSE" : "START";
      break;
    case 5:
      label = "+";
      break;
    case 6:
      label = "-";
      break;
    }

    int lw = displayDrv->u8g2Fonts.getUTF8Width(label);
    displayDrv->u8g2Fonts.setCursor(x + (w - lw) / 2, y + h - 8);
    displayDrv->u8g2Fonts.print(label);

    // Default colors
    displayDrv->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    displayDrv->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  }

  void getButtonRect(int index, int &x, int &y, int &w, int &h) {
    switch (index) {
    case 0:
      x = 20;
      y = 230;
      w = 60;
      h = 25;
      break;
    case 1:
      x = 85;
      y = 230;
      w = 60;
      h = 25;
      break;
    case 2:
      x = 150;
      y = 230;
      w = 60;
      h = 25;
      break;
    case 3:
      x = 230;
      y = 230;
      w = 70;
      h = 25;
      break;
    case 4:
      x = 310;
      y = 230;
      w = 70;
      h = 25;
      break;
    case 5:
      x = 360;
      y = 100;
      w = 30;
      h = 25;
      break;
    case 6:
      x = 360;
      y = 135;
      w = 30;
      h = 25;
      break;
    default:
      x = 0;
      y = 0;
      w = 0;
      h = 0;
      break;
    }
  }

  void executeAction() {
    switch (focusIndex) {
    case 0:
      setTimer(60);
      mode = MODE_1H;
      break;
    case 1:
      setTimer(30);
      mode = MODE_30M;
      break;
    case 2:
      setTimer(10);
      mode = MODE_10M;
      break;
    case 3:
      isActive = false;
      if (mode == MODE_1H)
        setTimer(60);
      else if (mode == MODE_30M)
        setTimer(30);
      else
        setTimer(10);
      break;
    case 4:
      isActive = !isActive;
      break;
    case 5:
      if (!isActive)
        adjustTimer(1);
      break;
    case 6:
      if (!isActive)
        adjustTimer(-1);
      break;
    }
  }

  void setTimer(int mins) {
    timeLeft = mins * 60;
    totalTime = timeLeft;
  }

  void adjustTimer(int mins) {
    timeLeft += mins * 60;
    if (timeLeft < 60)
      timeLeft = 60;
    totalTime = timeLeft;
  }

  StatusBar *statusBar;
  DisplayDriver *displayDrv;
  int timeLeft, totalTime;
  bool isActive;
  TimerMode mode;
  int focusIndex;
  uint32_t lastUpdateMs;
  bool isFirstDraw;

  // Last state tracking
  int lastTimeLeft;
  bool lastIsActive;
  int lastFocusIndex;
  TimerMode lastMode;
};
