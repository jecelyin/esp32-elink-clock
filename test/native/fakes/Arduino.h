#pragma once
#include "RuntimeFakes.h"
#include <algorithm>
using std::min;
constexpr int LOW = 0;
constexpr int HIGH = 1;
void digitalWrite(uint8_t pin, int value);
