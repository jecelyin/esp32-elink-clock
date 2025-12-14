#pragma once

#include <Arduino.h>
#include "../config.h"

// Button events
enum ButtonEvent {
    BTN_NONE = 0,
    BTN_ENTER_SHORT,
    BTN_ENTER_LONG,
    BTN_LEFT_CLICK,
    BTN_RIGHT_CLICK
};

class InputDriver {
public:
    InputDriver();
    void begin();
    ButtonEvent loop();

private:
    // State variables for button press detection
    int lastEnterState = HIGH;
    unsigned long enterPressTime = 0;
    bool enterLongHandled = false;

    int lastLeftState = HIGH;
    unsigned long lastLeftDebounce = 0;
    
    int lastRightState = HIGH;
    unsigned long lastRightDebounce = 0;

    // Adjust these constants as needed
    const unsigned long DEBOUNCE_DELAY = 50; 
    const unsigned long LONG_PRESS_DELAY = 1000;
};
