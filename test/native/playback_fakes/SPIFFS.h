#pragma once

#include <Arduino.h>
#include <FS.h>

class FakeFlash : public fs::FS {
public:
  bool exists(const String &) const;
};

extern FakeFlash SPIFFS;
