#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// 按钮标识
#define ACCEL_BUTTON 1
#define DECEL_BUTTON 2
#define FORWARD_MAX_SPEED 5
#define FORWARD_MIN_SPEED 1
#define REVERSE_MAX_SPEED -5
#define REVERSE_MIN_SPEED -1
#define STEP_MAX_SPEED 5
#define STEP_MIN_SPEED 1

// 事件类型
#define BUTTON_EVENT_SHORT_PRESS 1
#define BUTTON_EVENT_LONG_PRESS 2

void    buttonInit();
void    buttonTask(void* pvParameters);
bool    getAccelLongPressed();
bool    getDecelLongPressed();
int8_t  getMotorSpeed();
uint8_t getStepSpeed();