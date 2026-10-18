#include "RuntimeHealth.h"
#include <Arduino.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr uint32_t RECORD_MAGIC = 0x484C5431;
constexpr uint64_t CHECK_INTERVAL_US = 1000000ULL;
struct HealthRecord {
  uint32_t magic;
  uint32_t inverseMagic;
  uint32_t recoveries;
  uint32_t stalledMs;
  uint8_t stage;
  uint8_t screen;
  bool timedOut;
};

// 关键逻辑：阶段信息只写 RTC RAM，不写 NVS/Flash；软件复位后仍能读到
// 上次卡住位置。冷启动不信任旧 RAM，魔数及其反码共同过滤无效记录。
RTC_NOINIT_ATTR HealthRecord record;
portMUX_TYPE healthMux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t mainTask = nullptr;
esp_timer_handle_t healthTimer = nullptr;
int64_t lastProgressUs = 0;
int64_t deadlineUs = 0;
bool failureLatched = false;

const char *stageName(uint8_t stage) {
  static const char *const names[] = {
      "starting", "scheduled-tasks", "rtc-sync", "ui-update", "audio",
      "alarm", "input", "power", "idle", "sleeping"};
  static_assert(sizeof(names) / sizeof(names[0]) ==
                    static_cast<uint8_t>(RuntimeHealth::Stage::Count),
                "Health stage names must match the stage enum");
  return stage < static_cast<uint8_t>(RuntimeHealth::Stage::Count)
             ? names[stage] : "unknown";
}

bool isMainTask() {
  return mainTask != nullptr && xTaskGetCurrentTaskHandle() == mainTask;
}

void updateProgress(int64_t allowanceUs,
                    RuntimeHealth::Stage stage = RuntimeHealth::Stage::Count,
                    uint8_t screen = 0) {
  int64_t nowUs = esp_timer_get_time();
  portENTER_CRITICAL(&healthMux);
  if (!failureLatched) {
    if (stage != RuntimeHealth::Stage::Count) {
      record.stage = static_cast<uint8_t>(stage);
      record.screen = screen;
    }
    lastProgressUs = nowUs;
    deadlineUs = nowUs + allowanceUs + RuntimeHealth::ACTIVE_TIMEOUT_US;
  }
  portEXIT_CRITICAL(&healthMux);
}

void checkProgress(void *) {
  int64_t nowUs = esp_timer_get_time();
  portENTER_CRITICAL(&healthMux);
  bool expired = !failureLatched && nowUs > deadlineUs;
  if (expired) {
    failureLatched = true;
    record.timedOut = true;
    record.recoveries++;
    record.stalledMs = static_cast<uint32_t>((nowUs - lastProgressUs) / 1000);
  }
  portEXIT_CRITICAL(&healthMux);
  if (!expired) {
    return;
  }

  // 关键逻辑：独立的系统定时器任务只检查进度，绝不替主循环“喂狗”。
  // 先保留 RTC 现场，再进入 panic，复用现有 coredump/复位机制；这里
  // 不获取业务锁、不等待 Serial.flush，避免诊断本身再次被阻塞。
  char reason[128];
  snprintf(reason, sizeof(reason),
           "Main loop stalled: stage=%s screen=%u elapsed=%lums recovery=%lu",
           stageName(record.stage), record.screen,
           static_cast<unsigned long>(record.stalledMs),
           static_cast<unsigned long>(record.recoveries));
  esp_system_abort(reason);
}
} // namespace

namespace RuntimeHealth {
void reportBoot() {
  esp_reset_reason_t reason = esp_reset_reason();
  bool valid = reason != ESP_RST_POWERON && reason != ESP_RST_BROWNOUT &&
               record.magic == RECORD_MAGIC &&
               record.inverseMagic == ~RECORD_MAGIC &&
               record.stage < static_cast<uint8_t>(Stage::Count);
  Serial.printf("[Health] reset_reason=%d\n", static_cast<int>(reason));
  if (valid) {
    Serial.printf("[Health] previous stage=%s screen=%u timeout=%d "
                  "stalled=%lums recoveries=%lu\n",
                  stageName(record.stage), record.screen, record.timedOut,
                  static_cast<unsigned long>(record.stalledMs),
                  static_cast<unsigned long>(record.recoveries));
  }
  uint32_t recoveries = valid ? record.recoveries : 0;
  record = {};
  record.inverseMagic = ~RECORD_MAGIC;
  record.recoveries = recoveries;
  record.magic = RECORD_MAGIC;
}

bool begin() {
  if (healthTimer != nullptr) {
    return true;
  }
  mainTask = xTaskGetCurrentTaskHandle();
  updateProgress(0);
  esp_timer_create_args_t args = {};
  args.callback = checkProgress;
  args.name = "main-health";
  // 关键逻辑：保留原有休眠时长，不让健康检查额外唤醒设备；醒来后
  // 合并检查，避免长时间休眠产生大量积压回调。
  args.skip_unhandled_events = true;
  esp_err_t error = esp_timer_create(&args, &healthTimer);
  if (error == ESP_OK) {
    error = esp_timer_start_periodic(healthTimer, CHECK_INTERVAL_US);
  }
  if (error != ESP_OK) {
    Serial.printf("[Health] monitor start failed: %s\n", esp_err_to_name(error));
    if (healthTimer != nullptr) {
      esp_timer_delete(healthTimer);
      healthTimer = nullptr;
    }
    mainTask = nullptr;
    return false;
  }
  Serial.println("[Health] main loop progress monitor enabled (120s)");
  return true;
}

void mark(Stage stage, uint8_t screen) {
  if (!isMainTask()) {
    return;
  }
  updateProgress(0, stage, screen);
}

void progress() {
  if (isMainTask()) {
    updateProgress(0);
  }
}

void prepareSleep(uint64_t durationUs) {
  if (!isMainTask()) {
    return;
  }
  // 正常休眠不代表失去进度，期限必须包含本次预定休眠时间。
  updateProgress(static_cast<int64_t>(durationUs), Stage::Sleeping, record.screen);
}
} // namespace RuntimeHealth
