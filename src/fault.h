#pragma once

#include <Arduino.h>

#define TMC2209

extern volatile bool isH_BridgeFault, isChopping, isStepperFault, isINA226Fault;

extern TaskHandle_t faultTaskHandle;

void fault_init();
void fault_task(void* pvParameters);