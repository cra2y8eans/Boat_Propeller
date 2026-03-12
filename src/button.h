#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// 按钮标识
#define ACCEL_BUTTON 1
#define DECEL_BUTTON 2

// 事件类型
#define BUTTON_EVENT_SHORT_PRESS 1
#define BUTTON_EVENT_LONG_PRESS 2

extern volatile bool isAccelButtonLongPressed;
extern volatile bool isDecelButtonLongPressed;

// 事件结构体
struct ButtonEvent_t {
  uint8_t button; // ACCEL_BUTTON 或 DECEL_BUTTON
  uint8_t event;  // 短按或长按
};

extern QueueHandle_t buttonQueue;

void buttonInit();
void buttonTask(void* pvParameters);