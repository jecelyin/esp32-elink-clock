#include "RX8010SJ.h"

namespace RX8010SJ {

Adapter::Adapter(uint8_t i2cSlaveAddr) { i2cAddress = i2cSlaveAddr; }

Adapter::~Adapter() {}

/**
 *
 * PUBLIC FUNCTIONS
 *
 */

/**
 * GENERAL
 */

bool Adapter::initAdapter() {
  return initModule();
}

bool Adapter::initModule() {
  uint8_t flagValue = readFromModule(RX8010_FLAG);
  uint8_t vlf = getValueFromBinary(flagValue, RX8010_VLF_POS);

  // It's 1 when the module had power issues and needs to be reinitialised
  if (vlf == 1) {
    // The oscillator takes some time stabilise
    while (vlf == 1) {
      flagValue = setBinary(flagValue, RX8010_VLF_POS, 0);
      writeToModule(RX8010_FLAG, vlf);
      delay(10);
      flagValue = readFromModule(RX8010_FLAG);
      vlf = getValueFromBinary(flagValue, RX8010_VLF_POS);
    }

    resetModule();

    return true;
  }

  return false;
}

void Adapter::resetModule() {
  writeToModule(RX8010_RESV17, RX8010_ADDR17_DEF_VAL);
  writeToModule(RX8010_RESV30, RX8010_ADDR30_DEF_VAL);
  writeToModule(RX8010_RESV31, RX8010_ADDR31_DEF_VAL);
  writeToModule(RX8010_IRQ, RX8010_IRQ_DEF_VAL);

  writeFlag(RX8010_EXT, RX8010_TE_POS, 0);
  writeFlag(RX8010_FLAG, RX8010_VLF_POS, 0);

  writeToModule(RX8010_CTRL, RX8010_CTRL_DEF_VAL);
}

void Adapter::toggleGlobalStop(bool stopEnabled) {
  writeFlag(RX8010_CTRL, RX8010_STOP_POS, stopEnabled ? 1 : 0);
}

/**
 * DATE TIME
 */

DateTime Adapter::readDateTime() {
  uint8_t secondBin = readFromModule(RX8010_SEC);
  uint8_t minuteBin = readFromModule(RX8010_MIN);
  uint8_t hourBin = readFromModule(RX8010_HOUR);
  uint8_t dayOfWeekBin = readFromModule(RX8010_WDAY);
  uint8_t dayOfMonthBin = readFromModule(RX8010_MDAY);
  uint8_t monthBin = readFromModule(RX8010_MONTH);
  uint8_t yearBin = readFromModule(RX8010_YEAR);

  DateTime dateTime;

  dateTime.second = sumValueFromBinary(secondBin, 7);
  dateTime.minute = sumValueFromBinary(minuteBin, 7);
  dateTime.hour = sumValueFromBinary(hourBin, 6);
  dateTime.dayOfWeek = getSingleBit(dayOfWeekBin);
  dateTime.dayOfMonth = sumValueFromBinary(dayOfMonthBin, 6);
  dateTime.month = sumValueFromBinary(monthBin, 5);
  dateTime.year = sumValueFromBinary(yearBin, 8);

  return dateTime;
}

void Adapter::writeDateTime(DateTime dateTime) {
  uint8_t second = dateTime.second % 10;
  uint8_t minute = dateTime.minute % 10;
  uint8_t hour = dateTime.hour % 10;
  uint8_t dayOfWeek = setBinary(0, dateTime.dayOfWeek, 1);
  uint8_t dayOfMonth = dateTime.dayOfMonth % 10;
  uint8_t month = dateTime.month % 10;
  uint8_t year = dateTime.year % 10;

  second = setFortyBinary(second, dateTime.second);
  second = setTwentyBinary(second, dateTime.second);
  second = setTenBinary(second, dateTime.second);

  minute = setFortyBinary(minute, dateTime.minute);
  minute = setTwentyBinary(minute, dateTime.minute);
  minute = setTenBinary(minute, dateTime.minute);

  hour = setTwentyBinary(hour, dateTime.hour);
  hour = setTenBinary(hour, dateTime.hour);

  dayOfMonth = setTwentyBinary(dayOfMonth, dateTime.dayOfMonth);
  dayOfMonth = setTenBinary(dayOfMonth, dateTime.dayOfMonth);

  month = setTenBinary(month, dateTime.month);

  year = setEightyBinary(year, dateTime.year);
  year = setFortyBinary(year, dateTime.year);
  year = setTwentyBinary(year, dateTime.year);
  year = setTenBinary(year, dateTime.year);

  writeToModule(RX8010_SEC, second);
  writeToModule(RX8010_MIN, minute);
  writeToModule(RX8010_HOUR, hour);
  writeToModule(RX8010_WDAY, dayOfWeek);
  writeToModule(RX8010_MDAY, dayOfMonth);
  writeToModule(RX8010_MONTH, month);
  writeToModule(RX8010_YEAR, year);
}

/**
 * FCT
 */

void Adapter::setFCTCounter(uint16_t multiplier, uint8_t frequency) {
  uint8_t firstHalf = multiplier & 0b11111111;
  uint8_t secondHalf = multiplier >> 8;

  writeToModule(RX8010_TCOUNT0, firstHalf);
  writeToModule(RX8010_TCOUNT1, secondHalf);

  writeFlag(RX8010_EXT, RX8010_TSEL0_POS,
            getValueFromBinary(frequency, RX8010_TSEL0_POS));
  writeFlag(RX8010_EXT, RX8010_TSEL1_POS,
            getValueFromBinary(frequency, RX8010_TSEL1_POS));
  writeFlag(RX8010_EXT, RX8010_TSEL2_POS,
            getValueFromBinary(frequency, RX8010_TSEL2_POS));
}

uint16_t Adapter::getFCTCounter() {
  uint8_t firstHalf = readFromModule(RX8010_TCOUNT0);
  uint8_t secondHalf = readFromModule(RX8010_TCOUNT1);

  return firstHalf + (secondHalf << 8);
}

void Adapter::setFCTOutput(uint8_t pin) {
  if (pin > 1) {
    writeFlag(RX8010_CTRL, RX8010_TIE_POS, 0);
  } else {
    writeFlag(RX8010_IRQ, RX8010_TMPIN_POS, pin);
    writeFlag(RX8010_CTRL, RX8010_TIE_POS, 1);
  }
}

void Adapter::enableFCT() {
  writeFlag(RX8010_CTRL, RX8010_TSTP_POS, 0);
  writeFlag(RX8010_CTRL, RX8010_TIE_POS, 1);
  writeFlag(RX8010_EXT, RX8010_TE_POS, 1);
}

void Adapter::disableFCT() {
  writeFlag(RX8010_EXT, RX8010_TE_POS, 0);
  writeFlag(RX8010_CTRL, RX8010_TSTP_POS, 1);
}

bool Adapter::checkFCT() {
  uint8_t flag = readFromModule(RX8010_FLAG);
  bool interrupted = getValueFromBinary(flag, RX8010_TF_POS) == 1;

  if (interrupted) {
    writeFlag(RX8010_FLAG, RX8010_TF_POS, 0);
  }

  return interrupted;
}

/**
 * ALARM
 */

void Adapter::setAlarm(DateTime time, uint8_t mode) {
  uint8_t minute;
  uint8_t hour;

  if (time.minute == RX8010_ALARM_IGNORE) {
    minute = RX8010_AL_DISABLED;
  } else {
    minute = time.minute % 10;
    minute = setFortyBinary(minute, time.minute);
    minute = setTwentyBinary(minute, time.minute);
    minute = setTenBinary(minute, time.minute);
  }

  if (time.hour == RX8010_ALARM_IGNORE) {
    hour = RX8010_AL_DISABLED;
  } else {
    hour = time.hour % 10;
    hour = setTwentyBinary(hour, time.hour);
    hour = setTenBinary(hour, time.hour);
  }

  writeToModule(RX8010_ALMIN, minute);
  writeToModule(RX8010_ALHOUR, hour);

  if (mode == RX8010_ALARM_MOD_WEEK) {
    writeToModule(RX8010_ALWDAY, time.dayOfWeek == RX8010_ALARM_IGNORE
                                     ? RX8010_AL_DISABLED
                                     : time.dayOfWeek);
  } else {
    uint8_t day;

    if (time.hour == RX8010_ALARM_IGNORE) {
      day = RX8010_AL_DISABLED;
    } else {
      day = time.dayOfMonth % 10;
      day = setTwentyBinary(hour, time.hour);
      day = setTenBinary(hour, time.hour);
    }

    writeToModule(RX8010_ALWDAY, day);
  }

  writeFlag(RX8010_EXT, RX8010_WADA_POS, mode == RX8010_ALARM_MOD_WEEK ? 0 : 1);
}

void Adapter::enableAlarm() {
  writeFlag(RX8010_FLAG, RX8010_AF_POS, 0);
  writeFlag(RX8010_CTRL, RX8010_AIE_POS, 1);
}

void Adapter::disableAlarm() {
  writeFlag(RX8010_CTRL, RX8010_AIE_POS, 0);
  writeFlag(RX8010_FLAG, RX8010_AF_POS, 0);
}

bool Adapter::checkAlarm() {
  uint8_t flag = readFromModule(RX8010_FLAG);
  bool triggered = getValueFromBinary(flag, RX8010_AF_POS) == 1;

  if (triggered) {
    writeFlag(RX8010_FLAG, RX8010_AF_POS, 0);
  }

  return triggered;
}

/**
 * TIME UPDATE INTERRUPT
 */

void Adapter::setTUIMode(uint8_t mode) {
  writeFlag(RX8010_EXT, RX8010_USEL_POS, mode);
}

void Adapter::enableTUI() { writeFlag(RX8010_CTRL, RX8010_UIE_POS, 1); }

void Adapter::disableTUI() { writeFlag(RX8010_CTRL, RX8010_UIE_POS, 0); }

bool Adapter::checkTUI() {
  uint8_t flag = readFromModule(RX8010_FLAG);
  bool interrupted = getValueFromBinary(flag, RX8010_UF_POS) == 1;

  if (interrupted) {
    writeFlag(RX8010_FLAG, RX8010_UF_POS, 0);
  }

  return interrupted;
}

/**
 * FREQUENCY OUTPUT
 */

void Adapter::enableFOUT(uint8_t frequency, uint8_t pin) {
  switch (frequency) {
  case 3:
    writeFlag(RX8010_EXT, RX8010_FSEL0_POS, 1);
    writeFlag(RX8010_EXT, RX8010_FSEL1_POS, 1);
    break;
  case 2:
    writeFlag(RX8010_EXT, RX8010_FSEL0_POS, 0);
    writeFlag(RX8010_EXT, RX8010_FSEL1_POS, 1);
    break;
  case 1:
    writeFlag(RX8010_EXT, RX8010_FSEL0_POS, 1);
    writeFlag(RX8010_EXT, RX8010_FSEL1_POS, 0);
    break;
  case 0:
  default:
    disableFOUT();
    return;
  }

  writeFlag(RX8010_IRQ, RX8010_FOPIN0_POS, pin);
  writeFlag(RX8010_IRQ, RX8010_FOPIN1_POS, 0);
}

void Adapter::disableFOUT() {
  writeFlag(RX8010_EXT, RX8010_FSEL0_POS, 0);
  writeFlag(RX8010_EXT, RX8010_FSEL1_POS, 0);
}

/**
 *
 * PRIVATE FUNCTIONS
 *
 */

uint8_t Adapter::readFromModule(uint8_t address) {
  Wire.beginTransmission(i2cAddress);
  Wire.write(address);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)i2cAddress, (uint8_t)1);

  if (Wire.available()) {
    return Wire.read();
  }

  return -1;
}

