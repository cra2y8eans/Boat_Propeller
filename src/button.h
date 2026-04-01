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

void    buttonInit();
int8_t  getMotorSpeed();
uint8_t getStepSpeed();
void    buttonTask(void* pvParameters);