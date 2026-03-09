#pragma once

#include <Arduino.h>

#define LONG_BEEP_DURATION 1000 // 长鸣持续时间(ms)
#define SHORT_BEEP_DURATION 200 // 短鸣持续时间(ms)
#define LONG_BEEP_INTERVAL 300  // 长鸣间隔时间(ms)
#define SHORT_BEEP_INTERVAL 100 // 短鸣间隔时间(ms)

void buzzer(uint8_t times, uint16_t duration, uint16_t interval);
void buzzerUpdate(void* pvParameter); // 非阻塞更新函数，需要在loop中调用
