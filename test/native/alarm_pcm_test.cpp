#include "RuntimeFakes.h"
#include "../../src/config.h"
#include "../../src/drivers/AlarmPcmPlayer.h"
#include <driver/i2s.h>
#include <iostream>

FakeSerial Serial;
int amplifier = LOW;
uint32_t writeTimeout = 0;
uint32_t clockRate = 0;
int writeCalls = 0;
int uninstallCalls = 0;
esp_err_t writeError = ESP_OK;
bool partialWrite = false;
std::vector<int16_t> output;
const std::vector<int16_t> mono = {1, -2, 32767, -32768};

void digitalWrite(uint8_t pin, int value) {
  if (pin == AMP_EN) amplifier = value;
}
namespace ES8311AlarmCodec {
bool begin() { return true; }
}
esp_err_t i2s_driver_install(i2s_port_t, const i2s_config_t *, int, void *) {
  return ESP_OK;
}
esp_err_t i2s_set_pin(i2s_port_t, const i2s_pin_config_t *) { return ESP_OK; }
esp_err_t i2s_set_clk(i2s_port_t, uint32_t rate, int, int) {
  clockRate = rate;
  return ESP_OK;
}
esp_err_t i2s_zero_dma_buffer(i2s_port_t) { return ESP_OK; }
esp_err_t i2s_driver_uninstall(i2s_port_t) {
  ++uninstallCalls;
  return ESP_OK;
}
esp_err_t i2s_write(i2s_port_t, const void *bytes, size_t size,
                    size_t *written, uint32_t timeout) {
  ++writeCalls;
  writeTimeout = timeout;
  const int16_t *samples = static_cast<const int16_t *>(bytes);
  output.assign(samples, samples + size / sizeof(int16_t));
  *written = writeError == ESP_OK ? (partialWrite ? size / 2 : size) : 0;
  return writeError;
}

void appendLe(std::string &bytes, uint32_t value, size_t width) {
  for (size_t index = 0; index < width; ++index) {
    bytes.push_back(static_cast<char>((value >> (index * 8)) & 0xFF));
  }
}

std::string makeWav(uint32_t rate) {
  constexpr uint32_t SAMPLE_COUNT = 512;
  std::string bytes = "RIFF";
  appendLe(bytes, 36 + SAMPLE_COUNT * 2, 4);
  bytes += "WAVEfmt ";
  appendLe(bytes, 16, 4);
  appendLe(bytes, 1, 2);
  appendLe(bytes, 1, 2);
  appendLe(bytes, rate, 4);
  appendLe(bytes, rate * 2, 4);
  appendLe(bytes, 2, 2);
  appendLe(bytes, 16, 2);
  bytes += "data";
  appendLe(bytes, SAMPLE_COUNT * 2, 4);
  for (size_t index = 0; index < SAMPLE_COUNT; ++index) {
    appendLe(bytes, static_cast<uint16_t>(mono[index % mono.size()]), 2);
  }
  return bytes;
}

void testPlayback(uint32_t rate) {
  fs::FS fs;
  fs.content = makeWav(rate);
  AlarmPcmPlayer player;
  assert(player.begin(fs, "/alarm.wav"));
  assert(amplifier == HIGH);
  player.loop();
  assert(player.isPlaying());
  assert(clockRate == rate);
  assert(writeTimeout == (512000ULL + rate - 1) / rate + 500);
  assert(writeTimeout != portMAX_DELAY);
  assert(output.size() == 1024);
  for (size_t index = 0; index < 512; ++index) {
    assert(output[index * 2] == mono[index % mono.size()]);
    assert(output[index * 2 + 1] == mono[index % mono.size()]);
  }
  player.loop();
  assert(writeCalls == 2 && player.isPlaying());
  player.stop();
  assert(!player.isPlaying() && amplifier == LOW);
  assert(uninstallCalls == 1);
}

void testFailure(bool partial) {
  fs::FS fs;
  fs.content = makeWav(16000);
  AlarmPcmPlayer player;
  assert(player.begin(fs, "/alarm.wav"));
  writeError = partial ? ESP_OK : ESP_FAIL;
  partialWrite = partial;
  player.loop();
  assert(!player.isPlaying() && amplifier == LOW);
  assert(uninstallCalls == 1);
  assert(writeTimeout != portMAX_DELAY);
  player.loop();
  assert(writeCalls == 1);
  writeError = ESP_OK;
  partialWrite = false;
  assert(player.begin(fs, "/alarm.wav"));
  player.loop();
  assert(player.isPlaying() && writeCalls == 2);
  player.stop();
}

int main(int argc, char **argv) {
  assert(argc == 2);
  std::string test = argv[1];
  if (test == "playback") testPlayback(16000);
  else if (test == "low-rate") testPlayback(610);
  else if (test == "i2s-error") testFailure(false);
  else if (test == "partial-write") testFailure(true);
  else throw std::logic_error("Unknown audio test case: " + test);
  std::cout << "PASS audio-" << test << '\n';
}
