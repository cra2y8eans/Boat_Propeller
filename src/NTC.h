#pragma once

#include <Arduino.h>

void  NTC_Init();
void  NTC_task(void* pvParameters);
float getPCBtemp();
float getHighMosTemp();
float getLowMosTemp();
float getChipTemp();