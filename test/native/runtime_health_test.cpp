#include "RuntimeFakes.h"
#include "../../src/config.h"
#include "../../src/utils/HealthFile.h"
#include "../../src/utils/IdleSleep.h"
#include "../../src/utils/RuntimeHealth.h"
#include <functional>
#include <iostream>

FakeRuntime runtime;
FakeSerial Serial;
using RuntimeHealth::Stage;

const char *esp_err_to_name(esp_err_t error) {
  return error == ESP_OK ? "ESP_OK" : "ESP_FAIL";
}
TaskHandle_t xTaskGetCurrentTaskHandle() { return runtime.task; }
esp_reset_reason_t esp_reset_reason() { return runtime.resetReason; }
int64_t esp_timer_get_time() { return runtime.nowUs; }
unsigned long millis() { return static_cast<unsigned long>(runtime.nowUs / 1000); }
[[noreturn]] void esp_system_abort(const char *reason) {
  throw std::runtime_error(reason);
}

esp_err_t esp_timer_create(const esp_timer_create_args_t *args,
                           esp_timer_handle_t *handle) {
  ++runtime.createCalls;
  runtime.timerArgs = *args;
  if (runtime.createError == ESP_OK) {
    *handle = reinterpret_cast<void *>(2);
  }
  return runtime.createError;
}
esp_err_t esp_timer_start_periodic(esp_timer_handle_t, uint64_t period) {
  assert(period == 1000000);
  assert(runtime.timerArgs.skip_unhandled_events);
  return runtime.startError;
}
esp_err_t esp_timer_delete(esp_timer_handle_t) {
  ++runtime.deleteCalls;
  return ESP_OK;
}

esp_err_t wakeupResult() {
  return ++runtime.wakeupCalls == runtime.failWakeupCall ? ESP_FAIL : ESP_OK;
}
esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_wakeup_cause_t source) {
  assert(source == ESP_SLEEP_WAKEUP_ALL);
  return wakeupResult();
}
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t duration) {
  runtime.sleepDuration = duration;
  return wakeupResult();
}
esp_err_t gpio_wakeup_enable(gpio_num_t pin, int level) {
  assert(level == GPIO_INTR_LOW_LEVEL);
  runtime.pins.push_back(pin);
  return pin == runtime.failedPin ? ESP_FAIL : ESP_OK;
}
esp_err_t esp_sleep_enable_gpio_wakeup() { return wakeupResult(); }
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return runtime.wakeCause; }

void checkTimer() { runtime.timerArgs.callback(runtime.timerArgs.arg); }
esp_err_t esp_light_sleep_start() {
  ++runtime.sleepCalls;
  if (runtime.sleepError == ESP_OK) {
    runtime.nowUs += runtime.earlyWakeUs > 0 ? runtime.earlyWakeUs
                                            : runtime.sleepDuration;
    // 模拟长休眠醒来后，在主循环恢复前先执行积压的定时器回调。
    checkTimer();
  }
  return runtime.sleepError;
}

void startMonitor() {
  RuntimeHealth::reportBoot();
  assert(RuntimeHealth::begin());
}

std::string expectPanic(const std::function<void()> &action) {
  try {
    action();
  } catch (const std::runtime_error &error) {
    return error.what();
  }
  throw std::logic_error("Expected stalled-main panic was not raised");
}

void testStall() {
  startMonitor();
  RuntimeHealth::mark(Stage::RtcSync, 3);
  runtime.nowUs = RuntimeHealth::ACTIVE_TIMEOUT_US;
  checkTimer();
  runtime.nowUs++;
  std::string panic = expectPanic(checkTimer);
  assert(panic.find("stage=rtc-sync screen=3") != std::string::npos);
  runtime.resetReason = ESP_RST_PANIC;
  RuntimeHealth::reportBoot();
  assert(Serial.output.find("timeout=1 stalled=120000ms recoveries=1") !=
         std::string::npos);
  runtime.resetReason = ESP_RST_POWERON;
  Serial.output.clear();
  RuntimeHealth::reportBoot();
  assert(Serial.output.find("previous") == std::string::npos);
}

void testBackgroundCannotHideStall() {
  startMonitor();
  RuntimeHealth::mark(Stage::Input, 5);
  runtime.nowUs = RuntimeHealth::ACTIVE_TIMEOUT_US - 1;
  runtime.task = reinterpret_cast<void *>(99);
  RuntimeHealth::progress();
  RuntimeHealth::mark(Stage::Audio, 0);
  RuntimeHealth::prepareSleep(3600000000ULL);
  runtime.nowUs += 2;
  std::string panic = expectPanic(checkTimer);
  assert(panic.find("stage=input screen=5") != std::string::npos);
}

void testLongOperationProgress() {
  startMonitor();
  RuntimeHealth::mark(Stage::Input, 4);
  for (int step = 0; step < 100; ++step) {
    runtime.nowUs += 30000000;
    RuntimeHealth::progress();
    checkTimer();
  }
  runtime.nowUs += RuntimeHealth::ACTIVE_TIMEOUT_US + 1;
  assert(expectPanic(checkTimer).find("stage=input") != std::string::npos);
}

