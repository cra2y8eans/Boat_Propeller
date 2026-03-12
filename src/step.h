#pragma once

#include "Arduino.h"

void     stepper_init();
uint16_t speedToMicroseconds(uint8_t level);
void     stepper_control(bool turnLeft, bool turnRight, bool dirReverse, int stepSpeed);