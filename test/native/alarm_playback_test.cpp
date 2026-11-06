#include "../../src/managers/AlarmPlayback.h"
#include "../../src/managers/AlarmManager.h"
#include "../../src/managers/ConfigManager.h"
#include "../../src/drivers/AlarmPcmPlayer.h"
#include "../../src/drivers/AudioDriver.h"
#include "../../src/drivers/SDCardDriver.h"
#include <SPIFFS.h>
#include <iostream>

struct PlaybackState {
  uint32_t now = 100;
  uint32_t sequence = 1;
  uint32_t cpuFrequency = 80;
  String ringtone = "sd:/alarm.mp3";
  bool ringing = true;
  bool compressedOpens = true;
  bool pcmOpens = true;
  bool audioPlaying = false;
  bool codecOpens = true;
  bool flashExists = true;
  bool sdExists = true;
  uint8_t volume = 10;
  uint16_t gain = 32768;
  uint8_t volumeAtOpen = 255;
  uint16_t gainAtOpen = 65535;
  int audioStarts = 0;
  int pcmStarts = 0;
  int pcmLoops = 0;
  int codecStarts = 0;
  int restored = 0;
};

PlaybackState playback;
FakeSerial Serial;
FakeFlash SPIFFS;
unsigned long millis() { return playback.now; }
void digitalWrite(uint8_t, int) {}
bool setCpuFrequencyMhz(uint32_t frequency) {
  playback.cpuFrequency = frequency;
  return true;
}
bool FakeFlash::exists(const String &) const { return playback.flashExists; }

HolidayCalendar::HolidayCalendar() {}
HolidayCalendar::~HolidayCalendar() {}
AlarmManager::AlarmManager() {}
bool AlarmManager::isRinging() const { return playback.ringing; }
uint32_t AlarmManager::getTriggerSequence() const { return playback.sequence; }
String AlarmManager::getActiveRingtone() const { return playback.ringtone; }
ConfigManager::ConfigManager() { config.volume = 10; }
AlarmSoundSettings ConfigManager::getAlarmSoundSettings() const {
  return config.alarmSound;
}
AudioDriver::AudioDriver() {}
AudioDriver::~AudioDriver() {}
void AudioDriver::stop() { playback.audioPlaying = false; }
void AudioDriver::setVolume(uint8_t value) { playback.volume = value; }
bool AudioDriver::isPlaying() { return playback.audioPlaying; }
void AudioDriver::playFromSD(const char *) {
  ++playback.audioStarts;
  playback.volumeAtOpen = playback.volume;
  playback.audioPlaying = playback.compressedOpens;
}
void AudioDriver::playFromFS(fs::FS &, const char *path) { playFromSD(path); }
bool AlarmPcmPlayer::begin(fs::FS &, const char *) {
  ++playback.pcmStarts;
  playback.gainAtOpen = gain;
  active = playback.pcmOpens;
  return active;
}
void AlarmPcmPlayer::stop() { active = false; }
bool AlarmPcmPlayer::isPlaying() const { return active; }
void AlarmPcmPlayer::setGain(uint16_t value) { playback.gain = gain = value; }
void AlarmPcmPlayer::loop() { if (active) ++playback.pcmLoops; }
SDCardDriver::SDCardDriver() {}
bool SDCardDriver::begin() { return playback.sdExists; }
bool SDCardDriver::exists(const char *) { return playback.sdExists; }
namespace ES8311AlarmCodec {
bool begin() {
  ++playback.codecStarts;
  return playback.codecOpens;
}
}

void restoreRadio() { ++playback.restored; }

struct Fixture {
  AlarmManager alarms;
  ConfigManager config;
  AudioDriver audio;
  AlarmPcmPlayer pcm;
  SDCardDriver sd;
  AlarmPlayback controller{&alarms, &config, &audio, &pcm, &sd, restoreRadio};
};

void testCompressedRetry() {
  Fixture fixture;
  playback.compressedOpens = false;
  fixture.controller.service();
  assert(playback.audioStarts == 1 && playback.codecStarts == 0);
  assert(playback.volumeAtOpen == 0 && playback.gain == 0);
  playback.now += 4999;
  fixture.controller.service();
  assert(playback.audioStarts == 1 && playback.volume == 0);
  playback.now += 1;
  playback.compressedOpens = true;
  fixture.controller.service();
  assert(playback.audioStarts == 2 && playback.codecStarts == 1);
  assert(playback.volumeAtOpen == 0 && playback.volume == 0);
  playback.now += 1500;
  fixture.controller.service();
  assert(playback.volume == 10);
  playback.now += 1500;
  fixture.controller.service();
  assert(playback.volume == 21);
}

