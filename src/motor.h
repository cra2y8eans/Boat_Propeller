#pragma once

#include <Arduino.h>

enum ControlMode {
  HAND_MODE,    // 手动模式
  FOOT_MODE,    // 脚控模式
  CRUISE_MODE,  // 巡航模式
  STANDBY_MODE, // 待机模式
};

void        motorInit();
ControlMode getCurrentCtrlMode(); // 获取当前控制模式
void        motorControl(void* pvParameters);
void        modeIdentify(void* pvParameters);