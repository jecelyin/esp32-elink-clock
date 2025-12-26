#pragma once

#include <Arduino.h>
#include <Wire.h>

// ----- Type Definitions -----

typedef void (*receiveRDSFunction)(uint16_t block1, uint16_t block2,
                                   uint16_t block3, uint16_t block4);

typedef uint16_t RDA5807M_Freq;

struct RDA5807M_Info {
  bool active;
  uint8_t rssi;
  uint8_t snr;
  bool rds;
  bool tuned;
  bool mono;
  bool stereo;
};

struct RDA5807M_AudioInfo {
  uint8_t volume;
  bool mute;
  bool softmute;
  bool bassBoost;
};

enum RDA5807M_Band {
  RDA5807M_BAND_FM = 0,     // 87-108 MHz
  RDA5807M_BAND_FMWORLD = 1 // 76-108 MHz
};

// ----- RDA5807M Class -----

class RDA5807M {
public:
  RDA5807M();

  bool init();
  void term();

  // ----- Audio features -----
  void setVolume(uint8_t newVolume);
  uint8_t getVolume();

  void setBassBoost(bool switchOn);
  bool getBassBoost();

  void setMono(bool switchOn);
  bool getMono();

  void setMute(bool switchOn);
  bool getMute();

  void setSoftMute(bool switchOn);
  bool getSoftMute();

  // ----- Receiver features -----
  void setBand(RDA5807M_Band newBand);
  void
  setFrequency(uint16_t newF); // Frequency in 10kHz (e.g. 10110 = 101.1 MHz)
  uint16_t getFrequency();

  void seekUp(bool wrap = true);
  void seekDown(bool wrap = true);

  // ----- RDS -----
  void checkRDS();
  void attachReceiveRDS(receiveRDSFunction newFunction);

  // ----- Info -----
  void getRadioInfo(RDA5807M_Info *info);
  void getAudioInfo(RDA5807M_AudioInfo *info);

  // ----- Debug -----
  void debugStatus();
  void debugEnable(bool enable);

private:
  // I2C Addresses
  static const uint8_t I2C_ADDR =
      0x10; // Sequential Access Only per Datasheet 2.5

  // Registers
  uint16_t registers[16]; // Shadow registers

  // Internal State
  uint8_t _volume;
  uint8_t _maxVolume = 15;
  bool _bassBoost;
  bool _mono;
  bool _mute;
  bool _softMute;
  uint16_t _currentFreq;

  bool _debugEnabled;
  receiveRDSFunction _sendRDS = nullptr;

  // Helper functions
  void _readRegisters();
  void _saveRegisters();             // Saves 02-06
  void _saveRegister(uint8_t regNr); // Helper to save up to regNr
  void _write16(uint16_t val);
  uint16_t _read16();

  void int16_to_s(char *s, uint16_t val);
};
