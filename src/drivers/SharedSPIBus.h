#pragma once

#include <SPI.h>

namespace SharedSPIBus {
SPIClass &bus();
void begin();
void prepareDisplay();
void prepareSDCard();
void releaseSDCard();
} // namespace SharedSPIBus
