#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

class SimpleQRCode {
public:
  static constexpr uint8_t MATRIX_SIZE = 29;

  bool encodeText(const char *text) {
    if (text == nullptr)
      return false;

    size_t length = strlen(text);
    if (length > MAX_INPUT_BYTES)
      return false;

    clear();
    initGaloisField();
    addFunctionPatterns();

    uint8_t data[DATA_CODEWORDS];
    uint8_t ecc[ECC_CODEWORDS];
    uint8_t codewords[TOTAL_CODEWORDS];
    if (!buildDataCodewords(text, length, data))
      return false;

    buildErrorCorrection(data, ecc);
    memcpy(codewords, data, DATA_CODEWORDS);
    memcpy(codewords + DATA_CODEWORDS, ecc, ECC_CODEWORDS);
    placeDataBits(codewords, TOTAL_CODEWORDS * 8);
    drawFormatBits(MASK_PATTERN);
    return true;
  }

  uint8_t size() const { return MATRIX_SIZE; }

  bool isDark(uint8_t x, uint8_t y) const {
    if (x >= MATRIX_SIZE || y >= MATRIX_SIZE)
      return false;
    return modules[y][x] != 0;
  }

private:
  // 关键逻辑：当前配网 payload 为 37 字节，QR Version 3-L 的字节容量
  // 为 53 字节，能容纳 WPA WiFi 字符串并保持矩阵不至于过密。
  static constexpr uint8_t VERSION = 3;
  static constexpr uint8_t DATA_CODEWORDS = 55;
  static constexpr uint8_t ECC_CODEWORDS = 15;
  static constexpr uint8_t TOTAL_CODEWORDS = DATA_CODEWORDS + ECC_CODEWORDS;
  static constexpr uint8_t MAX_INPUT_BYTES = 53;
  static constexpr uint8_t MASK_PATTERN = 0;
  static constexpr uint16_t FORMAT_POLY = 0x537;
  static constexpr uint16_t FORMAT_MASK = 0x5412;
  static constexpr uint8_t ECC_FORMAT_BITS_L = 1;

  struct BitBuffer {
    uint8_t bytes[DATA_CODEWORDS];
    int bitLength;
  };

  uint8_t modules[MATRIX_SIZE][MATRIX_SIZE];
  uint8_t reserved[MATRIX_SIZE][MATRIX_SIZE];
  uint8_t gfExp[512];
  uint8_t gfLog[256];

  void clear() {
    memset(modules, 0, sizeof(modules));
    memset(reserved, 0, sizeof(reserved));
    memset(gfExp, 0, sizeof(gfExp));
    memset(gfLog, 0, sizeof(gfLog));
  }

  void initGaloisField() {
    // 关键逻辑：QR Reed-Solomon 使用 GF(256) 和 0x11D 本原多项式；
    // 预生成指数/对数表后，纠错码计算只需要查表乘法。
    uint16_t value = 1;
    for (int i = 0; i < 255; ++i) {
      gfExp[i] = static_cast<uint8_t>(value);
      gfLog[value] = static_cast<uint8_t>(i);
      value <<= 1;
      if ((value & 0x100) != 0)
        value ^= 0x11D;
    }

    for (int i = 255; i < 512; ++i) {
      gfExp[i] = gfExp[i - 255];
    }
  }

