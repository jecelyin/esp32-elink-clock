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
  virtual void draw(DisplayDriver *display) = 0;
  virtual void handleInput(UIKey key) = 0;
  virtual void onLongPress() {}

  void setUIManager(UIManager *mgr) { uiManager = mgr; }

protected:
  UIManager *uiManager;
};
