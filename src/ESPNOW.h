#pragma once

#include "Arduino.h"
#include "WiFi.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

struct RecvFromFootPad_t {
  uint16_t speed;
  bool     data[4] = {}; // 0、左转，1、右转，2、电推，3、功能
  float    batVoltage;
};
extern volatile RecvFromFootPad_t FootPadData;

extern volatile bool isDebugDeviceOnline, isFootPadOnline;

void esp_now_setup();
void esp_now_connection_check(void* pvParameters);
void dataSent(void* pvParameters);