void Adapter::writeToModule(uint8_t address, uint8_t data) {
  Wire.beginTransmission(i2cAddress);
  Wire.write(address);
  Wire.write(data);
  Wire.endTransmission();
}

void Adapter::writeFlag(uint8_t address, uint8_t pos, uint8_t value) {
  uint8_t addressValue = readFromModule(address);
  addressValue = setBinary(addressValue, pos, value);
  writeToModule(address, addressValue);
}

uint8_t Adapter::getSingleBit(uint8_t binary) {
  for (uint8_t i = 0; i <= 7; i++) {
    if (binary >> i == 1) {
      return i;
    }
  }

  return 0;
}

uint8_t Adapter::getValueFromBinary(uint8_t binary, uint8_t pos) {
  return getValueFromBinary(binary, pos, 1);
}

uint8_t Adapter::getValueFromBinary(uint8_t binary, uint8_t pos, uint8_t val) {
  return ((binary >> pos) & 1) == 1 ? val : 0;
}

uint8_t Adapter::sumValueFromBinary(uint8_t binary, uint8_t length) {
  uint8_t sum = 0;

  for (uint8_t i = 0; i < length; i++) {
    uint8_t value;

    if (i < 4) {
      value = 1 << i;
    } else {
      value = 10 * (1 << (i - 4));
    }

    sum += getValueFromBinary(binary, i, value);
  }

  return sum;
}

