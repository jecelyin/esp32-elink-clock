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
         uint16_t shortPressRepeatGapMs = 120,
         uint16_t longPressRepeatGapMs = 0);
  void begin();
  ButtonEvent update();
  void syncPressedState(unsigned long now);
  void suppressUntilReleased();
  bool hasPendingPress();
  void clearPendingPresses();
  bool isPressed();

private:
  static void IRAM_ATTR handleInterruptThunk(void *arg);
  void IRAM_ATTR handleInterrupt();
  void IRAM_ATTR queueShortPressFromInterrupt(uint32_t now);
  bool IRAM_ATTR queueLongPressFromInterrupt(uint32_t duration);

  ButtonEvent consumePendingEvent();
  bool consumeSuppressedRelease(int physicalState, unsigned long now);
  ButtonEvent detectLongPress(int physicalState, unsigned long now);
  void clearPendingCounts();
  void syncInterruptPressedState(unsigned long now);
  void syncPolledReleasedState(unsigned long now);
  void updatePolledState(int physicalState, unsigned long now);

  uint8_t pin;
  ButtonEvent shortPressEvent;
  ButtonEvent longPressEvent;
  uint16_t shortPressRepeatGapMs;
  uint16_t longPressRepeatGapMs;
  int lastPhysicalState = HIGH;
  int stableState = HIGH;
  unsigned long lastDebounceTime = 0;
  unsigned long pressStartTime = 0;
  unsigned long lastLongPressEventTime = 0;
  unsigned long suppressedReleaseStartTime = 0;
  bool longPressed = false;

  volatile int irqLastState = HIGH;
  volatile uint32_t irqLastChangeTime = 0;
  volatile uint32_t irqPressStartTime = 0;
  volatile uint32_t irqLastQueuedShortTime = 0;
  volatile uint8_t pendingShortPressCount = 0;
  volatile uint8_t pendingLongPressCount = 0;
  volatile bool irqLongHandled = false;
  volatile bool suppressedUntilRelease = false;

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
  void suppressEnterUntilReleased();
  void clearPendingEnterPresses();
  void clearPendingEnterPressesIfDirectionActive();

private:
  Button enterButton;
  Button leftButton;
  Button rightButton;
};
