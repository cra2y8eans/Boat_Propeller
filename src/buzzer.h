#pragma once

#include <Arduino.h>

// ===== 常量定义 =====
#define LONG_BEEP_DURATION  1000  // 长鸣持续时间(ms)
#define SHORT_BEEP_DURATION 200   // 短鸣持续时间(ms)
#define LONG_BEEP_INTERVAL  300   // 长鸣间隔时间(ms)
#define SHORT_BEEP_INTERVAL 100   // 短鸣间隔时间(ms)

// ===== 公共接口 =====
void buzzerInit();

/**
 * @brief 启动蜂鸣器鸣叫（非阻塞）
 * @param times    鸣叫次数 (1-255)
 * @param duration 持续时间(ms)
 * @param interval 间隔时间(ms)
 */
void buzzer(uint8_t times, uint16_t duration, uint16_t interval);

/**
 * @brief 蜂鸣器状态机更新函数（非阻塞）
 * @param pvParameter 任务参数（可传NULL）
 * @note 必须在FreeRTOS任务中持续调用
 */
void buzzerUpdate(void* pvParameter);