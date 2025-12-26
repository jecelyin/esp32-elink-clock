#include "RDA5807M.h"

// ----- Register Definitions -----

#define RADIO_REG_CHIPID 0x00

#define RADIO_REG_CTRL 0x02
#define RADIO_REG_CTRL_OUTPUT 0x8000
#define RADIO_REG_CTRL_UNMUTE 0x4000
#define RADIO_REG_CTRL_MONO 0x2000
#define RADIO_REG_CTRL_BASS 0x1000
#define RADIO_REG_CTRL_SEEKUP 0x0200 // Bit 9
#define RADIO_REG_CTRL_SEEK 0x0100   // Bit 8
#define RADIO_REG_CTRL_RDS 0x0008    // Bit 3
#define RADIO_REG_CTRL_NEW 0x0010    // Bit 4
#define RADIO_REG_CTRL_RESET 0x0002
#define RADIO_REG_CTRL_ENABLE 0x0001

#define RADIO_REG_CHAN 0x03
#define RADIO_REG_CHAN_SPACE 0x0003
#define RADIO_REG_CHAN_SPACE_100 0x0000
#define RADIO_REG_CHAN_BAND 0x000C
#define RADIO_REG_CHAN_BAND_FM 0x0000
#define RADIO_REG_CHAN_BAND_FMWORLD 0x0008
#define RADIO_REG_CHAN_TUNE 0x0010
#define RADIO_REG_CHAN_NR 0x7FC0

#define RADIO_REG_R4 0x04
#define RADIO_REG_R4_EM50 0x0800
#define RADIO_REG_R4_SOFTMUTE 0x0200
#define RADIO_REG_R4_AFC 0x0100

#define RADIO_REG_VOL 0x05
#define RADIO_REG_VOL_VOL 0x000F

#define RADIO_REG_RA 0x0A
#define RADIO_REG_RA_RDS 0x8000
#define RADIO_REG_RA_RDSBLOCK 0x0800
#define RADIO_REG_RA_STEREO 0x0400
#define RADIO_REG_RA_NR 0x03FF

#define RADIO_REG_RB 0x0B
#define RADIO_REG_RB_FMTRUE 0x0100
#define RADIO_REG_RB_FMREADY 0x0080

#define RADIO_REG_RDSA 0x0C
#define RADIO_REG_RDSB 0x0D
#define RADIO_REG_RDSC 0x0E
#define RADIO_REG_RDSD 0x0F

RDA5807M::RDA5807M() {
  _volume = 1;
  _maxVolume = 15;
  _bassBoost = false;
  _mono = false;
  _mute = false;
  _softMute = false;
  _debugEnabled = false;
  _currentFreq = 8790; // Default

  // Initialize shadow registers
  for (int i = 0; i < 16; i++) {
    registers[i] = 0;
  }
}

bool RDA5807M::init() {
  // 1. Initialize Register Cache with defaults
  registers[RADIO_REG_CHIPID] = 0x5804;
  registers[1] = 0x0000;

  // Reset enabled, Enable=1
  registers[RADIO_REG_CTRL] = (RADIO_REG_CTRL_RESET | RADIO_REG_CTRL_ENABLE);

  registers[RADIO_REG_CHAN] = 0x0000; // Default band (FM), Space 100kHz
  registers[RADIO_REG_R4] = RADIO_REG_R4_EM50; // De-emphasis 50us

  // Volume defaults (simulating old driver's 0x9081)
  registers[RADIO_REG_VOL] = 0x9081;

  registers[6] = 0x0000;
  registers[7] = 0x0000;

  // 2. Reset the chip (Sequential Write 02h-06h)
  _saveRegisters(); // Writes 02,03,04,05,06

  delay(100);

  // 3. Enable normal operation (Sequential Write again without Reset bit)
  registers[RADIO_REG_CTRL] = RADIO_REG_CTRL_ENABLE | RADIO_REG_CTRL_OUTPUT |
                              RADIO_REG_CTRL_RDS | RADIO_REG_CTRL_NEW;
  // Keep defaults for others
  _saveRegisters(); // Writes 02,03,04,05,06

  return true;
}

void RDA5807M::term() {
  setVolume(0);
  registers[RADIO_REG_CTRL] = 0x0000; // Disable
  _saveRegisters();
}

// ----- Audio -----

void RDA5807M::setVolume(uint8_t newVolume) {
  if (newVolume > _maxVolume)
    newVolume = _maxVolume;
  _volume = newVolume;

  registers[RADIO_REG_VOL] &= ~RADIO_REG_VOL_VOL;
  registers[RADIO_REG_VOL] |= (_volume & RADIO_REG_VOL_VOL);

  _saveRegister(RADIO_REG_VOL);
}

