#pragma once

#include "../config.h"
#include <Arduino.h>

// Button events
enum ButtonEvent {
  BTN_NONE = 0,
  BTN_ENTER_SHORT,
  BTN_ENTER_LONG,
  BTN_LEFT_CLICK,
  BTN_RIGHT_CLICK
};

class Button {
public:
    Button(uint8_t pin, const char* name, ButtonEvent shortPressEvent,
           bool supportsLongPress = false);
    void begin();
    ButtonEvent update();
    void syncPressedState(unsigned long now);

private:
    uint8_t pin;
    const char* name;
    ButtonEvent shortPressEvent;
    bool supportsLongPress;
    int lastPhysicalState = HIGH;
    int stableState = HIGH;
    unsigned long lastDebounceTime = 0;
    unsigned long pressStartTime = 0;
    bool longPressed = false;
    
    static const unsigned long DEBOUNCE_DELAY = 50;
    static const unsigned long LONG_PRESS_DELAY = 500;
};

class InputDriver {
public:
  InputDriver();
  void begin();
  ButtonEvent loop();
  void syncWakePressedButtons();

private:
  Button enterButton;
  Button leftButton;
  Button rightButton;
};
