#include "InputDriver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

Button::Button(uint8_t pin, ButtonEvent shortPressEvent,
               ButtonEvent longPressEvent,
               uint16_t longPressRepeatGapMs)
    : pin(pin), shortPressEvent(shortPressEvent),
      longPressEvent(longPressEvent),
      longPressRepeatGapMs(longPressRepeatGapMs) {}

void Button::begin() {
  pinMode(pin, INPUT_PULLUP);

  // 关键逻辑：按键仅使用主循环轮询，避免 GPIO CHANGE 中断在收音机页
  // 因电源噪声/按键抖动形成中断风暴。
  int initialState = digitalRead(pin);
  lastPhysicalState = initialState;
  stableState = initialState;
  lastDebounceTime = millis();

  // 关键逻辑：不要在运行期挂 GPIO CHANGE 中断。
  // Radio 页面打开功放/收音机后，按键线偶发抖动会形成中断风暴；
  // 实机已出现 Core1 卡在 gpio_intr_service 的 Interrupt WDT。
  // 主循环 50ms 轮询足够识别按键，light sleep 唤醒另由 gpio_wakeup 负责。
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
  lastLongPressEventTime = 0;
  longPressed = false;
  lastDebounceTime = now;
#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Input][wake-sync] pin=%u at=%lums\n", pin, now);
#endif
}

ButtonEvent Button::update() {
  int physicalState = digitalRead(pin);
  unsigned long now = millis();
  if (consumeSuppressedRelease(physicalState, now)) {
    return BTN_NONE;
  }

  ButtonEvent polledEvent = updatePolledState(physicalState, now);
  if (polledEvent != BTN_NONE) {
    return polledEvent;
  }
  return detectLongPress(physicalState, now);
}

void Button::suppressUntilReleased() {
  // 关键逻辑：ENTER 长按被业务消费后，后续释放沿只代表结束本次手势。
  // 如果不屏蔽，释放抖动可能被轮询状态机当成短按，菜单页会立刻确认 Home。
  suppressedUntilRelease = true;
  suppressedReleaseStartTime = 0;
}

void Button::clearPendingPresses() {
  syncPolledReleasedState(millis());
}

bool Button::isPressed() { return digitalRead(pin) == LOW; }

ButtonEvent Button::updatePolledState(int physicalState, unsigned long now) {
  if (physicalState != lastPhysicalState) {
    lastDebounceTime = now;
  }

  if ((now - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (physicalState != stableState) {
      int previousStableState = stableState;
      stableState = physicalState;
      if (stableState == LOW) {
        pressStartTime = now;
        longPressed = false;
      } else if (previousStableState == LOW && !longPressed) {
        uint32_t duration = now - pressStartTime;
        pressStartTime = 0;
        if (duration > TAP_TIMEOUT) {
          return shortPressEvent;
        }
      }
    }
  }

  lastPhysicalState = physicalState;
  return BTN_NONE;
}

ButtonEvent Button::detectLongPress(int physicalState, unsigned long now) {
  if (longPressEvent == BTN_NONE || physicalState != LOW ||
      stableState != LOW) {
    return BTN_NONE;
  }

  if (!longPressed && (now - pressStartTime > LONG_PRESS_TIMEOUT)) {
    longPressed = true;
    lastLongPressEventTime = now;
#if ENABLE_SERIAL_DEBUG
    Serial.printf("[Input][poll-long] event=%d pin=%u hold=%lums\n",
                  longPressEvent, pin, now - pressStartTime);
#endif
    return longPressEvent;
  }

  if (longPressed && longPressRepeatGapMs > 0 &&
      now - lastLongPressEventTime >= longPressRepeatGapMs) {
    lastLongPressEventTime = now;
    return longPressEvent;
  }

  return BTN_NONE;
}

bool Button::consumeSuppressedRelease(int physicalState, unsigned long now) {
  if (!suppressedUntilRelease) {
    return false;
  }

  if (physicalState == LOW) {
    suppressedReleaseStartTime = 0;
    lastPhysicalState = LOW;
    stableState = LOW;
    return true;
  }

  if (suppressedReleaseStartTime == 0) {
    suppressedReleaseStartTime = now;
    return true;
  }

  if (now - suppressedReleaseStartTime <= DEBOUNCE_DELAY) {
    return true;
  }

  suppressedUntilRelease = false;
  syncPolledReleasedState(now);
  return true;
}

void Button::syncPolledReleasedState(unsigned long now) {
  // 关键逻辑：释放沿锁存的事件被消费后，必须同步轮询状态为已释放。
  // 否则处理按键触发屏幕刷新后，旧的 LOW 状态可能跨过长按阈值，
  // 下一轮就会被误判成长按。
  stableState = HIGH;
  lastPhysicalState = HIGH;
  pressStartTime = 0;
  lastLongPressEventTime = 0;
  longPressed = false;
  lastDebounceTime = now;
}

InputDriver::InputDriver()
    : enterButton(KEY_ENTER, BTN_ENTER_SHORT, BTN_ENTER_LONG),
      leftButton(KEY_LEFT, BTN_LEFT_SHORT, BTN_LEFT_LONG, 260),
      rightButton(KEY_RIGHT, BTN_RIGHT_SHORT, BTN_RIGHT_LONG, 260) {}

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

void InputDriver::suppressEnterUntilReleased() {
  enterButton.suppressUntilReleased();
}

void InputDriver::clearPendingEnterPresses() {
  enterButton.clearPendingPresses();
}

void InputDriver::clearPendingEnterPressesIfDirectionActive() {
  // 关键逻辑：LEFT/RIGHT 是全局移动光标手势，硬件串扰混入 ENTER 时
  // 不能让 ENTER 先于方向键被消费；否则短按会触发当前焦点动作。
  if (leftButton.isPressed() || rightButton.isPressed()) {
    clearPendingEnterPresses();
  }
}

ButtonEvent InputDriver::loop() {
  ButtonEvent ev = enterButton.update();
  if (ev != BTN_NONE) {
    return ev;
  }

  ev = leftButton.update();
  if (ev != BTN_NONE) {
    return ev;
  }

  ev = rightButton.update();
  if (ev != BTN_NONE) {
    return ev;
  }

  return BTN_NONE;
}
