#pragma once

#include "../drivers/AudioDriver.h"
#include "../drivers/SDCardDriver.h"
#include "ConfigManager.h"
#include <Arduino.h>
#include <vector>

enum LoopMode { LOOP_NONE, LOOP_ALL, LOOP_ONE };

struct TrackInfo {
  String title;
  String artist;
  String path;
  uint32_t duration; // in seconds
};

class MusicManager {
public:
  MusicManager(AudioDriver *audio, SDCardDriver *sd, ConfigManager *config);

  void init();
  void update();

  // Playlist management
  void scanSD();
  const std::vector<TrackInfo> &getPlaylist() const { return playlist; }
  int getCurrentTrackIndex() const { return currentTrackIndex; }

  // Playback control
  void playTrack(int index);
  void togglePlay();
  void nextTrack();
  void prevTrack();
  void stop();
  void seek(uint32_t seconds);

  // Volume & Loop
  void setVolume(int vol);
  int getVolume() const;
  void toggleLoopMode();
  LoopMode getLoopMode() const { return loopMode; }

  // Status
  bool isPlaying() const;
  uint32_t getElapsedSeconds() const;
  uint32_t getTotalSeconds() const;
  String formatTime(uint32_t seconds);

private:
  AudioDriver *audio;
  SDCardDriver *sd;
  ConfigManager *config;

  std::vector<TrackInfo> playlist;
  int currentTrackIndex = -1;
  LoopMode loopMode = LOOP_ALL;

  unsigned long lastUpdate = 0;
  uint32_t elapsedSeconds = 0;

  void loadTrack(int index);
};