void testCodecRetry() {
  Fixture fixture;
  playback.codecOpens = false;
  fixture.controller.service();
  assert(!playback.audioPlaying && playback.volume == 0);
  playback.now += 5000;
  playback.codecOpens = true;
  fixture.controller.service();
  assert(playback.audioPlaying && playback.volumeAtOpen == 0);
  assert(playback.volume == 0 && playback.codecStarts == 2);
}

void testPcmLifecycle() {
  Fixture fixture;
  playback.ringtone = "spiffs:/alarm.wav";
  fixture.controller.service();
  assert(playback.pcmStarts == 1 && playback.gainAtOpen == 0);
  playback.now += 1500;
  fixture.controller.service();
  uint16_t middleGain = playback.gain;
  assert(middleGain > 0 && middleGain < AlarmVolumeRamp::UNITY_GAIN);
  fixture.pcm.loop();
  fixture.pcm.loop();
  fixture.controller.service();
  assert(playback.pcmLoops == 2 && playback.pcmStarts == 1);
  assert(playback.gain == middleGain);
  playback.now += 1500;
  fixture.controller.service();
  assert(playback.gain == AlarmVolumeRamp::UNITY_GAIN);
  ++playback.sequence;
  fixture.controller.service();
  assert(playback.pcmStarts == 2 && playback.gainAtOpen == 0);
  assert(playback.gain == 0);
}

void testStopRestore() {
  Fixture fixture;
  fixture.config.config.volume = 7;
  playback.audioPlaying = true;
  fixture.controller.service();
  assert(playback.audioStarts == 1 && playback.volumeAtOpen == 0);
  assert(playback.cpuFrequency == 160);
  playback.ringing = false;
  fixture.controller.service();
  assert(playback.volume == 7 && !playback.audioPlaying);
  assert(playback.cpuFrequency == 80 && playback.restored == 1);
  fixture.controller.stop();
  fixture.controller.service();
  assert(playback.restored == 1);
  playback.ringing = true;
  ++playback.sequence;
  fixture.controller.service();
  assert(playback.volumeAtOpen == 0 && playback.audioStarts == 2);
}

void testNoFade(bool muted) {
  Fixture fixture;
  fixture.config.config.alarmSound.volume = muted ? 0 : 8;
  fixture.config.config.alarmSound.fadeInSeconds = 0;
  playback.ringtone = "spiffs:/alarm.wav";
  fixture.controller.service();
  uint16_t expected = muted ? 0 : 12 * 512;
  assert(playback.volume == (muted ? 0 : 8));
  assert(playback.gainAtOpen == expected && playback.gain == expected);
  playback.now += 60000;
  fixture.controller.service();
  assert(playback.gain == expected && playback.pcmStarts == 1);
}

void testMissingFallback() {
  Fixture fixture;
  playback.sdExists = false;
  playback.flashExists = false;
  fixture.controller.service();
  playback.now += 4999;
  fixture.controller.service();
  assert(playback.audioStarts == 0 && playback.pcmStarts == 0);
  assert(playback.volume == 0 && playback.gain == 0);
  playback.now += 1;
  playback.flashExists = true;
  fixture.controller.service();
  assert(playback.pcmStarts == 1 && playback.gainAtOpen == 0);
  playback.now += 3000;
  fixture.controller.service();
  assert(playback.gain == AlarmVolumeRamp::UNITY_GAIN);
}

int main(int argc, char **argv) {
  assert(argc == 2);
  std::string test = argv[1];
  if (test == "compressed-retry") testCompressedRetry();
  else if (test == "codec-retry") testCodecRetry();
  else if (test == "pcm-lifecycle") testPcmLifecycle();
  else if (test == "stop-restore") testStopRestore();
  else if (test == "no-fade") testNoFade(false);
  else if (test == "mute") testNoFade(true);
  else if (test == "missing-fallback") testMissingFallback();
  else throw std::logic_error("Unknown controller test case: " + test);
  std::cout << "PASS alarm-controller-" << test << '\n';
}