  bool buildDataCodewords(const char *text, size_t length, uint8_t *data) {
    BitBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));

    if (!appendBits(buffer, 0x4, 4))
      return false;
    if (!appendBits(buffer, static_cast<uint16_t>(length), 8))
      return false;

    for (size_t i = 0; i < length; ++i) {
      if (!appendBits(buffer, static_cast<uint8_t>(text[i]), 8))
        return false;
    }

    finishDataCodewords(buffer);
    memcpy(data, buffer.bytes, DATA_CODEWORDS);
    return true;
  }

  bool appendBits(BitBuffer &buffer, uint16_t value, uint8_t count) {
    if (buffer.bitLength + count > DATA_CODEWORDS * 8)
      return false;

    for (int i = count - 1; i >= 0; --i) {
      if (((value >> i) & 1U) != 0)
        setBufferBit(buffer, buffer.bitLength);
      buffer.bitLength++;
    }
    return true;
  }

  void setBufferBit(BitBuffer &buffer, int bitIndex) {
    int byteIndex = bitIndex >> 3;
    uint8_t bitMask = static_cast<uint8_t>(0x80 >> (bitIndex & 7));
    buffer.bytes[byteIndex] |= bitMask;
  }

  void finishDataCodewords(BitBuffer &buffer) {
    // 关键逻辑：QR 数据区必须先写 terminator、补齐字节边界，再用
    // 0xEC/0x11 交替填满剩余 codeword。
    int capacityBits = DATA_CODEWORDS * 8;
    int terminatorBits = capacityBits - buffer.bitLength;
    buffer.bitLength += terminatorBits < 4 ? terminatorBits : 4;

    while ((buffer.bitLength & 7) != 0) {
      buffer.bitLength++;
    }
    appendPadCodewords(buffer, capacityBits);
  }

  void appendPadCodewords(BitBuffer &buffer, int capacityBits) {
    uint8_t pad = 0xEC;
    while (buffer.bitLength < capacityBits) {
      buffer.bytes[buffer.bitLength >> 3] = pad;
      buffer.bitLength += 8;
      pad = pad == 0xEC ? 0x11 : 0xEC;
    }
  }

  void buildErrorCorrection(const uint8_t *data, uint8_t *ecc) {
    // 关键逻辑：Version 3-L 只有一个 RS block，因此数据码字后面
    // 直接追加 15 个纠错码字，不需要做跨 block 交织。
    uint8_t generator[ECC_CODEWORDS];
    buildGenerator(generator);
    memset(ecc, 0, ECC_CODEWORDS);

    for (int i = 0; i < DATA_CODEWORDS; ++i) {
      uint8_t factor = data[i] ^ ecc[0];
      shiftEcc(ecc);
      applyGenerator(ecc, generator, factor);
    }
  }

  void buildGenerator(uint8_t *generator) {
    memset(generator, 0, ECC_CODEWORDS);
    generator[ECC_CODEWORDS - 1] = 1;
    uint8_t root = 1;

    for (int i = 0; i < ECC_CODEWORDS; ++i) {
      multiplyGeneratorByRoot(generator, root);
      root = gfMultiply(root, 2);
    }
  }

  void multiplyGeneratorByRoot(uint8_t *generator, uint8_t root) {
    for (int i = 0; i < ECC_CODEWORDS; ++i) {
      generator[i] = gfMultiply(generator[i], root);
      if (i + 1 < ECC_CODEWORDS)
        generator[i] ^= generator[i + 1];
    }
  }

  void shiftEcc(uint8_t *ecc) {
    memmove(ecc, ecc + 1, ECC_CODEWORDS - 1);
    ecc[ECC_CODEWORDS - 1] = 0;
  }

  void applyGenerator(uint8_t *ecc, const uint8_t *generator, uint8_t factor) {
    for (int i = 0; i < ECC_CODEWORDS; ++i) {
      ecc[i] ^= gfMultiply(generator[i], factor);
    }
  }

  uint8_t gfMultiply(uint8_t left, uint8_t right) const {
    if (left == 0 || right == 0)
      return 0;
    return gfExp[gfLog[left] + gfLog[right]];
  }

  void addFunctionPatterns() {
    // 关键逻辑：功能图形必须先占位，后续数据 zig-zag 写入时要跳过
    // finder、timing、alignment、format 和 dark module。
    drawFinderPattern(0, 0);
    drawFinderPattern(MATRIX_SIZE - 7, 0);
    drawFinderPattern(0, MATRIX_SIZE - 7);
    drawTimingPatterns();
    drawAlignmentPattern(22, 22);
    setFunctionModule(8, MATRIX_SIZE - 8, true);
    reserveFormatAreas();
  }

  void drawFinderPattern(int left, int top) {
    for (int dy = -1; dy <= 7; ++dy) {
      for (int dx = -1; dx <= 7; ++dx) {
        int x = left + dx;
        int y = top + dy;
        if (!isInBounds(x, y))
          continue;
        bool dark = isFinderDark(dx, dy);
        setFunctionModule(x, y, dark);
      }
    }
  }

  bool isFinderDark(int dx, int dy) const {
    bool inFinder = dx >= 0 && dx <= 6 && dy >= 0 && dy <= 6;
    bool outer = dx == 0 || dx == 6 || dy == 0 || dy == 6;
    bool center = dx >= 2 && dx <= 4 && dy >= 2 && dy <= 4;
    return inFinder && (outer || center);
  }

  void drawTimingPatterns() {
    for (int i = 8; i < MATRIX_SIZE - 8; ++i) {
      bool dark = (i & 1) == 0;
      setFunctionModule(6, i, dark);
      setFunctionModule(i, 6, dark);
    }
  }

  void drawAlignmentPattern(int centerX, int centerY) {
    for (int dy = -2; dy <= 2; ++dy) {
      for (int dx = -2; dx <= 2; ++dx) {
        int distanceX = dx < 0 ? -dx : dx;
        int distanceY = dy < 0 ? -dy : dy;
        bool dark = distanceX == 2 || distanceY == 2 || (dx == 0 && dy == 0);
        setFunctionModule(centerX + dx, centerY + dy, dark);
      }
    }
  }

  void reserveFormatAreas() {
    for (int i = 0; i <= 5; ++i) {
      reserveModule(8, i);
      reserveModule(i, 8);
    }
    reserveModule(8, 7);
    reserveModule(8, 8);
    reserveModule(7, 8);
    reserveSecondFormatCopy();
  }

  void reserveSecondFormatCopy() {
    for (int i = 0; i <= 7; ++i) {
      reserveModule(MATRIX_SIZE - 1 - i, 8);
    }
    for (int i = 8; i <= 14; ++i) {
      reserveModule(8, MATRIX_SIZE - 15 + i);
    }
  }

  void placeDataBits(const uint8_t *data, int bitCount) {
    // 关键逻辑：QR 数据按从右下开始的双列蛇形路径写入，遇到第 6 列
    // timing pattern 时需要整体跳过。
    int bitIndex = 0;
    for (int right = MATRIX_SIZE - 1; right >= 1; right -= 2) {
      if (right == 6)
        right = 5;
      placeDataColumnPair(data, bitCount, bitIndex, right);
    }
  }

  void placeDataColumnPair(const uint8_t *data, int bitCount, int &bitIndex,
                           int right) {
    bool upward = ((right + 1) & 2) == 0;
    for (int vert = 0; vert < MATRIX_SIZE; ++vert) {
      int y = upward ? MATRIX_SIZE - 1 - vert : vert;
      placeDataPairAt(data, bitCount, bitIndex, right, y);
    }
  }

  void placeDataPairAt(const uint8_t *data, int bitCount, int &bitIndex,
                       int right, int y) {
    for (int j = 0; j < 2; ++j) {
      int x = right - j;
      if (reserved[y][x] != 0)
        continue;
      bool dark = bitIndex < bitCount && getDataBit(data, bitIndex);
      modules[y][x] = (dark ^ shouldMask(x, y)) ? 1 : 0;
      bitIndex++;
    }
  }

  bool getDataBit(const uint8_t *data, int bitIndex) const {
    uint8_t value = data[bitIndex >> 3];
    return ((value >> (7 - (bitIndex & 7))) & 1U) != 0;
  }

  bool shouldMask(int x, int y) const { return ((x + y) & 1) == 0; }

  void drawFormatBits(uint8_t mask) {
    // 关键逻辑：format bits 同时声明纠错等级和掩膜编号；扫描器会按
    // 这里的 mask 信息反掩膜，所以必须在数据写入后覆盖两份格式区。
    uint16_t bits = getFormatBits(mask);
    for (int i = 0; i <= 5; ++i) {
      setFunctionModule(8, i, getFormatBit(bits, i));
      setFunctionModule(i, 8, getFormatBit(bits, 14 - i));
    }
    drawRemainingFormatBits(bits);
  }

  void drawRemainingFormatBits(uint16_t bits) {
    setFunctionModule(8, 7, getFormatBit(bits, 6));
    setFunctionModule(8, 8, getFormatBit(bits, 7));
    setFunctionModule(7, 8, getFormatBit(bits, 8));
    for (int i = 0; i <= 7; ++i) {
      setFunctionModule(MATRIX_SIZE - 1 - i, 8, getFormatBit(bits, i));
    }
    drawBottomLeftFormatBits(bits);
    setFunctionModule(8, MATRIX_SIZE - 8, true);
  }

  void drawBottomLeftFormatBits(uint16_t bits) {
    for (int i = 8; i <= 14; ++i) {
      setFunctionModule(8, MATRIX_SIZE - 15 + i, getFormatBit(bits, i));
    }
  }

  uint16_t getFormatBits(uint8_t mask) const {
    uint16_t data = static_cast<uint16_t>((ECC_FORMAT_BITS_L << 3) | mask);
    uint16_t bits = static_cast<uint16_t>(data << 10);
    for (int i = 14; i >= 10; --i) {
      if (((bits >> i) & 1U) != 0)
        bits ^= static_cast<uint16_t>(FORMAT_POLY << (i - 10));
    }
    return static_cast<uint16_t>(((data << 10) | bits) ^ FORMAT_MASK);
  }

  bool getFormatBit(uint16_t bits, uint8_t index) const {
    return ((bits >> index) & 1U) != 0;
  }

  bool isInBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < MATRIX_SIZE && y < MATRIX_SIZE;
  }

  void setFunctionModule(int x, int y, bool dark) {
    if (!isInBounds(x, y))
      return;
    modules[y][x] = dark ? 1 : 0;
    reserved[y][x] = 1;
  }

  void reserveModule(int x, int y) {
    if (isInBounds(x, y))
      reserved[y][x] = 1;
  }
};
