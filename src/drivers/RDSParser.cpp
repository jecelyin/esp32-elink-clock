#include "RDSParser.h"

RDSParser::RDSParser() { reset(); }

void RDSParser::reset() {
  memset(_psName, ' ', 8);
  _psName[8] = '\0';
  memset(_radioText, ' ', 64);
  _radioText[64] = '\0';

  _psStatus = 0;
  _rtStatus = 0;
  _rtABFlag = false;
  _psChanged = false;
  _rtChanged = false;
}

void RDSParser::decode(uint16_t block1, uint16_t block2, uint16_t block3,
                       uint16_t block4) {
  // Block B contains Group Type and Version
  uint8_t groupType = (block2 >> 12) & 0xF;
  uint8_t groupVersion = (block2 >> 11) & 0x1;

  switch (groupType) {
  case 0: // Group 0: Basic tuning and switching information (PS)
    _handleGroup0(block2, block3, block4);
    break;
  case 2: // Group 2: Radio Text
    _handleGroup2(block2, block3, block4);
    break;
  default:
    // Other groups not implemented yet
    break;
  }
}

void RDSParser::_handleGroup0(uint16_t block2, uint16_t block3,
                              uint16_t block4) {
  // Bits 1-0 of block 2 are the segment index
  uint8_t idx = block2 & 0x3;
  char c1 = (char)(block4 >> 8);
  char c2 = (char)(block4 & 0xFF);

  // Basic validation: filter non-printable characters
  if (c1 < 32 || c1 > 126)
    c1 = ' ';
  if (c2 < 32 || c2 > 126)
    c2 = ' ';

  if (_psName[idx * 2] != c1 || _psName[idx * 2 + 1] != c2) {
    _psName[idx * 2] = c1;
    _psName[idx * 2 + 1] = c2;
    _psChanged = true;
  }
}

void RDSParser::_handleGroup2(uint16_t block2, uint16_t block3,
                              uint16_t block4) {
  bool abFlag = (block2 & 0x10) != 0;
  uint8_t idx = block2 & 0xF;

  if (abFlag != _rtABFlag) {
    // Clear text on flag toggle
    memset(_radioText, ' ', 64);
    _rtABFlag = abFlag;
    _rtStatus = 0;
    _rtChanged = true;
  }

  char chars[4];
  int count = 0;

  // Group 2A (version 0) carries 4 characters in blocks C and D
  // Group 2B (version 1) carries 2 characters in block D
  uint8_t version = (block2 >> 11) & 0x1;

  if (version == 0) {
    chars[0] = (char)(block3 >> 8);
    chars[1] = (char)(block3 & 0xFF);
    chars[2] = (char)(block4 >> 8);
    chars[3] = (char)(block4 & 0xFF);
    count = 4;
  } else {
    chars[0] = (char)(block4 >> 8);
    chars[1] = (char)(block4 & 0xFF);
    count = 2;
  }

  for (int i = 0; i < count; i++) {
    char c = chars[i];
    if (c < 32 || c > 126)
      c = ' ';

    uint8_t pos = (version == 0) ? (idx * 4 + i) : (idx * 2 + i);
    if (pos < 64 && _radioText[pos] != c) {
      _radioText[pos] = c;
      _rtChanged = true;
    }
  }
}
