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
  Button(uint8_t pin, ButtonEvent shortPressEvent,
         bool supportsLongPress = false);
  void begin();
  ButtonEvent update();
  void syncPressedState(unsigned long now);

private:
  static void IRAM_ATTR handleInterruptThunk(void *arg);
  void IRAM_ATTR handleInterrupt();
  void IRAM_ATTR queueShortPressFromInterrupt();

  ButtonEvent consumePendingEvent();
  ButtonEvent detectLongPress(int physicalState, unsigned long now);
  void syncInterruptPressedState(unsigned long now);
  void syncPolledReleasedState(unsigned long now);
  void updatePolledState(int physicalState, unsigned long now);

  uint8_t pin;
  ButtonEvent shortPressEvent;
  bool supportsLongPress;
  int lastPhysicalState = HIGH;
  int stableState = HIGH;
  unsigned long lastDebounceTime = 0;
  unsigned long pressStartTime = 0;
  bool longPressed = false;

  volatile int irqLastState = HIGH;
  volatile uint32_t irqLastChangeTime = 0;
  volatile uint32_t irqPressStartTime = 0;
  volatile uint8_t pendingShortPressCount = 0;
  volatile bool irqLongHandled = false;

  static const uint8_t MAX_PENDING_EVENTS = 8;
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
