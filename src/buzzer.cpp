#include "buzzer.h"
#include <Arduino.h>

// ===== 硬件配置 =====
static uint8_t buzzerPin = 39;

// ===== 状态机定义 =====

/**
 * @enum BuzzerState
 * @brief 蜂鸣器状态机的所有状态
 * @details 状态转换流程:
 *          BUZZER_IDLE → BUZZER_ON → BUZZER_OFF → BUZZER_ON → ... → BUZZER_IDLE
 *          (空闲状态)    (鸣叫中)    (间隔中)      (鸣叫中)         (空闲)
 */
typedef enum {
  BUZZER_IDLE,  // 空闲状态：蜂鸣器不工作，等待调用buzzer()启动
  BUZZER_ON,    // 鸣叫状态：蜂鸣器正在发声
  BUZZER_OFF    // 间隔状态：蜂鸣器关闭，等待下次鸣叫
} BuzzerState;

// ===== 状态机变量 =====

static BuzzerState buzzerState     = BUZZER_IDLE;
static uint32_t    stateStartTime  = 0;
static uint8_t     remainingBeeps  = 0;
static uint16_t    currentDuration = 0;
static uint16_t    currentInterval = 0;

// ===== 公共接口函数 =====

/**
 * @brief  初始化蜂鸣器
 */
void buzzerInit() {
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
}

/**
 * @brief  启动蜂鸣器鸣叫（非阻塞）
 * @param  times    鸣叫次数，范围: 1-255
 * @param  duration 每次鸣叫持续时间，单位: 毫秒
 * @param  interval 每次鸣叫间隔时间，单位: 毫秒
 * @details         此函数立即返回，不阻塞任务执行
 *                  蜂鸣器通过buzzerUpdate()在后台自动控制
 */
void buzzer(uint8_t times, uint16_t duration, uint16_t interval) {
  if (times == 0) return;
  
  remainingBeeps  = times;
  currentDuration = duration;
  currentInterval = interval;
  
  if (times == 1) {
    currentInterval = 0;
  }
  
  buzzerState    = BUZZER_ON;
  stateStartTime = millis();
  digitalWrite(buzzerPin, HIGH);
}

/**
 * @brief  蜂鸣器状态机更新函数（非阻塞）
 * @param  pvParameter 任务参数（可传NULL）
 * @details 必须在FreeRTOS任务中持续调用
 */
void buzzerUpdate(void* pvParameter) {
  while (1) {
    if (buzzerState == BUZZER_IDLE) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }
    
    uint32_t currentTime   = millis();
    uint32_t elapsedTime    = currentTime - stateStartTime;
    
    switch (buzzerState) {
      case BUZZER_ON:
        if (elapsedTime >= currentDuration) {
          digitalWrite(buzzerPin, LOW);
          remainingBeeps--;
          
          if (remainingBeeps > 0 && currentInterval > 0) {
            buzzerState    = BUZZER_OFF;
            stateStartTime = currentTime;
          } else {
            buzzerState = BUZZER_IDLE;
          }
        }
        break;
        
      case BUZZER_OFF:
        if (elapsedTime >= currentInterval) {
          buzzerState    = BUZZER_ON;
          stateStartTime = currentTime;
          digitalWrite(buzzerPin, HIGH);
        }
        break;
        
      default:
        buzzerState = BUZZER_IDLE;
        break;
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
