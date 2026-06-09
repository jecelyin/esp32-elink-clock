#include "RDA5807M.h"
#include <esp_log.h>

// ----- Register Definitions -----
#define I2C_ADDR_SEQ 0x10
#define I2C_ADDR_IDX 0x11

#define REG_CHIP_ID 0x00
#define REG_CTRL 0x02
#define REG_CHAN 0x03
#define REG_R4 0x04
#define REG_VOL 0x05
#define REG_RA 0x0a
#define REG_RB 0x0b
#define REG_RDSA 0x0c
#define REG_RDSB 0x0d
#define REG_RDSC 0x0e
#define REG_RDSD 0x0f

// Control Bits
#define BIT_CTRL_ENABLE 0
#define BIT_CTRL_SOFT_RESET 1
#define BIT_CTRL_RDS_EN 3
#define BIT_CTRL_CLK_MODE 4
#define BIT_CTRL_SEEK 8
#define BIT_CTRL_SEEKUP 9
#define BIT_CTRL_BASS 12
#define BIT_CTRL_MONO 13
#define BIT_CTRL_DMUTE 14
#define BIT_CTRL_DHIZ 15

#define BIT_CHAN_SPACE 0
#define BIT_CHAN_BAND 2
#define BIT_CHAN_TUNE 4
#define BIT_CHAN_CHAN 6

#define BIT_R4_DE 11
#define BIT_R4_SOFTMUTE_EN 9

#define BIT_VOL_VOLUME 0
#define BIT_VOL_SEEKTH 8

#define BIT_RA_STC 14
#define MASK_RA_READCHAN 0x03FF
#define BIT_RB_RSSI 9

#define BV(x) (1 << (x))

static const char *TAG = "RDA5807M";

RDA5807M::RDA5807M() {
  _volume = 10;
  _maxVolume = 15;
  _bassBoost = false;
  _mono = false;
  _mute = false;
  _softMute = false;
  _debugEnabled = false;
  _currentFreq = 8790;

  for (int i = 0; i < 16; i++) {
    registers[i] = 0;
  }
}

bool RDA5807M::init() {

  // 1. Check ID using Index Access (0x11)
  uint16_t chipId = _readReg(REG_CHIP_ID);
  if (chipId == 0 || chipId == 0xFFFF) {
    ESP_LOGE(TAG, "Radio not found on 0x11 (ID: %04X).", chipId);
    return false;
  }
  ESP_LOGI(TAG, "Chip ID: %04X", chipId);

  // 2. Soft Reset
  _writeReg(REG_CTRL, BV(BIT_CTRL_SOFT_RESET) | BV(BIT_CTRL_ENABLE));
  delay(50);

  // 3. Complete Initialization (Synced with rda5807.c)
  // Enable: DHIZ (Audio Output), DMUTE (Unmute), RDS, ENABLE
  registers[REG_CTRL] = BV(BIT_CTRL_DHIZ) | BV(BIT_CTRL_DMUTE) |
                        BV(BIT_CTRL_RDS_EN) | BV(BIT_CTRL_ENABLE);
  _writeReg(REG_CTRL, registers[REG_CTRL]);

  // De-emphasis = 50us
  registers[REG_R4] = BV(BIT_R4_DE);
  _writeReg(REG_R4, registers[REG_R4]);

  // Set Volume, Seek threshold = ~32 dB SNR, INT_MODE = 1
  registers[REG_VOL] = 0x888a;
  _writeReg(REG_VOL, registers[REG_VOL]);
  setVolume(_volume);

  ESP_LOGI(TAG, "Initialized via 0x11.");
  return true;
}

void RDA5807M::term() {
  setVolume(0);
  _writeReg(REG_CTRL, 0);
}

// ----- Audio -----

void RDA5807M::setVolume(uint8_t newVolume) {
  if (newVolume > _maxVolume)
    newVolume = _maxVolume;
  _volume = newVolume;

  registers[REG_VOL] &= ~(0x0F << BIT_VOL_VOLUME);
  registers[REG_VOL] |= (_volume << BIT_VOL_VOLUME);
  _writeReg(REG_VOL, registers[REG_VOL]);
}

uint8_t RDA5807M::getVolume() { return _volume; }

void RDA5807M::setBassBoost(bool switchOn) {
  _bassBoost = switchOn;
  if (switchOn)
    registers[REG_CTRL] |= BV(BIT_CTRL_BASS);
  else
    registers[REG_CTRL] &= ~BV(BIT_CTRL_BASS);
  _writeReg(REG_CTRL, registers[REG_CTRL]);
}

bool RDA5807M::getBassBoost() { return _bassBoost; }

void RDA5807M::setMono(bool switchOn) {
  _mono = switchOn;
  if (switchOn)
    registers[REG_CTRL] |= BV(BIT_CTRL_MONO);
  else
    registers[REG_CTRL] &= ~BV(BIT_CTRL_MONO);
  _writeReg(REG_CTRL, registers[REG_CTRL]);
}

bool RDA5807M::getMono() { return _mono; }

void RDA5807M::setMute(bool switchOn) {
  _mute = switchOn;
  if (switchOn)
    registers[REG_CTRL] &= ~BV(BIT_CTRL_DMUTE);
  else
    registers[REG_CTRL] |= BV(BIT_CTRL_DMUTE);
  _writeReg(REG_CTRL, registers[REG_CTRL]);
}

bool RDA5807M::getMute() { return _mute; }

