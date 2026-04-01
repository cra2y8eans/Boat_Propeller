#include "step.h"
#include "ESPNOW.h"
#include "button.h"
#include "esp_log.h"
#include "fault.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "motor.h"

#define ARDUINO
// #define PIO
#define AUTO_DISABLE_DELAY 60000 // 超时休眠，单位毫秒

static portMUX_TYPE  step_Mux          = portMUX_INITIALIZER_UNLOCKED; // 定义临界区变量
static const char*   TAG               = "stepper";                    // 日志标签
static const uint8_t step_dir          = 14;                           // 步进方向引脚
static const uint8_t step_on           = 47;                           // 步进引脚
static const uint8_t step_en           = 35;                           // 步进使能引脚
static unsigned long lastOperationTime = 0;                            // 上一次操作时间
static bool          isSleeped         = true;                         // 电机是否休眠；默认休眠。休眠时step_en为高电平

void stepper_init() {
  pinMode(step_dir, OUTPUT);
  pinMode(step_on, OUTPUT);
  pinMode(step_en, OUTPUT);
  digitalWrite(step_en, HIGH);
}

// 将速度等级转换为微秒数
static uint16_t speedToMicroseconds(uint8_t level) {
  return 1000 + (level - 1) * 750;
}

void stepperEmergencyStop() {
  digitalWrite(step_en, HIGH);
  digitalWrite(step_on, LOW);
  isSleeped = true;
#ifdef ARDUINO
  Serial.println("步进电机紧急停止");
#elif defined(PIO)
  ESP_LOGW(TAG, "步进电机紧急停止");
#endif
}

void stepper_control_task(void* pvParameter) {
  uint32_t         nextToggleTime = 0;     // 下一次引脚切换时间（微秒）
  bool             pinState       = false; // 当前 step_on 引脚电平
  bool             pulseActive    = false; // 是否正在产生脉冲序列
  uint32_t         pulseInterval  = 2000;  // 默认脉冲间隔（微秒）
  TickType_t       xLastWakeTime  = xTaskGetTickCount();
  const TickType_t xPeriod        = pdMS_TO_TICKS(1); // 单位ms，换算为频率： 1000Hz → 周期为 1000次/秒
  uint32_t         lastCheck      = 0;
  while (1) {
    // 每 1000 次循环或每 5 秒检查一次栈水位
    if (millis() - lastCheck > 5000) {
      UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(NULL);
#ifdef ARDUINO
      Serial.printf("步进电机控制任务 Stack left: %d\n", stackHighWater);
#elif defined(PIO)
      ESP_LOGI(TAG, "Stack left: %d", stackHighWater);
#endif
      lastCheck = millis();
    }
    // 如果步进电机故障，则等待1秒后重试
    if (isStepperFault) {
      vTaskDelayUntil(&xLastWakeTime, xPeriod);
      continue;
    }
    uint32_t now = micros();
    // 读取共享数据（临界区保护）
    taskENTER_CRITICAL(&step_Mux);
    ControlMode mode       = getCurrentCtrlMode();
    bool        turnLeft   = FootPadData.data[0];
    bool        turnRight  = FootPadData.data[1];
    bool        dirReverse = isAccelButtonLongPressed; // 方向翻转标志
    uint8_t     speedLevel = stepSpeed;                // 步进速度档位（1~5）
    taskEXIT_CRITICAL(&step_Mux);
    // 计算脉冲间隔（微秒）
    pulseInterval = speedToMicroseconds(speedLevel);
    // 判断是否需要工作（仅脚控/巡航模式且有转向命令）
    bool needWork = (mode == FOOT_MODE || mode == CRUISE_MODE) && (turnLeft || turnRight);
    if (needWork) {
      // 唤醒电机（使能低电平有效）
      if (isSleeped) {
        digitalWrite(step_en, LOW);
        isSleeped         = false;
        lastOperationTime = millis();
      }
      // 设置方向（根据 dirReverse 翻转）
      if (turnLeft && !turnRight) {
        digitalWrite(step_dir, dirReverse ? HIGH : LOW);
      } else if (turnRight && !turnLeft) {
        digitalWrite(step_dir, dirReverse ? LOW : HIGH);
      }
      // 非阻塞脉冲产生
      if (!pulseActive) {
        // 开始一个新脉冲：拉高
        digitalWrite(step_on, HIGH);
        pinState       = true;
        nextToggleTime = now + pulseInterval;
        pulseActive    = true;
      } else {
        if (pinState && (now >= nextToggleTime)) {
          // 高电平结束，拉低
          digitalWrite(step_on, LOW);
          pinState       = false;
          nextToggleTime = now + pulseInterval;
        } else if (!pinState && (now >= nextToggleTime)) {
          // 低电平结束，开始下一个脉冲
          digitalWrite(step_on, HIGH);
          pinState       = true;
          nextToggleTime = now + pulseInterval;
        }
      }
      lastOperationTime = millis();
    } else {
      // 无转向命令，停止脉冲并确保引脚为低
      if (pulseActive) {
        digitalWrite(step_on, LOW);
        pinState    = false;
        pulseActive = false;
      }
      // 休眠检查
      if (!isSleeped && (millis() - lastOperationTime > AUTO_DISABLE_DELAY)) {
        digitalWrite(step_en, HIGH);
        isSleeped = true;
#ifdef ARDUINO
        Serial.println("步进电机自动休眠");
#elif defined(PIO)
        ESP_LOGI(TAG, "步进电机自动休眠");
#endif
      }
    }
    // 任务延时1ms，让出CPU（使用宏确保可移植）
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}