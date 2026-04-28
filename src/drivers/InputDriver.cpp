#include "InputDriver.h"

Button::Button(uint8_t pin, const char *name, ButtonEvent shortPressEvent,
               bool supportsLongPress)
    : pin(pin), name(name), shortPressEvent(shortPressEvent),
      supportsLongPress(supportsLongPress) {}

void Button::begin() {
  pinMode(pin, INPUT_PULLUP);

  // 关键逻辑：按键状态机启动时必须与当前物理电平对齐，
  // 否则从轻睡眠唤醒后的第一帧很容易把真实按键误判成抖动。
  int initialState = digitalRead(pin);
  lastPhysicalState = initialState;
  stableState = initialState;
  lastDebounceTime = millis();
}

void Button::syncPressedState(unsigned long now) {
  if (digitalRead(pin) != LOW) {
    return;
  }

  // 关键逻辑：ESP32 在 light sleep 中被按键唤醒时，
  // 主循环错过了“按下沿”，这里主动把状态机同步到已按下状态，
  // 这样后续释放时才能稳定产出 click 事件。
  stableState = LOW;
  lastPhysicalState = LOW;
  pressStartTime = now;
  longPressed = false;
  lastDebounceTime = now;
}

ButtonEvent Button::update() {
  int physicalState = digitalRead(pin);
  unsigned long now = millis();
  ButtonEvent event = BTN_NONE;

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
        // Serial.printf("[Button:%s] Pressed 2\n", name);
      } else {
        // Button Released
        unsigned long duration = now - pressStartTime;
        Serial.printf("[Button:%s] Released after %lums\n", name, duration);
        if (!longPressed && duration > DEBOUNCE_DELAY) {
          event = shortPressEvent;
        }
      }
    }
  }

  if (supportsLongPress && stableState == LOW && !longPressed &&
      (now - pressStartTime > LONG_PRESS_DELAY)) {
    longPressed = true;
    Serial.printf("[Button:%s] Long Press detected\n", name);
    event = BTN_ENTER_LONG;
  }

  lastPhysicalState = physicalState;
  return event;
}

InputDriver::InputDriver()
    : enterButton(KEY_ENTER, "ENTER", BTN_ENTER_SHORT, true),
      leftButton(KEY_LEFT, "LEFT", BTN_LEFT_CLICK),
      rightButton(KEY_RIGHT, "RIGHT", BTN_RIGHT_CLICK) {}

void InputDriver::begin() {
  enterButton.begin();
  leftButton.begin();
  rightButton.begin();
}

void InputDriver::syncWakePressedButtons() {
  unsigned long now = millis();
  enterButton.syncPressedState(now);
  leftButton.syncPressedState(now);
  rightButton.syncPressedState(now);
}

ButtonEvent InputDriver::loop() {
  ButtonEvent e = BTN_NONE;

  ButtonEvent ev = enterButton.update();
  if (ev != BTN_NONE)
    e = ev;

  ev = leftButton.update();
  if (ev != BTN_NONE)
    e = ev;

  ev = rightButton.update();
  if (ev != BTN_NONE)
    e = ev;

  return e;
}
