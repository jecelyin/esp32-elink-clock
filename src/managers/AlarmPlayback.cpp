#include "AlarmPlayback.h"
#include "AlarmManager.h"
#include "ConfigManager.h"
#include "../drivers/AlarmPcmPlayer.h"
#include "../drivers/AudioDriver.h"
#include "../drivers/ES8311AlarmCodec.h"
#include "../drivers/SDCardDriver.h"
#include <SPIFFS.h>

namespace {
constexpr uint8_t IDLE_CPU_FREQUENCY_MHZ = 80;
constexpr uint8_t ALARM_CPU_FREQUENCY_MHZ = 160;
constexpr uint32_t RETRY_INTERVAL_MS = 5000UL;
} // namespace

AlarmPlayback::AlarmPlayback(AlarmManager *alarms, ConfigManager *config,
                             AudioDriver *audio, AlarmPcmPlayer *pcm,
                             SDCardDriver *sd, void (*restoreRadio)())
    : alarms(alarms), config(config), audio(audio), pcm(pcm), sd(sd),
      restoreRadio(restoreRadio) {}

void AlarmPlayback::stop() {
  if (!ownsAudio) {
    return;
  }
  pcm->stop();
  audio->stop();
  audio->setVolume(config->config.volume);
  setCpuFrequencyMhz(IDLE_CPU_FREQUENCY_MHZ);
  ownsAudio = false;
  codecReady = false;
  attempted = false;
  restoreRadio();
}

void AlarmPlayback::acquire(uint32_t sequence) {
  // 新闹钟无论是否复用同一个文件，都必须停止旧音频并从静音重新渐强。
  // 设置只在本次响铃开始时取快照，网页保存不会突然改变正在响铃的增益。
  pcm->stop();
  audio->stop();
  ramp.reset(config->getAlarmSoundSettings());
  appliedVolume = -1;
  applyVolume();
  setCpuFrequencyMhz(ALARM_CPU_FREQUENCY_MHZ);
  digitalWrite(RADIO_EN, LOW);
  ownsAudio = true;
  codecReady = false;
  attempted = false;
  triggerSequence = sequence;
  Serial.println("[Alarm] audio channel acquired");
}

void AlarmPlayback::applyVolume() {
  uint32_t now = millis();
  uint8_t volume = ramp.volume(now);
  if (appliedVolume != volume) {
    audio->setVolume(volume);
    appliedVolume = volume;
  }
  pcm->setGain(ramp.gain(now));
}

bool AlarmPlayback::prepareCodec() {
  if (!codecReady) {
    // 解码器已经提供 I2S 时钟；功放在 ES8311 初始化结束前保持静音。
    digitalWrite(AMP_EN, LOW);
    if (!ES8311AlarmCodec::begin()) {
      Serial.println("[Alarm] codec initialization failed");
      audio->stop();
      return false;
    }
    codecReady = true;
  }
  digitalWrite(AMP_EN, HIGH);
  return true;
}

bool AlarmPlayback::playFromFlash(const String &path) {
  if (path.endsWith(".wav")) {
    return pcm->begin(SPIFFS, path.c_str());
  }
  audio->playFromFS(SPIFFS, path.c_str());
  return audio->isPlaying() && prepareCodec();
}

bool AlarmPlayback::playConfiguredAlarm() {
  uint32_t now = millis();
  if (attempted && now - lastAttemptMs < RETRY_INTERVAL_MS) {
    return false;
  }
  attempted = true;
  lastAttemptMs = now;
  String ringtone = AlarmRingtones::normalize(alarms->getActiveRingtone());
  if (ringtone.startsWith("sd:")) {
    String path = ringtone.substring(3);
    if (sd->begin() && sd->exists(path.c_str())) {
      audio->playFromSD(path.c_str());
      // 文件存在不代表解码器成功打开；失败时不能提前消耗渐强时间。
      return audio->isPlaying() && prepareCodec();
    }
  }
  if (ringtone.startsWith("spiffs:")) {
    String path = ringtone.substring(7);
    if (SPIFFS.exists(path)) {
      return playFromFlash(path);
    }
  }
  String fallback = String(AlarmRingtones::DEFAULT_VALUE).substring(7);
  if (SPIFFS.exists(fallback)) {
    Serial.printf("Alarm ringtone not found: %s, using default\n",
                  ringtone.c_str());
    return pcm->begin(SPIFFS, fallback.c_str());
  }
  Serial.printf("Alarm ringtone not found: %s\n", ringtone.c_str());
  return false;
}

void AlarmPlayback::service() {
  if (!alarms->isRinging()) {
    stop();
    return;
  }
  uint32_t sequence = alarms->getTriggerSequence();
  if (!ownsAudio || sequence != triggerSequence) {
    acquire(sequence);
  }
  applyVolume();
  if (!pcm->isPlaying() && !audio->isPlaying() && playConfiguredAlarm()) {
    // 第一次成功打开铃声后才开始计时，避免 SD 挂载、编解码器启动或
    // 文件失败重试消耗渐强时间；文件循环与重试不会重置同次响铃包络。
    ramp.start(millis());
    applyVolume();
    Serial.println("[Alarm] ringtone playback started");
  }
}
