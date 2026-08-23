#include "AlarmPcmPlayer.h"
#include "../config.h"
#include "ES8311AlarmCodec.h"
#include <driver/i2s.h>

namespace {
constexpr i2s_port_t ALARM_I2S_PORT = I2S_NUM_0;
constexpr uint16_t PCM_FORMAT = 1;
constexpr uint16_t REQUIRED_CHANNELS = 1;
constexpr uint16_t REQUIRED_BITS_PER_SAMPLE = 16;

uint16_t readLe16(const uint8_t *bytes) {
  return static_cast<uint16_t>(bytes[0]) |
         (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t readLe32(const uint8_t *bytes) {
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

bool hasId(const uint8_t *bytes, const char *id) {
  return memcmp(bytes, id, 4) == 0;
}
} // namespace

bool AlarmPcmPlayer::begin(fs::FS &fs, const char *path) {
  stop();

  file = fs.open(path, FILE_READ);
  if (!file) {
    Serial.printf("[Alarm][PCM] open failed: %s\n", path);
    return false;
  }
  if (!parseWav()) {
    Serial.printf("[Alarm][PCM] invalid WAV: %s\n", path);
    stop();
    return false;
  }
  if (!installI2S()) {
    stop();
    return false;
  }

  // Keep the power amplifier muted while ES8311 resets and its analog
  // reference settles. I2S is already running, so the BCLK-derived codec
  // clock is present throughout initialization.
  digitalWrite(AMP_EN, LOW);
  if (!ES8311AlarmCodec::begin()) {
    Serial.println("[Alarm][PCM] codec initialization failed");
    stop();
    return false;
  }
  if (!restartData()) {
    Serial.println("[Alarm][PCM] data seek failed");
    stop();
    return false;
  }

  sourceBytesPlayed = 0;
  loopCount = 0;
  firstWriteLogged = false;
  active = true;
  digitalWrite(AMP_EN, HIGH);
  Serial.printf("[Alarm][PCM] ready path=%s rate=%lu channels=1 bits=16 "
                "data=%lu\n",
                path, static_cast<unsigned long>(sampleRate),
                static_cast<unsigned long>(dataSize));
  return true;
}

void AlarmPcmPlayer::loop() {
  if (!active) {
    return;
  }

  if (dataRemaining == 0 && !restartData()) {
    Serial.println("[Alarm][PCM] loop seek failed");
    stop();
    return;
  }

  size_t bytesToRead = min(static_cast<uint32_t>(sizeof(monoBuffer)),
                           dataRemaining);
  bytesToRead &= ~static_cast<size_t>(1);
  size_t bytesRead = file.read(reinterpret_cast<uint8_t *>(monoBuffer),
                               bytesToRead);
  if (bytesRead == 0 || (bytesRead & 1U) != 0) {
    Serial.printf("[Alarm][PCM] read failed remaining=%lu read=%u\n",
                  static_cast<unsigned long>(dataRemaining),
                  static_cast<unsigned>(bytesRead));
    stop();
    return;
  }

  size_t samples = bytesRead / sizeof(int16_t);
  for (size_t i = 0; i < samples; ++i) {
    // ES8311 has one DAC path, while ESP32 I2S uses a conventional stereo
    // frame. Duplicate mono into both slots so either codec slot selection
    // receives the same alarm sample.
    stereoBuffer[i * 2] = monoBuffer[i];
    stereoBuffer[i * 2 + 1] = monoBuffer[i];
  }

  size_t requested = samples * 2 * sizeof(int16_t);
  size_t written = 0;
  esp_err_t error = i2s_write(ALARM_I2S_PORT, stereoBuffer, requested,
                              &written, portMAX_DELAY);
  if (error != ESP_OK || written != requested) {
    Serial.printf("[Alarm][PCM] I2S write failed error=%d requested=%u "
                  "written=%u\n",
                  static_cast<int>(error), static_cast<unsigned>(requested),
                  static_cast<unsigned>(written));
    stop();
    return;
  }

  if (!firstWriteLogged) {
    firstWriteLogged = true;
    Serial.printf("[Alarm][PCM] first I2S write=%u bytes\n",
                  static_cast<unsigned>(written));
  }

  dataRemaining -= bytesRead;
  sourceBytesPlayed += bytesRead;
  if (dataRemaining == 0) {
    ++loopCount;
    Serial.printf("[Alarm][PCM] loop=%lu sourceBytes=%lu\n",
                  static_cast<unsigned long>(loopCount),
                  static_cast<unsigned long>(sourceBytesPlayed));
  }
}

void AlarmPcmPlayer::stop() {
  active = false;
  if (i2sInstalled) {
    i2s_zero_dma_buffer(ALARM_I2S_PORT);
    i2s_driver_uninstall(ALARM_I2S_PORT);
    i2sInstalled = false;
  }
  if (file) {
    file.close();
  }
  dataRemaining = 0;
  digitalWrite(AMP_EN, LOW);
}

bool AlarmPcmPlayer::isPlaying() const { return active; }

bool AlarmPcmPlayer::parseWav() {
  uint8_t riffHeader[12];
  if (!readExact(riffHeader, sizeof(riffHeader)) ||
      !hasId(riffHeader, "RIFF") || !hasId(riffHeader + 8, "WAVE")) {
    return false;
  }

  bool foundFormat = false;
  bool foundData = false;
  uint16_t format = 0;
  uint16_t channels = 0;
  uint16_t bitsPerSample = 0;
  uint32_t parsedSampleRate = 0;

  while (file.available() >= 8 && (!foundFormat || !foundData)) {
    uint8_t chunkHeader[8];
    if (!readExact(chunkHeader, sizeof(chunkHeader))) {
      return false;
    }
    uint32_t chunkSize = readLe32(chunkHeader + 4);
    uint32_t chunkOffset = file.position();
    uint64_t nextChunk = static_cast<uint64_t>(chunkOffset) + chunkSize +
                         (chunkSize & 1U);
    if (nextChunk > file.size()) {
      return false;
    }

    if (hasId(chunkHeader, "fmt ")) {
      if (chunkSize < 16) {
        return false;
      }
      uint8_t formatData[16];
      if (!readExact(formatData, sizeof(formatData))) {
        return false;
      }
      format = readLe16(formatData);
      channels = readLe16(formatData + 2);
      parsedSampleRate = readLe32(formatData + 4);
      bitsPerSample = readLe16(formatData + 14);
      foundFormat = true;
    } else if (hasId(chunkHeader, "data")) {
      dataOffset = chunkOffset;
      dataSize = chunkSize;
      foundData = true;
    }

    if (!file.seek(static_cast<uint32_t>(nextChunk))) {
      return false;
    }
  }

  if (!foundFormat || !foundData || format != PCM_FORMAT ||
      channels != REQUIRED_CHANNELS ||
      bitsPerSample != REQUIRED_BITS_PER_SAMPLE || parsedSampleRate == 0 ||
      dataSize == 0 || (dataSize & 1U) != 0) {
    Serial.printf("[Alarm][PCM] WAV format=%u channels=%u rate=%lu bits=%u "
                  "data=%lu\n",
                  format, channels,
                  static_cast<unsigned long>(parsedSampleRate), bitsPerSample,
                  static_cast<unsigned long>(dataSize));
    return false;
  }

  sampleRate = parsedSampleRate;
  return true;
}

bool AlarmPcmPlayer::installI2S() {
  i2s_config_t config{};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = sampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format =
      static_cast<i2s_comm_format_t>(I2S_COMM_FORMAT_STAND_I2S);
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = 256;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;

  esp_err_t error = i2s_driver_install(ALARM_I2S_PORT, &config, 0, nullptr);
  if (error != ESP_OK) {
    Serial.printf("[Alarm][PCM] I2S install failed error=%d\n",
                  static_cast<int>(error));
    return false;
  }
  i2sInstalled = true;

  i2s_pin_config_t pins{};
#if ESP_IDF_VERSION_MAJOR >= 4
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
#endif
  pins.bck_io_num = I2S_BCK;
  pins.ws_io_num = I2S_WS;
  pins.data_out_num = I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  error = i2s_set_pin(ALARM_I2S_PORT, &pins);
  if (error == ESP_OK) {
    error = i2s_set_clk(ALARM_I2S_PORT, sampleRate,
                        I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  }
  if (error != ESP_OK) {
    Serial.printf("[Alarm][PCM] I2S pin/clock failed error=%d\n",
                  static_cast<int>(error));
    return false;
  }

  i2s_zero_dma_buffer(ALARM_I2S_PORT);
  Serial.printf("[Alarm][PCM] I2S ready bclk=%d lrck=%d data=%d rate=%lu\n",
                I2S_BCK, I2S_WS, I2S_DOUT,
                static_cast<unsigned long>(sampleRate));
  return true;
}

bool AlarmPcmPlayer::restartData() {
  if (!file.seek(dataOffset)) {
    return false;
  }
  dataRemaining = dataSize;
  return true;
}

bool AlarmPcmPlayer::readExact(uint8_t *buffer, size_t length) {
  return file.read(buffer, length) == length;
}
