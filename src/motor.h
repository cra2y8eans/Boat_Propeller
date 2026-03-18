#pragma once

#include <Arduino.h>

enum ControlMode {
  HAND_MODE,    // 手动模式
  FOOT_MODE,    // 脚控模式
  CRUISE_MODE,  // 巡航模式
  STANDBY_MODE, // 待机模式
};

extern volatile uint8_t stepSpeed;

void        motorInit();
void        modeIdentify(void* pvParameters);
void        motorControl(void* pvParameters);
ControlMode getCurrentCtrlMode(); // 获取当前控制模式
uint8_t     getStepSpeed();       // 获取当前步进电机转速
void        motorEmergencyStop();
