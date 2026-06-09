# ESP32 电子墨水屏时钟 - 功耗优化指南

## 当前状态
- 平均电流：50mA
- 目标电流：1-5mA（轻度睡眠模式）

## 已完成的优化

### 1. 代码层面优化
- [x] 降低 CPU 频率到 80MHz
- [x] 启用电源管理（轻度睡眠）
- [x] WiFi 按需连接（每小时同步一次）
- [x] 外设电源动态管理（音频、收音机）
- [x] 增加任务延迟时间

### 2. 待优化的关键点

## 深度优化方案

### 方案 1：禁用串口输出（预计降低 10-20mA）

**问题**：`Serial.println()` 会保持 UART 活跃，阻止轻度睡眠。

**解决方案**：
在 `config.h` 或 `main.cpp` 开头添加：
```cpp
#define DISABLE_SERIAL  // 禁用所有串口输出
```

或者在调试完成后，注释掉所有 `Serial.print` 调用。

**预期效果**：降低 10-20mA

---

### 方案 2：使用外部中断处理按键（预计降低 5-10mA）

**问题**：当前使用轮询方式检测按键，每 50ms 执行一次。

**解决方案**：
修改 `InputDriver`，使用外部中断：

```cpp
// config.h 中添加中断引脚定义
#define KEY_LEFT_INT GPIO_INTR_NEGEDGE
#define KEY_RIGHT_INT GPIO_INTR_NEGEDGE
#define KEY_ENTER_INT GPIO_INTR_NEGEDGE

// InputDriver.cpp 中修改
void IRAM_ATTR keyInterruptHandler(void* arg) {
  // 设置唤醒标志
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(keySemaphore, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

void InputDriver::begin() {
  // 配置外部中断
  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_INTR_NEGEDGE;
  io_conf.pin_bit_mask = (1ULL<<KEY_LEFT) | (1ULL<<KEY_RIGHT) | (1ULL<<KEY_ENTER);
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_conf);
  
  // 安装 GPIO 中断服务
  gpio_install_isr_service(0);
  gpio_isr_handler_add((gpio_num_t)KEY_LEFT, keyInterruptHandler, (void*)KEY_LEFT);
  gpio_isr_handler_add((gpio_num_t)KEY_RIGHT, keyInterruptHandler, (void*)KEY_RIGHT);
  gpio_isr_handler_add((gpio_num_t)KEY_ENTER, keyInterruptHandler, (void*)KEY_ENTER);
}
```

**预期效果**：降低 5-10mA（CPU 可以更频繁地睡眠）

---

### 方案 3：优化 I2C 通信频率（预计降低 3-5mA）

**问题**：每 30 秒读取一次 RTC，I2C 通信消耗电力。

**解决方案**：
修改 `main.cpp` 中的系统检查间隔：

```cpp
// 从 30 秒改为 5 分钟
uint32_t checkInterval = alarmManager.isRinging() ? 1000 : 300000; // 5 分钟
```

或者更激进：只在分钟变化时读取 RTC：

```cpp
static uint8_t lastMinute = 99;
uint8_t currentMinute = rtcDriver.getMinute();
if (currentMinute != lastMinute) {
  lastMinute = currentMinute;
  // 更新显示
  uiManager.update();
}
delay(1000); // 每秒检查一次分钟变化
```

**预期效果**：降低 3-5mA

---

### 方案 4：确保 WiFi 完全断电（预计降低 20-50mA）

**问题**：`WiFi.mode(WIFI_OFF)` 可能没有完全切断射频电源。

**解决方案**：
在 `ConnectionManager.cpp` 的 `enableNetwork(false)` 中添加：

```cpp
void ConnectionManager::enableNetwork(bool enable) {
  if (networkEnabled == enable)
    return;

  networkEnabled = enable;
  if (!enable) {
    WiFi.disconnect(true, true);  // 完全断开，擦除凭证
    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();  // 完全停止 WiFi 驱动
    Serial.println("WiFi Power Off to save energy");
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin();  // 使用保存的凭证连接
    Serial.println("WiFi Power On for sync");
  }
}
```

**预期效果**：如果 WiFi 没有完全断电，可降低 20-50mA

---

### 方案 5：使用深度睡眠 + 定时器唤醒（预计降低到 0.1-0.5mA）

**适用场景**：如果不需要实时响应按键，可以进入深度睡眠。

