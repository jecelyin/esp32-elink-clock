#pragma once

#include <Arduino.h>

namespace ES8311AlarmCodec {

// Initializes the ES8311 DAC for the board's BCLK-only I2S wiring. The I2S
// driver must already be running so the codec has a clock during reset.
bool begin();

} // namespace ES8311AlarmCodec
