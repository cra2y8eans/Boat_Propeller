#pragma once

#include <Arduino.h>

void temperatureRead(void* pvParameters);
float getPCBtemp();
float getHighMosTemp();
float getLowMosTemp();