#pragma once

#include "../config.h"
#include <Arduino.h>

// Button events
enum ButtonEvent {
  BTN_NONE = 0,
  BTN_ENTER_SHORT,
  BTN_ENTER_LONG,
  BTN_LEFT_SHORT,
  BTN_LEFT_LONG,
  BTN_RIGHT_SHORT,
  BTN_RIGHT_LONG
};

class Button {
public:
  Button(uint8_t pin, ButtonEvent shortPressEvent, ButtonEvent longPressEvent,
         uint16_t longPressRepeatGapMs = 0);
  void begin();
  ButtonEvent update();
  void syncPressedState(unsigned long now);
  void suppressUntilReleased();
  void clearPendingPresses();
  bool isPressed();

private:
  bool consumeSuppressedRelease(int physicalState, unsigned long now);
  ButtonEvent detectLongPress(int physicalState, unsigned long now);
  void syncPolledReleasedState(unsigned long now);
  ButtonEvent updatePolledState(int physicalState, unsigned long now);

  uint8_t pin;
  ButtonEvent shortPressEvent;
  ButtonEvent longPressEvent;
  uint16_t longPressRepeatGapMs;
  int lastPhysicalState = HIGH;
  int stableState = HIGH;
  unsigned long lastDebounceTime = 0;
  unsigned long pressStartTime = 0;
  unsigned long lastLongPressEventTime = 0;
  unsigned long suppressedReleaseStartTime = 0;
  bool longPressed = false;

  bool suppressedUntilRelease = false;

  static const unsigned long DEBOUNCE_DELAY = 50;
  static const unsigned long TAP_TIMEOUT = 120;
  static const unsigned long LONG_PRESS_TIMEOUT = 600;
};

class InputDriver {
public:
  InputDriver();
  void begin();
  ButtonEvent loop();
  void syncWakePressedButtons();
  void suppressEnterUntilReleased();
  void clearPendingEnterPresses();
  void clearPendingEnterPressesIfDirectionActive();

private:
  Button enterButton;
  Button leftButton;
  Button rightButton;
};
