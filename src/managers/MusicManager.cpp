#include "MusicManager.h"

MusicManager::MusicManager(AudioDriver *audio, SDCardDriver *sd,
                           ConfigManager *config)
    : audio(audio), sd(sd), config(config) {}

void MusicManager::init() {
  scanSD();
  if (!playlist.empty()) {
    currentTrackIndex = 0;
  }
}

void MusicManager::update() {
  audio->loop();

  // Check if song finished
  if (currentTrackIndex != -1 && !audio->isPlaying() &&
      audio->getElapsed() >= audio->getDuration() && audio->getDuration() > 0) {
    if (loopMode == LOOP_ONE) {
      loadTrack(currentTrackIndex);
    } else {
      nextTrack();
    }
  }
}

void MusicManager::scanSD() {
  playlist.clear();
  std::vector<FileInfo> files = sd->listDir("/");
  for (const auto &file : files) {
    if (!file.isDirectory && file.name.endsWith(".mp3")) {
      TrackInfo track;
      track.title = file.name;
      // Remove extension for title
      int dotIdx = track.title.lastIndexOf('.');
      if (dotIdx > 0)
        track.title = track.title.substring(0, dotIdx);

      track.artist = "Unknown Artist";
      track.path = "/" + file.name;
      track.duration = 0; // Duration will be known once played or parsed
      playlist.push_back(track);
    }
  }

  if (currentTrackIndex >= (int)playlist.size()) {
    currentTrackIndex = playlist.empty() ? -1 : 0;
  }
}

void MusicManager::playTrack(int index) {
  if (index >= 0 && index < (int)playlist.size()) {
    currentTrackIndex = index;
    loadTrack(index);
  }
}

void MusicManager::togglePlay() {
  if (currentTrackIndex == -1 && !playlist.empty()) {
    playTrack(0);
  } else {
    audio->pause(); // Audio library pauseResume() handles both
  }
}

void MusicManager::nextTrack() {
  if (playlist.empty())
    return;
  currentTrackIndex = (currentTrackIndex + 1) % playlist.size();
  loadTrack(currentTrackIndex);
}

void MusicManager::prevTrack() {
  if (playlist.empty())
    return;
  currentTrackIndex =
      (currentTrackIndex - 1 + playlist.size()) % playlist.size();
  loadTrack(currentTrackIndex);
}

void MusicManager::stop() { audio->stop(); }

void MusicManager::seek(uint32_t seconds) {
  // Audio library usually doesn't have a simple seek(seconds) for all formats,
  // but some versions have audio.setTimeOffset(seconds).
  // For now, let's keep it simple as the HTML design's seek is optional.
}

void MusicManager::setVolume(int vol) {
  config->config.volume =
      constrain(vol, 0, 15); // Audio library range is typically 0-21 or 0-100
  audio->setVolume(config->config.volume);
  config->save();
}

int MusicManager::getVolume() const { return config->config.volume; }

void MusicManager::toggleLoopMode() {
  if (loopMode == LOOP_ALL)
    loopMode = LOOP_ONE;
  else if (loopMode == LOOP_ONE)
    loopMode = LOOP_NONE;
  else
    loopMode = LOOP_ALL;
}

bool MusicManager::isPlaying() const { return audio->isPlaying(); }

uint32_t MusicManager::getElapsedSeconds() const { return audio->getElapsed(); }

uint32_t MusicManager::getTotalSeconds() const { return audio->getDuration(); }

String MusicManager::formatTime(uint32_t seconds) {
  uint32_t m = seconds / 60;
  uint32_t s = seconds % 60;
  char buf[10];
  snprintf(buf, sizeof(buf), "%02u:%02u", m, s);
  return String(buf);
}

void MusicManager::loadTrack(int index) {
  if (index >= 0 && index < (int)playlist.size()) {
    audio->playFromSD(playlist[index].path.c_str());
  }
}
