#pragma once

#include "ina226.h"
#include <Arduino.h>

float getCurrentMA();
float getPowerMW();
float getBusVoltageMV();
void  ina226_task(void* pvParameters);
void  ina226_print_task(void* pvParameters);
