#include "InputDriver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

Button::Button(uint8_t pin, ButtonEvent shortPressEvent, bool supportsLongPress,
               uint16_t shortPressRepeatGapMs)
    : pin(pin), shortPressEvent(shortPressEvent),
      supportsLongPress(supportsLongPress),
      shortPressRepeatGapMs(shortPressRepeatGapMs) {}

void Button::begin() {
  pinMode(pin, INPUT_PULLUP);

  // 关键逻辑：轮询状态机和中断锁存状态必须同时对齐当前电平。
  // 菜单页电子墨水屏局刷期间主循环会暂停数百毫秒，短按可能完整发生在
  // 这段阻塞窗口内；中断锁存负责把这些短按先记下来，后续主循环再消费。
  int initialState = digitalRead(pin);
  lastPhysicalState = initialState;
  stableState = initialState;
  lastDebounceTime = millis();
  irqLastState = initialState;
  irqLastChangeTime = lastDebounceTime;
  irqPressStartTime = initialState == LOW ? lastDebounceTime : 0;
  irqLastQueuedShortTime = 0;

  attachInterruptArg(digitalPinToInterrupt(pin), handleInterruptThunk, this,
                     CHANGE);
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
  syncInterruptPressedState(now);
}

ButtonEvent Button::update() {
  ButtonEvent pendingEvent = consumePendingEvent();
  if (pendingEvent != BTN_NONE) {
    return pendingEvent;
  }

  int physicalState = digitalRead(pin);
  unsigned long now = millis();
  updatePolledState(physicalState, now);
  return detectLongPress(physicalState, now);
}

void Button::updatePolledState(int physicalState, unsigned long now) {
  if (physicalState != lastPhysicalState) {
    lastDebounceTime = now;
  }

  if ((now - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (physicalState != stableState) {
      stableState = physicalState;
      if (stableState == LOW) {
        pressStartTime = now;
        longPressed = false;
      }
    }
  }

  lastPhysicalState = physicalState;
}

ButtonEvent Button::detectLongPress(int physicalState, unsigned long now) {
  if (supportsLongPress && physicalState == LOW && stableState == LOW &&
      !longPressed && (now - pressStartTime > LONG_PRESS_DELAY)) {
    longPressed = true;
    noInterrupts();
    irqLongHandled = true;
    interrupts();
    return BTN_ENTER_LONG;
  }

  return BTN_NONE;
}

ButtonEvent Button::consumePendingEvent() {
  ButtonEvent event = BTN_NONE;
  bool consumedShortPress = false;
  bool consumedLongPress = false;

  noInterrupts();
  if (pendingLongPressCount > 0) {
    pendingLongPressCount--;
    event = BTN_ENTER_LONG;
    consumedLongPress = true;
  } else if (pendingShortPressCount > 0) {
    pendingShortPressCount--;
    event = shortPressEvent;
    consumedShortPress = true;
  }
  interrupts();

  if (consumedShortPress || consumedLongPress) {
    syncPolledReleasedState(millis());
  }

  return event;
}

void Button::syncInterruptPressedState(unsigned long now) {
  noInterrupts();
  irqLastState = LOW;
  irqLastChangeTime = now;
  irqPressStartTime = now;
  irqLongHandled = false;
  interrupts();
}

void Button::syncPolledReleasedState(unsigned long now) {
  // 关键逻辑：释放沿锁存的事件被消费后，必须同步轮询状态为已释放。
  // 否则处理按键触发屏幕刷新后，旧的 LOW 状态可能跨过 500ms 阈值，
  // 下一轮就会被误判成长按。
  stableState = HIGH;
  lastPhysicalState = HIGH;
  pressStartTime = 0;
  longPressed = false;
  lastDebounceTime = now;
}

void IRAM_ATTR Button::handleInterruptThunk(void *arg) {
  static_cast<Button *>(arg)->handleInterrupt();
}

void IRAM_ATTR Button::handleInterrupt() {
  int physicalState = digitalRead(pin);
  uint32_t now = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;

  if (physicalState == irqLastState ||
      now - irqLastChangeTime < DEBOUNCE_DELAY) {
    return;
  }

  irqLastChangeTime = now;
  irqLastState = physicalState;
  if (physicalState == LOW) {
    irqPressStartTime = now;
    irqLongHandled = false;
    return;
  }

  uint32_t duration = now - irqPressStartTime;
  if (duration <= DEBOUNCE_DELAY || irqLongHandled) {
    return;
  }

  if (queueLongPressFromInterrupt(duration)) {
    return;
  }

  // 关键逻辑：真实按住时间不足长按阈值时，释放沿只锁存短按；
  // 长按兜底已在上方按物理时长处理，避免短按被错误升级。
  queueShortPressFromInterrupt(now);
}

bool IRAM_ATTR Button::queueLongPressFromInterrupt(uint32_t duration) {
  if (!supportsLongPress || duration <= LONG_PRESS_DELAY) {
    return false;
  }

  // 关键逻辑：设置页启动配网 AP 和电子墨水屏全刷都可能让主循环错过
  // “仍按住”的 500ms 窗口；释放沿记录的是物理按住时长，可安全兜底长按。
  if (pendingLongPressCount < MAX_PENDING_EVENTS) {
    pendingLongPressCount++;
  }
  irqLongHandled = true;
  return true;
}

void IRAM_ATTR Button::queueShortPressFromInterrupt(uint32_t now) {
  // 关键逻辑：真实连按和释放回弹都表现为多次 HIGH 释放沿。
  // 这里按键级别限制最小短按间隔，防止 ENTER 的一次释放被排成两次事件；
  // left/right 的间隔更短，仍然支持快速切换时排队。
  if (irqLastQueuedShortTime != 0 &&
      now - irqLastQueuedShortTime < shortPressRepeatGapMs) {
    return;
  }

  if (pendingShortPressCount < MAX_PENDING_EVENTS) {
    pendingShortPressCount++;
    irqLastQueuedShortTime = now;
  }
}

InputDriver::InputDriver()
    : enterButton(KEY_ENTER, BTN_ENTER_SHORT, true, 450),
      leftButton(KEY_LEFT, BTN_LEFT_CLICK, false, 120),
      rightButton(KEY_RIGHT, BTN_RIGHT_CLICK, false, 120) {}

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