**实现方案**：
```cpp
#include <esp_sleep.h>

void enterDeepSleep(uint32_t sleepTimeUs) {
  // 配置唤醒源：定时器 + 按键（需要按键引脚支持唤醒）
  esp_sleep_enable_timer_wakeup(sleepTimeUs);
  
  // 配置 GPIO 唤醒（按键）
  esp_sleep_enable_ext0_wakeup((gpio_num_t)KEY_ENTER, 0);
  esp_sleep_enable_ext1_wakeup(
    (1ULL << KEY_LEFT) | (1ULL << KEY_RIGHT), 
    ESP_EXT1_WAKEUP_ALL_LOW
  );
  
  // 进入深度睡眠
  esp_deep_sleep_start();
}

// 在 loop() 中调用
if (!audioDriver.isPlaying() && !alarmManager.isRinging()) {
  enterDeepSleep(60 * 1000000);  // 睡眠 60 秒
}
```

**注意**：深度睡眠会重置 CPU，需要保存状态到 RTC 内存。

**预期效果**：降低到 0.1-0.5mA（但体验会变差）

---

## 推荐实施顺序

1. **第一阶段**（预计降到 20-30mA）
   - [ ] 禁用串口输出
   - [ ] 确保 WiFi 完全断电
   - [ ] 增加主循环延迟到 100-200ms

2. **第二阶段**（预计降到 5-10mA）
   - [ ] 使用外部中断处理按键
   - [ ] 优化 I2C 通信频率
   - [ ] 确保电子墨水屏控制器进入睡眠

3. **第三阶段**（预计降到 1-5mA）
   - [ ] 实施轻度睡眠优化
   - [ ] 禁用所有不必要的外设时钟
   - [ ] 优化 FreeRTOS 任务优先级

4. **第四阶段**（预计降到 <1mA）
   - [ ] 使用深度睡眠架构
   - [ ] 使用 RX8010SJ 闹钟中断唤醒

---

## 测试方法

### 1. 电流测量
使用万用表或示波器测量电流：
- 串联一个采样电阻（1-10Ω）到电源
- 使用示波器测量电压降
- 计算电流：I = U / R

### 2. 代码调试
添加电流测量点：
```cpp
// 在关键代码段前后测量电流
digitalWrite(DEBUG_PIN, HIGH);
// 代码段
digitalWrite(DEBUG_PIN, LOW);
```

### 3. 使用 ESP32 内置电流测量
某些开发板支持：
```cpp
#include <esp_bluedroid_api.h>
#include <esp_bt.h>

// 禁用蓝牙和 WiFi 射频
esp_bluedroid_disable();
esp_bt_controller_disable();
```

---

## 常见问题

### Q1: 轻度睡眠不生效怎么办？
**A**: 检查是否有以下阻止睡眠的因素：
- UART 输出（Serial.print）
- 活跃的定时器（Timer）
- 未处理的中断
- WiFi/BT 射频未完全关闭

### Q2: 如何确认轻度睡眠是否生效？
**A**: 测量电流，或者使用 GPIO 输出心跳信号：
```cpp
void loop() {
  digitalWrite(HEARTBEAT_PIN, HIGH);
  delay(10);
  digitalWrite(HEARTBEAT_PIN, LOW);
  delay(100);  // 如果轻度睡眠生效，这段时间电流会下降
}
```

### Q3: 深度睡眠后如何恢复状态？
**A**: 使用 RTC 内存或 EEPROM：
```cpp
RTC_DATA_ATTR int bootCount = 0;  // 存储在 RTC 内存，深度睡眠不丢失

void setup() {
  bootCount++;
  Serial.println("Boot count: " + String(bootCount));
}
```

---

## 预期效果总结

| 优化阶段 | 预期电流 | 关键措施 |
|---------|---------|---------|
| 当前状态 | 50mA | 基准 |
| 第一阶段 | 20-30mA | 禁用串口 + WiFi 完全断电 |
| 第二阶段 | 5-10mA | 外部中断 + 优化 I2C |
| 第三阶段 | 1-5mA | 轻度睡眠 + 外设管理 |
| 第四阶段 | <1mA | 深度睡眠架构 |

---

## 下一步行动

1. **立即实施**：禁用串口输出（`#define DISABLE_SERIAL`）
2. **本周完成**：确保 WiFi 完全断电
3. **下周实施**：改用外部中断处理按键
4. **长期规划**：如果电池续航仍不满足，考虑深度睡眠架构

---

**最后建议**：对于电子墨水屏时钟，推荐使用**第三阶段优化**（轻度睡眠），可以在保持良好用户体验的同时，将电流降到 1-5mA，使用 1000mAh 电池可以续航 200-1000 小时（8-40 天）。
