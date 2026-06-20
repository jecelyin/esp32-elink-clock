#pragma once

#include <Arduino.h>

namespace I2CBus {

void begin();
bool lock(uint32_t timeoutMs = 200);
void unlock();
bool recover();
bool powerDownSharedBus(uint32_t timeoutMs = 200);
bool probeDevice(uint8_t address, uint32_t timeoutMs = 200);

class Guard {
public:
  explicit Guard(uint32_t timeoutMs = 200);
  ~Guard();
  bool isLocked() const;

private:
  bool locked = false;
};

} // namespace I2CBus
