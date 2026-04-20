#pragma once

#include <Arduino.h>

void  Fan_Init();
bool  getFanChanState();      // 返回通道风扇是否开启（true=开，false=关）
float getFanHeatSpeed();      // 返回散热风扇速度百分比
void  Fan_task(void* pvParameters);