uint8_t RDA5807M::getVolume() { return _volume; }

void RDA5807M::setBassBoost(bool switchOn) {
  _bassBoost = switchOn;
  if (switchOn)
    registers[RADIO_REG_CTRL] |= RADIO_REG_CTRL_BASS;
  else
    registers[RADIO_REG_CTRL] &= ~RADIO_REG_CTRL_BASS;
  _saveRegister(RADIO_REG_CTRL);
}

bool RDA5807M::getBassBoost() { return _bassBoost; }

void RDA5807M::setMono(bool switchOn) {
  _mono = switchOn;
  if (switchOn)
    registers[RADIO_REG_CTRL] |= RADIO_REG_CTRL_MONO;
  else
    registers[RADIO_REG_CTRL] &= ~RADIO_REG_CTRL_MONO;
  _saveRegister(RADIO_REG_CTRL);
}

bool RDA5807M::getMono() { return _mono; }

void RDA5807M::setMute(bool switchOn) {
  _mute = switchOn;
  if (switchOn)
    registers[RADIO_REG_CTRL] &= ~RADIO_REG_CTRL_UNMUTE;
  else
    registers[RADIO_REG_CTRL] |= RADIO_REG_CTRL_UNMUTE;
  _saveRegister(RADIO_REG_CTRL);
}

bool RDA5807M::getMute() { return _mute; }

void RDA5807M::setSoftMute(bool switchOn) {
  _softMute = switchOn;
  if (switchOn)
    registers[RADIO_REG_R4] |= RADIO_REG_R4_SOFTMUTE;
  else
    registers[RADIO_REG_R4] &= ~RADIO_REG_R4_SOFTMUTE;
  _saveRegister(RADIO_REG_R4);
}

bool RDA5807M::getSoftMute() { return _softMute; }

// ----- Receiver -----

void RDA5807M::setBand(RDA5807M_Band newBand) {
  uint16_t r = RADIO_REG_CHAN_BAND_FM;
  if (newBand == RDA5807M_BAND_FMWORLD) {
    r = RADIO_REG_CHAN_BAND_FMWORLD;
  }

  registers[RADIO_REG_CHAN] &= ~RADIO_REG_CHAN_BAND;
  registers[RADIO_REG_CHAN] |= r;
  registers[RADIO_REG_CHAN] |= RADIO_REG_CHAN_SPACE_100;

  _saveRegister(RADIO_REG_CHAN);
}

void RDA5807M::setFrequency(uint16_t newF) {
  _currentFreq = newF;

  // Calculate chan
  uint16_t spacing = 10;  // 100kHz
  uint16_t bottom = 8700; // 87.0 MHz

  // Adjust bottom based on band
  if ((registers[RADIO_REG_CHAN] & RADIO_REG_CHAN_BAND) ==
      RADIO_REG_CHAN_BAND_FMWORLD) {
    bottom = 7600;
  }

  if (newF < bottom)
    newF = bottom;

  uint16_t channel = (newF - bottom) / spacing;

  registers[RADIO_REG_CHAN] &= ~(RADIO_REG_CHAN_TUNE | RADIO_REG_CHAN_NR);
  registers[RADIO_REG_CHAN] |= RADIO_REG_CHAN_TUNE;
  registers[RADIO_REG_CHAN] |= (channel << 6);

  _saveRegister(RADIO_REG_CHAN);
}

uint16_t RDA5807M::getFrequency() {
  _readRegisters();
  uint16_t channel = registers[RADIO_REG_RA] & RADIO_REG_RA_NR;
  uint16_t spacing = 10;
  uint16_t bottom = 8700;
  if ((registers[RADIO_REG_CHAN] & RADIO_REG_CHAN_BAND) ==
      RADIO_REG_CHAN_BAND_FMWORLD) {
    bottom = 7600;
  }
  return bottom + (channel * spacing);
}

void RDA5807M::seekUp(bool wrap) {
  registers[RADIO_REG_CTRL] |= RADIO_REG_CTRL_SEEKUP;
  registers[RADIO_REG_CTRL] |= RADIO_REG_CTRL_SEEK;
  _saveRegister(RADIO_REG_CTRL);

  registers[RADIO_REG_CTRL] &= ~RADIO_REG_CTRL_SEEK;
}

