#pragma once

#include "INA226.h"
#include <Arduino.h>

void  ina226_init();
float getCurrentMA();
float getPowerMW();
float getBusVoltageMV();
void  ina226_task(void* pvParameters);
