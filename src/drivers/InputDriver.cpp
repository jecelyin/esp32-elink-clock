#include "InputDriver.h"

InputDriver::InputDriver() {}

void InputDriver::begin() {
    pinMode(KEY_ENTER, INPUT_PULLUP);
    pinMode(KEY_LEFT, INPUT_PULLUP);
    pinMode(KEY_RIGHT, INPUT_PULLUP);
}

ButtonEvent InputDriver::loop() {
    unsigned long now = millis();
    ButtonEvent e = BTN_NONE;

    // --- LEFT BUTTON ---
    int leftState = digitalRead(KEY_LEFT);
    if (leftState != lastLeftState) {
        lastLeftDebounce = now;
    }
    if ((now - lastLeftDebounce) > DEBOUNCE_DELAY) {
        // If state stabilized low (pressed)
        // Wait, for click usually we detect on RELEASE or PRESS. 
        // Let's detect on PRESS (Low) but ensure we don't spam.
        // Actually standard debounce logic often maintains a "stable state" variable.
        // Simplified: Detect falling edge with debounce.
        static int leftStable = HIGH;
        if (leftState != leftStable) {
            leftStable = leftState;
            if (leftStable == LOW) {
                e = BTN_LEFT_CLICK;
            }
        }
    }
    lastLeftState = leftState;

    // --- RIGHT BUTTON ---
    int rightState = digitalRead(KEY_RIGHT);
    if (rightState != lastRightState) {
        lastRightDebounce = now;
    }
    if ((now - lastRightDebounce) > DEBOUNCE_DELAY) {
        static int rightStable = HIGH;
        if (rightState != rightStable) {
            rightStable = rightState;
            if (rightStable == LOW) {
                 e = BTN_RIGHT_CLICK;
            }
        }
    }
    lastRightState = rightState;

    // --- ENTER BUTTON (Long Press Logic) ---
    int enterState = digitalRead(KEY_ENTER);
    
    // Press started
    if (lastEnterState == HIGH && enterState == LOW) {
        enterPressTime = now;
        enterLongHandled = false;
    }
    
    // Holding
    if (enterState == LOW) {
        Serial.println("ENTER holding...");
        if (!enterLongHandled && (now - enterPressTime > LONG_PRESS_DELAY)) {
            e = BTN_ENTER_LONG;
            enterLongHandled = true; // Prevent re-triggering
        }
    }

    // Released
    if (lastEnterState == LOW && enterState == HIGH) {
        if (!enterLongHandled) {
             // Short press released
             // Maybe add a minimum short press time to debounce noise? > 50ms
             if (now - enterPressTime > DEBOUNCE_DELAY) {
                 e = BTN_ENTER_SHORT;
             }
        }
    }
    lastEnterState = enterState;

    return e;
}
