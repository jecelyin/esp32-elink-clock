#pragma once

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
constexpr esp_err_t ESP_ERR_NO_MEM = 0x101;
const char *esp_err_to_name(esp_err_t error);

using TaskHandle_t = void *;
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) (++*(mux))
#define portEXIT_CRITICAL(mux) (--*(mux))
#define RTC_NOINIT_ATTR
TaskHandle_t xTaskGetCurrentTaskHandle();

enum esp_reset_reason_t {
  ESP_RST_POWERON = 1, ESP_RST_SW = 3, ESP_RST_PANIC = 4, ESP_RST_BROWNOUT = 9
};
esp_reset_reason_t esp_reset_reason();
[[noreturn]] void esp_system_abort(const char *reason);

using esp_timer_handle_t = void *;
struct esp_timer_create_args_t {
  void (*callback)(void *) = nullptr;
  void *arg = nullptr;
  const char *name = nullptr;
  bool skip_unhandled_events = false;
};
int64_t esp_timer_get_time();
esp_err_t esp_timer_create(const esp_timer_create_args_t *args,
                           esp_timer_handle_t *handle);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t handle, uint64_t period);
esp_err_t esp_timer_delete(esp_timer_handle_t handle);

enum esp_sleep_wakeup_cause_t {
  ESP_SLEEP_WAKEUP_UNDEFINED = 0, ESP_SLEEP_WAKEUP_ALL = 1,
  ESP_SLEEP_WAKEUP_TIMER = 4, ESP_SLEEP_WAKEUP_GPIO = 7
};
using gpio_num_t = int;
constexpr int GPIO_INTR_LOW_LEVEL = 4;
esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_wakeup_cause_t source);
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t duration);
esp_err_t gpio_wakeup_enable(gpio_num_t pin, int level);
esp_err_t esp_sleep_enable_gpio_wakeup();
esp_err_t esp_light_sleep_start();
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause();

struct FakeSerial {
  std::string output;
  void printf(const char *format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    output += text;
  }
  void println(const char *text) { output += std::string(text) + "\n"; }
  void flush() {}
};
extern FakeSerial Serial;
unsigned long millis();

struct FakeRuntime {
  int64_t nowUs = 0;
  TaskHandle_t task = reinterpret_cast<TaskHandle_t>(1);
  esp_reset_reason_t resetReason = ESP_RST_POWERON;
  esp_timer_create_args_t timerArgs;
  esp_err_t createError = ESP_OK;
  esp_err_t startError = ESP_OK;
  int createCalls = 0;
  int deleteCalls = 0;
  int wakeupCalls = 0;
  int failWakeupCall = 0;
  int sleepCalls = 0;
  int failedPin = -1;
  std::vector<int> pins;
  uint64_t sleepDuration = 0;
  uint64_t earlyWakeUs = 0;
  esp_err_t sleepError = ESP_OK;
  esp_sleep_wakeup_cause_t wakeCause = ESP_SLEEP_WAKEUP_TIMER;
};
extern FakeRuntime runtime;
