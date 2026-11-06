#pragma once

#include "AlarmVolumeRamp.h"

class AlarmManager;
class ConfigManager;
class AudioDriver;
class AlarmPcmPlayer;
class SDCardDriver;
class String;

// 统一管理两条闹钟播放路径及其生命周期，主循环只负责定期调用 service。
class AlarmPlayback {
public:
  AlarmPlayback(AlarmManager *alarms, ConfigManager *config, AudioDriver *audio,
                AlarmPcmPlayer *pcm, SDCardDriver *sd, void (*restoreRadio)());
  void service();
  void stop();

private:
  void acquire(uint32_t sequence);
  void applyVolume();
  bool playConfiguredAlarm();
  bool playFromFlash(const String &path);
  bool prepareCodec();

  AlarmManager *alarms;
  ConfigManager *config;
  AudioDriver *audio;
  AlarmPcmPlayer *pcm;
  SDCardDriver *sd;
  void (*restoreRadio)();
  AlarmVolumeRamp ramp;
  uint32_t lastAttemptMs = 0;
  uint32_t triggerSequence = 0;
  int appliedVolume = -1;
  bool ownsAudio = false;
  bool codecReady = false;
  bool attempted = false;
};
