#pragma once

#include "../drivers/DisplayDriver.h"

class UIManager; // Forward declaration

enum UIKey {
  UI_KEY_NONE = 0,
  UI_KEY_ENTER = 1,
  UI_KEY_LEFT = 2,
  UI_KEY_RIGHT = 3,
  UI_KEY_ENTER_LONG = 4,
  UI_KEY_LEFT_LONG = 5,
  UI_KEY_RIGHT_LONG = 6
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
  // 关键逻辑：onInput 返回 true 表示页面已接收并处理事件；
  // false 表示当前页面不接收该事件，或 UIManager 正在绘制中。
  virtual bool onInput(UIKey key) = 0;
  // 关键逻辑：大多数页面由 UIManager 在输入后统一重绘；
  // 已在 onInput 内完成局刷/全刷的页面可关闭自动重绘。
  virtual bool shouldDrawAfterInput() const { return true; }

  void setUIManager(UIManager *mgr) { uiManager = mgr; }

protected:
  UIManager *uiManager;
};