void RDA5807M::seekDown(bool wrap) {
  registers[RADIO_REG_CTRL] &= ~RADIO_REG_CTRL_SEEKUP;
  registers[RADIO_REG_CTRL] |= RADIO_REG_CTRL_SEEK;
  _saveRegister(RADIO_REG_CTRL);
  registers[RADIO_REG_CTRL] &= ~RADIO_REG_CTRL_SEEK;
}

// ----- RDS -----

void RDA5807M::checkRDS() {
  _readRegisters();
  if (_sendRDS) {
    // Check if RDS data is available (Bit 15 of Register 0Ah)
    if (registers[RADIO_REG_RA] & RADIO_REG_RA_RDS) {
      _sendRDS(registers[RADIO_REG_RDSA], registers[RADIO_REG_RDSB],
               registers[RADIO_REG_RDSC], registers[RADIO_REG_RDSD]);
    }
  }
}

void RDA5807M::attachReceiveRDS(receiveRDSFunction newFunction) {
  _sendRDS = newFunction;
}

// ----- Info -----

void RDA5807M::getRadioInfo(RDA5807M_Info *info) {
  _readRegisters();
  info->active = true;
  info->rssi = registers[RADIO_REG_RB] >> 10;
  info->tuned = (registers[RADIO_REG_RB] & RADIO_REG_RB_FMTRUE) != 0;
  info->stereo = (registers[RADIO_REG_RA] & RADIO_REG_RA_STEREO) != 0;
  info->rds = (registers[RADIO_REG_RA] & RADIO_REG_RA_RDS) != 0;
  info->mono = (registers[RADIO_REG_CTRL] & RADIO_REG_CTRL_MONO) != 0;
  info->snr = 0;
}

void RDA5807M::getAudioInfo(RDA5807M_AudioInfo *info) {
  info->volume = _volume;
  info->mute = _mute;
  info->softmute = _softMute;
  info->bassBoost = _bassBoost;
}

// ----- Debug -----

void RDA5807M::debugEnable(bool enable) { _debugEnabled = enable; }

void RDA5807M::debugStatus() {
  if (!_debugEnabled)
    return;

  _readRegisters();

  Serial.print("Freq: ");
  Serial.print(getFrequency());
  Serial.print(" RSSI: ");
  Serial.print(registers[RADIO_REG_RB] >> 10);
  Serial.print(" Stereo: ");
  Serial.print((registers[RADIO_REG_RA] & RADIO_REG_RA_STEREO) ? "Y" : "N");
  Serial.println();

  for (int i = 0; i < 16; i++) {
    Serial.printf("Reg[%02X]: %04X\n", i, registers[i]);
  }
}

// ----- Internal I2C -----

void RDA5807M::_readRegisters() {
  // Read from I2C Address 0x10 starts at Register 0x0A (Datasheet 2.5)
  // We read 6 words (12 bytes) to cover 0x0A to 0x0F
  Wire.requestFrom(I2C_ADDR, (uint8_t)12);

  if (Wire.available() >= 12) {
    for (int i = 0; i < 6; i++) {
      registers[0x0A + i] = _read16();
    }
  }
}

void RDA5807M::_saveRegisters() {
  // Write to I2C Address 0x10 starts at Register 0x02 (Datasheet 2.5)
  // We want to save 0x02 to 0x06 (5 words) ideally.
  // Datasheet says we can stop anytime by STOP condition.

  Wire.beginTransmission(I2C_ADDR);
  for (int i = 2; i <= 6; i++) {
    _write16(registers[i]);
  }
  Wire.endTransmission();
}

void RDA5807M::_saveRegister(uint8_t regNr) {
  // We cannot do Random Access (0x11) strictly per 2.5.
  // We must do Sequential Write from 0x02 up to regNr.

  // Ensure we are only saving writable regs 0x02 to 0x07 (or 3A)
  // We only care about up to 0x07 (Data Sheet mentions only up to 0x5...0x7
  // mainly for control) If regNr < 2 or > 6, maybe we shouldn't be here, but
  // let's handle 2-6.

  if (regNr < 0x02)
    return;

  Wire.beginTransmission(I2C_ADDR);
  for (int i = 0x02; i <= regNr; i++) {
    _write16(registers[i]);
  }
  Wire.endTransmission();
}

void RDA5807M::_write16(uint16_t val) {
  Wire.write(val >> 8);
  Wire.write(val & 0xFF);
}

uint16_t RDA5807M::_read16() {
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  return (hi << 8) | lo;
}

void RDA5807M::int16_to_s(char *s, uint16_t val) { sprintf(s, "%d", val); }
