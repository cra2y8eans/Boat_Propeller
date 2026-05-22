#pragma once

#include "Arduino.h"
#include "WiFi.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

struct RecvFromFootPad_t {
  uint16_t speed;
  bool     data[6] = { }; // 0、左转，1、右转，2、电推，3、功能，4、正在充电，5、电池已满
  float    batVoltage, batPercentage, footPadChipTemp;
};

extern volatile bool isFootPadOnline;

void              esp_now_setup();
void              esp_now_connection_check(void* pvParameters);
void              dataSent(void* pvParameters);
RecvFromFootPad_t getFootPadData();
