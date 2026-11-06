#pragma once

#include <cassert>
#include <cstdint>

using SemaphoreHandle_t = int *;

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
  static int depth = 0;
  return &depth;
}

inline bool xSemaphoreTake(SemaphoreHandle_t mutex, uint32_t) {
  assert(*mutex == 0);
  ++*mutex;
  return true;
}

inline bool xSemaphoreGive(SemaphoreHandle_t mutex) {
  assert(*mutex == 1);
  --*mutex;
  return true;
}
