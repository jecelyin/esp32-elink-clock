#pragma once

#include "../drivers/DisplayDriver.h"

class UIManager; // Forward declaration

enum UIKey {
  UI_KEY_NONE = 0,
  UI_KEY_ENTER = 1,
  UI_KEY_LEFT = 2,
  UI_KEY_RIGHT = 3
};

class Screen {
public:
  virtual ~Screen() {}
  virtual void init() {}
  virtual void enter() {}
  virtual void exit() {}
  virtual void update() {} // Called periodically
  // 关键逻辑：屏幕自己声明“最短空闲睡眠周期”，
  // 这样休眠调度和页面刷新节奏绑定，避免主循环硬编码 1 秒频繁睡醒。
  virtual uint32_t getIdleSleepIntervalMs() const { return 3600000UL; }
  virtual void draw(DisplayDriver *display) = 0;
  virtual bool handleInput(UIKey key) = 0;
  virtual bool onLongPress() { return false; }

  void setUIManager(UIManager *mgr) { uiManager = mgr; }

protected:
  UIManager *uiManager;
};
