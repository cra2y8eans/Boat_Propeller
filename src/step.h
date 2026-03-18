#pragma once

#include "Arduino.h"

void stepper_init();
void stepperEmergencyStop();
void stepper_control_task(void* pvParameter);