void RDA5807M::setSoftMute(bool switchOn) {
  _softMute = switchOn;
  if (switchOn)
    registers[REG_R4] |= BV(BIT_R4_SOFTMUTE_EN);
  else
    registers[REG_R4] &= ~BV(BIT_R4_SOFTMUTE_EN);
  _writeReg(REG_R4, registers[REG_R4]);
}

bool RDA5807M::getSoftMute() { return _softMute; }

// ----- Receiver -----

void RDA5807M::setBand(RDA5807M_Band newBand) {
  uint16_t b = (newBand == RDA5807M_BAND_FMWORLD) ? 1 : 0;
  registers[REG_CHAN] &= ~(0x03 << BIT_CHAN_BAND);
  registers[REG_CHAN] |= (b << BIT_CHAN_BAND);
  _writeReg(REG_CHAN, registers[REG_CHAN]);
}

void RDA5807M::setFrequency(uint16_t newF) {
  uint16_t bottom = 8700;
  if (((registers[REG_CHAN] >> BIT_CHAN_BAND) & 0x03) == 1) {
    bottom = 7600;
  }

  uint16_t channel = (newF - bottom) / 10;

  registers[REG_CHAN] &= ~(0x03FF << BIT_CHAN_CHAN);
  registers[REG_CHAN] |= (channel << BIT_CHAN_CHAN);
  registers[REG_CHAN] |= BV(BIT_CHAN_TUNE);

  _writeReg(REG_CHAN, registers[REG_CHAN]);

  // Wait for Tune Complete (STC bit in RA)
  int retry = 50;
  while (retry--) {
    uint16_t status = _readReg(REG_RA);
    if (status & BV(BIT_RA_STC))
      break;
    delay(10);
  }
}

uint16_t RDA5807M::getFrequency() {
  uint16_t ra = _readReg(REG_RA);
  uint16_t channel = ra & MASK_RA_READCHAN;
  uint16_t bottom = 8700;
  if (((registers[REG_CHAN] >> BIT_CHAN_BAND) & 0x03) == 1) {
    bottom = 7600;
  }
  return bottom + (channel * 10);
}

void RDA5807M::seekUp(bool wrap) {
  registers[REG_CTRL] |= BV(BIT_CTRL_SEEKUP);
  registers[REG_CTRL] |= BV(BIT_CTRL_SEEK);
  _writeReg(REG_CTRL, registers[REG_CTRL]);
  registers[REG_CTRL] &= ~BV(BIT_CTRL_SEEK);
  _writeReg(REG_CTRL, registers[REG_CTRL]);
}

void RDA5807M::seekDown(bool wrap) {
  registers[REG_CTRL] &= ~BV(BIT_CTRL_SEEKUP);
  registers[REG_CTRL] |= BV(BIT_CTRL_SEEK);
  _writeReg(REG_CTRL, registers[REG_CTRL]);
  registers[REG_CTRL] &= ~BV(BIT_CTRL_SEEK);
  _writeReg(REG_CTRL, registers[REG_CTRL]);
}

// ----- RDS -----

void RDA5807M::checkRDS() {
  if (_sendRDS) {
    uint16_t ra = _readReg(REG_RA);
    if (ra & (1 << 15)) {
      uint16_t blocks[4];
      blocks[0] = _readReg(REG_RDSA);
      blocks[1] = _readReg(REG_RDSB);
      blocks[2] = _readReg(REG_RDSC);
      blocks[3] = _readReg(REG_RDSD);
      _sendRDS(blocks[0], blocks[1], blocks[2], blocks[3]);
    }
  }
}

void RDA5807M::attachReceiveRDS(receiveRDSFunction newFunction) {
  _sendRDS = newFunction;
}

// ----- Info -----

void RDA5807M::getRadioInfo(RDA5807M_Info *info) {
  uint16_t ra = _readReg(REG_RA);
  uint16_t rb = _readReg(REG_RB);

  info->active = true;
  info->rssi = rb >> BIT_RB_RSSI;
  info->tuned = (ra & BV(BIT_RA_STC)) != 0;
  info->stereo = (ra & (1 << 10)) != 0;
  info->rds = (ra & (1 << 15)) != 0;
  info->mono = (registers[REG_CTRL] & BV(BIT_CTRL_MONO)) != 0;
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
  ESP_LOGI(TAG, "Freq: %d, Vol: %d", getFrequency(), _volume);
}

// ----- Internal I2C -----

void RDA5807M::_readRegisters() {
  for (int i = 0; i < 6; i++) {
    registers[0x0A + i] = _readReg(0x0A + i);
  }
}

void RDA5807M::_saveRegisters() {
  for (int i = 2; i <= 6; i++) {
    _writeReg(i, registers[i]);
  }
}

void RDA5807M::_saveRegister(uint8_t regNr) {
  _writeReg(regNr, registers[regNr]);
}

void RDA5807M::_writeReg(uint8_t reg, uint16_t val) {
  Wire.beginTransmission(I2C_ADDR_IDX);
  Wire.write(reg);
  _write16(val);
  Wire.endTransmission();
}

uint16_t RDA5807M::_readReg(uint8_t reg) {
  Wire.beginTransmission(I2C_ADDR_IDX);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0)
    return 0;
  Wire.requestFrom(I2C_ADDR_IDX, (uint8_t)2);
  return _read16();
}

void RDA5807M::_write16(uint16_t val) {
  Wire.write(val >> 8);
  Wire.write(val & 0xFF);
}

uint16_t RDA5807M::_read16() {
  if (Wire.available() < 2)
    return 0;
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  return (hi << 8) | lo;
}

void RDA5807M::int16_to_s(char *s, uint16_t val) { sprintf(s, "%d", val); }
