#include "InputDriver.h"

Button::Button(uint8_t pin, const char *name) : pin(pin), name(name) {}

void Button::begin() {}

ButtonEvent Button::update() {
  int physicalState = digitalRead(pin);
  unsigned long now = millis();
  ButtonEvent event = BTN_NONE;

  if (physicalState == LOW) {
    Serial.printf("[Button:%s] Pressed\n", name);
  }

  if (physicalState != lastPhysicalState) {
    lastDebounceTime = now;
  }

  if ((now - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (physicalState != stableState) {
      stableState = physicalState;
      if (stableState == LOW) {
        // Button Pressed
        pressStartTime = now;
        longPressed = false;
        Serial.printf("[Button:%s] Pressed 2\n", name);
      } else {
        // Button Released
        unsigned long duration = now - pressStartTime;
        Serial.printf("[Button:%s] Released after %lums\n", name, duration);
        if (!longPressed && duration > DEBOUNCE_DELAY) {
          if (strcmp(name, "ENTER") == 0)
            event = BTN_ENTER_SHORT;
          else if (strcmp(name, "LEFT") == 0)
            event = BTN_LEFT_CLICK;
          else if (strcmp(name, "RIGHT") == 0)
            event = BTN_RIGHT_CLICK;
        }
      }
    }
  }

  if (stableState == LOW && !longPressed &&
      (now - pressStartTime > LONG_PRESS_DELAY)) {
    longPressed = true;
    Serial.printf("[Button:%s] Long Press detected\n", name);
    if (strcmp(name, "ENTER") == 0)
      event = BTN_ENTER_LONG;
  }

  lastPhysicalState = physicalState;
  return event;
}

InputDriver::InputDriver()
    : enterButton(KEY_ENTER, "ENTER"), leftButton(KEY_LEFT, "LEFT"),
      rightButton(KEY_RIGHT, "RIGHT") {}

void InputDriver::begin() {
  enterButton.begin();
  leftButton.begin();
  rightButton.begin();
}

ButtonEvent InputDriver::loop() {
  ButtonEvent e = BTN_NONE;

  ButtonEvent ev = enterButton.update();
  if (ev != BTN_NONE)
    e = ev;

  ev = leftButton.update();
  if (ev != BTN_NONE)
    e = ev;
  // todo: right button is float
  // ev = rightButton.update();
  // if (ev != BTN_NONE) e = ev;

  return e;
}
