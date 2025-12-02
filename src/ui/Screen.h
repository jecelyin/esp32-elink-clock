#pragma once

#include "../drivers/DisplayDriver.h"

class UIManager; // Forward declaration

class Screen {
public:
  virtual ~Screen() {}
  virtual void init() {}
  virtual void enter() {}
  virtual void exit() {}
  virtual void update() {} // Called periodically
  virtual void draw(DisplayDriver *display) = 0;
  virtual void handleInput(int key) = 0;

  void setUIManager(UIManager *mgr) { uiManager = mgr; }

protected:
  UIManager *uiManager;
};
