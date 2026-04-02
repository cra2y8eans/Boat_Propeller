#pragma once

#include <Arduino.h>

#define TMC2209

extern volatile bool isH_BridgeFault, isChopping;

#ifdef TMC2209
extern volatile bool isStepperFault;
#elif defined(DRV8872)
extern volatile bool isDrv8872Fault;
#endif

extern TaskHandle_t faultTaskHandle;

void fault_init();
void fault_task(void* pvParameters);