#pragma once

#include <Arduino.h>

void  NTC_Init();
void  temperatureRead(void* pvParameters);
float getPCBtemp();
float getHighMosTemp();
float getLowMosTemp();