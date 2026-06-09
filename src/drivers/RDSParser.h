#pragma once

#include <Arduino.h>

class RDSParser {
public:
  RDSParser();

  // Process a set of 4 RDS blocks
  void decode(uint16_t block1, uint16_t block2, uint16_t block3,
              uint16_t block4);

  // Reset buffers (e.g. on frequency change)
  void reset();

  // Get decoded information
  const char *getPSName() const { return _psName; }
  const char *getRadioText() const { return _radioText; }

  bool hasPSChanged() {
    bool changed = _psChanged;
    _psChanged = false;
    return changed;
  }
  bool hasRTChanged() {
    bool changed = _rtChanged;
    _rtChanged = false;
    return changed;
  }

private:
  char _psName[9];     // Program Service Name (8 chars + null)
  char _radioText[65]; // Radio Text (64 chars + null)

  uint8_t _psStatus; // Bitmask for received segments (4 bits)
  uint8_t _rtStatus; // Bitmask for received segments (16 bits)
  bool _rtABFlag;    // Text A/B flag

  bool _psChanged;
  bool _rtChanged;

  void _handleGroup0(uint16_t block2, uint16_t block3, uint16_t block4);
  void _handleGroup2(uint16_t block2, uint16_t block3, uint16_t block4);
};