void testSleepAndWake() {
  startMonitor();
  RuntimeHealth::mark(Stage::Idle, 0);
  assert(IdleSleep::enter(3600000000ULL));
  assert(runtime.sleepDuration == 3600000000ULL);
  assert((runtime.pins == std::vector<int>{KEY_LEFT, KEY_RIGHT, KEY_ENTER}));
  RuntimeHealth::mark(Stage::Idle, 0);
  runtime.nowUs += RuntimeHealth::ACTIVE_TIMEOUT_US + 1;
  assert(expectPanic(checkTimer).find("stage=idle") != std::string::npos);
}

void testEarlyButtonWake() {
  startMonitor();
  runtime.earlyWakeUs = 500000;
  runtime.wakeCause = ESP_SLEEP_WAKEUP_GPIO;
  assert(IdleSleep::enter(3600000000ULL));
  assert(runtime.nowUs == 500000);
  assert(Serial.output.find("cause=gpio") != std::string::npos);
  // 提前醒来后必须恢复普通期限，不能沿用剩下的一小时“休眠豁免”。
  runtime.nowUs += RuntimeHealth::ACTIVE_TIMEOUT_US + 1;
  expectPanic(checkTimer);
}

void testOverdueSleep() {
  startMonitor();
  RuntimeHealth::prepareSleep(3600000000ULL);
  runtime.nowUs = 3600000000ULL + RuntimeHealth::ACTIVE_TIMEOUT_US + 1;
  assert(expectPanic(checkTimer).find("stage=sleeping") != std::string::npos);
}

void testLongUptime() {
  runtime.nowUs = (static_cast<int64_t>(UINT32_MAX) - 1000) * 1000;
  startMonitor();
  RuntimeHealth::mark(Stage::UiUpdate, 0);
  runtime.nowUs += 2000000;
  checkTimer();
  runtime.nowUs += RuntimeHealth::ACTIVE_TIMEOUT_US;
  expectPanic(checkTimer);
}

void testSleepFailure(int failedCall, int failedPin, esp_err_t sleepError) {
  startMonitor();
  runtime.failWakeupCall = failedCall;
  runtime.failedPin = failedPin;
  runtime.sleepError = sleepError;
  assert(!IdleSleep::enter(3600000000ULL));
  int expectedSleepCalls = failedCall == 0 && failedPin < 0 ? 1 : 0;
  assert(runtime.sleepCalls == expectedSleepCalls);
  assert(Serial.output.find("staying awake") != std::string::npos);
  runtime.nowUs = RuntimeHealth::ACTIVE_TIMEOUT_US + 1;
  expectPanic(checkTimer);
}

void testMonitorStartFailure(bool createFailure) {
  runtime.createError = createFailure ? ESP_ERR_NO_MEM : ESP_OK;
  runtime.startError = createFailure ? ESP_OK : ESP_FAIL;
  assert(!RuntimeHealth::begin());
  assert(runtime.deleteCalls == (createFailure ? 0 : 1));
  runtime.createError = runtime.startError = ESP_OK;
  assert(RuntimeHealth::begin());
  assert(runtime.createCalls == 2);
  assert(RuntimeHealth::begin());
  assert(runtime.createCalls == 2);
}

void testDownload() {
  startMonitor();
  File file(std::string(1360 * 10, 'x'));
  HealthFile download(file);
  assert(download.size() == file.size());
  assert(std::string(download.name()) == file.name());
  Stream &stream = download;
  uint8_t bytes[1360];
  for (int chunk = 0; chunk < 10; ++chunk) {
    runtime.nowUs += 30000000;
    assert(stream.readBytes(bytes, sizeof(bytes)) == sizeof(bytes));
    for (uint8_t byte : bytes) {
      assert(byte == 'x');
    }
    checkTimer();
  }
  assert(file.position() == file.size());
  runtime.nowUs += RuntimeHealth::ACTIVE_TIMEOUT_US - 1;
  assert(stream.readBytes(bytes, sizeof(bytes)) == 0);
  runtime.nowUs += 2;
  expectPanic(checkTimer);
}

int main(int argc, char **argv) {
  assert(argc == 2);
  std::string test = argv[1];
  if (test == "stall") testStall();
  else if (test == "background") testBackgroundCannotHideStall();
  else if (test == "long-operation") testLongOperationProgress();
  else if (test == "sleep") testSleepAndWake();
  else if (test == "early-wake") testEarlyButtonWake();
  else if (test == "overdue-sleep") testOverdueSleep();
  else if (test == "long-uptime") testLongUptime();
  else if (test == "wake-disable-fail") testSleepFailure(1, -1, ESP_OK);
  else if (test == "wake-timer-fail") testSleepFailure(2, -1, ESP_OK);
  else if (test == "wake-source-fail") testSleepFailure(3, -1, ESP_OK);
  else if (test == "wake-left-fail") testSleepFailure(0, KEY_LEFT, ESP_OK);
  else if (test == "wake-right-fail") testSleepFailure(0, KEY_RIGHT, ESP_OK);
  else if (test == "wake-enter-fail") testSleepFailure(0, KEY_ENTER, ESP_OK);
  else if (test == "sleep-reject") testSleepFailure(0, -1, ESP_FAIL);
  else if (test == "create-fail") testMonitorStartFailure(true);
  else if (test == "start-fail") testMonitorStartFailure(false);
  else if (test == "download") testDownload();
  else throw std::logic_error("Unknown test case: " + test);
  std::cout << "PASS " << test << '\n';
}
