#pragma once

#include "Arduino.h"
#include <TMCStepper.h>

extern TMC2209Stepper myStepper;

void     stepper_init();
uint16_t getSG_RESULT();
uint16_t getStepCurrent();
void     setStealthChopMode(bool enable);
void stepperEmergencyStop();
bool     getStealthChopMode();
void     setStepCurrent(uint16_t current_mA);
uint16_t getStepCurrentSetting();
void     stepper_control_task(void* pvParameter);
