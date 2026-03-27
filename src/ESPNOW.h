#pragma once

#include "Arduino.h"
#include "WiFi.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

struct RecvFromFootPad_t {
  uint16_t speed;
  bool     data[4] = { }; // 0、左转，1、右转，2、电推，3、功能
  float    batVoltage, batPercentage, footPadChipTemp;
};
extern volatile RecvFromFootPad_t FootPadData;
// 定义临界区变量
extern portMUX_TYPE  esp_now_Mux;
extern volatile bool isDebugDeviceOnline, isFootPadOnline;

void esp_now_setup();
void esp_now_connection_check(void* pvParameters);
void dataSent(void* pvParameters);