uint8_t Adapter::setEightyBinary(uint8_t binary, uint8_t val) {
  if (val >= 80) {
    return setBinary(binary, 7, 1);
  }

  return setBinary(binary, 7, 0);
}

uint8_t Adapter::setFortyBinary(uint8_t binary, uint8_t val) {
  if (val >= 40 && val < 80) {
    return setBinary(binary, 6, 1);
  }

  return setBinary(binary, 6, 0);
}

uint8_t Adapter::setTwentyBinary(uint8_t binary, uint8_t val) {
  if ((val >= 20 && val < 40) || (val >= 60 && val < 80)) {

    return setBinary(binary, 5, 1);
  }

  return setBinary(binary, 5, 0);
}

uint8_t Adapter::setTenBinary(uint8_t binary, uint8_t val) {
  if ((val >= 10 && val < 20) || (val >= 30 && val < 40) ||
      (val >= 50 && val < 60) || (val >= 70 && val < 80) ||
      (val >= 90 && val < 100)) {

    return setBinary(binary, 4, 1);
  }

  return setBinary(binary, 4, 0);
}

uint8_t Adapter::setBinary(uint8_t binary, uint8_t pos, uint8_t flagVal) {
  if (flagVal == 1) {
    return binary | (1 << pos);
  }

  return binary & (~(1 << pos));
}

} // namespace RX8010SJ