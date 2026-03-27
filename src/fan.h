#pragma once

#include <Arduino.h>

void  Fan_Init();
float getFanChanSpeed();
float getFanHeatSpeed();
void  Fan_task(void* pvParameters);