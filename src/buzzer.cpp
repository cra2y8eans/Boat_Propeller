#include "buzzer.h"
#include <Arduino.h>

static uint8_t buzzerPin = 39;

/**
 * @enum    状态机定义
 * @brief   蜂鸣器状态机的所有状态
 * @details 状态转换流程:
 *          BUZZER_IDLE → BUZZER_ON → BUZZER_OFF → BUZZER_ON → ... → BUZZER_IDLE
 *          (空闲状态)    (鸣叫中)    (间隔中)      (鸣叫中)         (空闲)
 */
typedef enum {
  BUZZER_IDLE, // 空闲状态：蜂鸣器不工作，等待调用buzzer()启动
  BUZZER_ON,   // 鸣叫状态：蜂鸣器正在发声
  BUZZER_OFF   // 间隔状态：蜂鸣器关闭，等待下次鸣叫
} BuzzerState;
static BuzzerState buzzerState = BUZZER_IDLE;

static uint32_t stateStartTime  = 0; // 记录进入当前状态时的millis()值，用于计算持续时间
static uint8_t  remainingBeeps  = 0; // 每次鸣叫后递减，减到0时完成整个鸣叫序列
static uint16_t currentDuration = 0; // 每次鸣叫时HIGH电平保持的时间
static uint16_t currentInterval = 0; // 两次鸣叫之间LOW电平保持的时间

void buzzerInit() {
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW); // 初始化为低电平(关闭蜂鸣器)
}

/**
 * @brief  启动蜂鸣器鸣叫（非阻塞）
 * @param  times    鸣叫次数，范围: 1-255
 * @param  duration 每次鸣叫持续时间，单位: 毫秒
 * @param  interval 每次鸣叫间隔时间，单位: 毫秒
 * @details         此函数立即返回，不阻塞任务执行
 *                  蜂鸣器通过buzzerUpdate()在后台自动控制
 * @note            如果只鸣叫1次，interval参数会被忽略
 * @note            重复调用会覆盖之前的鸣叫序列
 */
void buzzer(uint8_t times, uint16_t duration, uint16_t interval) {
  // 参数检查：次数为0直接返回
  if (times == 0) return;
  // 保存鸣叫参数
  remainingBeeps  = times;    // 设置剩余鸣叫次数
  currentDuration = duration; // 保存鸣叫持续时间
  currentInterval = interval; // 保存间隔时间
  // 优化：如果只鸣叫1次，不需要间隔时间
  if (times == 1) {
    currentInterval = 0;
  }
  // 启动状态机
  buzzerState    = BUZZER_ON;    // 进入鸣叫状态
  stateStartTime = millis();     // 记录状态开始时间
  digitalWrite(buzzerPin, HIGH); // 打开蜂鸣器
  // 函数立即返回，不阻塞
}

/**
 * @brief   蜂鸣器状态机更新函数（非阻塞）
 * @details 必须在主循环或专用任务中定期调用
 *          检查时间并根据状态切换蜂鸣器状态
 * @note    调用频率建议: 10-50ms
 * @note    如果不调用此函数，蜂鸣器将无法工作
 */
void buzzerUpdate(void* pvParameter) {
  while (1) {
    if (buzzerState == BUZZER_IDLE) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue; // 如果处于空闲状态，无需处理
    }
    uint32_t currentTime = millis();                     // 获取当前时间
    uint32_t elapsedTime = currentTime - stateStartTime; // 计算当前状态持续的时间
    // 状态机处理
    switch (buzzerState) {
    // ===== 鸣叫状态 =====
    case BUZZER_ON: {
      if (elapsedTime >= currentDuration) { // 检查鸣叫持续时间是否到达
        digitalWrite(buzzerPin, LOW);       // 关闭蜂鸣器
        remainingBeeps--;                   // 减少剩余鸣叫次数
        // 判断是否还有剩余鸣叫
        if (remainingBeeps > 0 && currentInterval > 0) {
          buzzerState    = BUZZER_OFF;  // 还有剩余鸣叫且需要间隔 → 进入间隔状态
          stateStartTime = currentTime; // 重置计时器
        } else {
          buzzerState = BUZZER_IDLE; // 没有剩余鸣叫或不需要间隔 → 完成整个序列
        }
      }
      break;
    }
    // ===== 间隔状态 =====
    case BUZZER_OFF: {
      // 检查间隔时间是否到达
      if (elapsedTime >= currentInterval) {
        // 间隔结束 → 再次鸣叫
        buzzerState    = BUZZER_ON;
        stateStartTime = currentTime;  // 重置计时器
        digitalWrite(buzzerPin, HIGH); // 打开蜂鸣器
      }
      break;
    }
    // ===== 默认处理 =====
    default:
      buzzerState = BUZZER_IDLE; // 未知状态 → 强制返回空闲
      break;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}