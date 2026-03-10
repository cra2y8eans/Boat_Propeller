#pragma once

#include "Arduino.h"
#include "WiFi.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern QueueHandle_t control_mode_Queue;

enum ContorlMode {
  HAND_MODE,    // 手动模式
  FOOT_MODE,    // 脚控模式
  CRUISE_MODE,  // 巡航模式
  STANDBY_MODE, // 待机模式
};
extern ContorlMode current_ctrl_mode, last_ctrl_mode;


