#pragma once

#include "Arduino.h"

void stepper_init();
void stepper_control_task(void* pvParameter);