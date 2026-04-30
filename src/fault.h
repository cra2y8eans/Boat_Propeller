#pragma once
#include <Arduino.h>

// GSTAT寄存器的DRV_ERR位，表示TMC2209发生过故障（如过热、过流等）。
// 读取该寄存器可以检查是否有未处理的故障状态。写入1可以清除该故障状态。
#define GSTAT_DRV_ERR 0x02

// GSTAT寄存器的UV_CP位，表示电荷泵电压过低。
// 当电荷泵电压不足时，TMC2209可能无法正常驱动步进电机，导致性能下降或失步。写入1可以清除该故障状态。
#define GSTAT_UV_CP   0x04

// 步进电机故障类型枚举
typedef enum {
    STEPPER_FAULT_NONE = 0,
    STEPPER_FAULT_OVERTEMP,
    STEPPER_FAULT_SHORT_CIRCUIT,
    STEPPER_FAULT_UV_CP,
    STEPPER_FAULT_DRV_ERR_UNKNOWN,
} StepperFaultType_t;

extern volatile bool isH_BridgeFault, isChopping, isStepperFault, isINA226Fault;
extern volatile StepperFaultType_t stepperFaultType;
extern TaskHandle_t faultTaskHandle;

void fault_init();
void fault_task(void* pvParameters);
void onChopping(bool enable);
void clearTMC2209Fault();   // 清除 TMC2209 GSTAT 故障标志