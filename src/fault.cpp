#include "fault.h"
#include "ESPNOW.h"
#include "buzzer.h"
#include "esp_log.h"
#include "led.h"
#include "motor.h"
#include "step.h"

static const char*          TAG              = "FAULT";
volatile bool               isH_BridgeFault  = false;
volatile bool               isChopping       = false;
volatile StepperFaultType_t stepperFaultType = STEPPER_FAULT_NONE; // 步进电机故障类型，默认无故障

TaskHandle_t faultTaskHandle = NULL;

/** DRV8701故障引脚 nFAULT
    当芯片检测到以下故障时此引脚被拉低：
        1、VM 欠压 (UVLO)
        2、电荷泵欠压 (CPUV)
        3、过流 (OCP) —— 包括 VDS 过流或 SP 引脚过压
        4、前置驱动器故障 (PDF) —— 如 GHx/GLx 短路、驱动电流不足
        5、热关断 (TSD)
    恢复：对于 OCP 和 PDF，故障清除后会自动重试；对于 UVLO/CPUV/TSD，条件恢复正常后自动释放

    注：需外部上拉。
*/
#define H_BRIDGE_FAULT 1
static const uint8_t H_BridgeFault_pin = 11;

/** DRV8701斩波检测引脚 SNSOUT
    当内部PWM电流斩波触发时，此引脚被拉低，表示正在限流。触发条件如下：
        1、电流达到设定的斩波阈值ICHOP。
        2、进入 慢衰减（制动）模式 进行固定关断时间 (tOFF) 限流。
    恢复：一旦芯片内部的电流斩波周期结束，该引脚自动恢复（高阻态）。

    注：需外部上拉。
*/
#define SNSOUT_CHOPPING 2
static const uint8_t chop_pin = 10;

/** H桥故障中断处理
 *@brief    H桥故障引脚检测，当H桥发生故障时，引脚状态改变，触发中断处理。
            采用中断+事件通知的方式唤醒任务。
 *@note     如只做翻转标志位的操作，函数内部可简化为：isH_BridgeFault = !isH_BridgeFault;

*/
// 中断处理函数使用IRAM_ATTR宏定义，避免函数被编译到flash中（放在RAM中），提高执行效率
void IRAM_ATTR H_BridgeFault_ISR() {
  // 中断在触发并执行回调函数之后，原则上会返回到中断发生前的代码继续执行
  // 但如果在中断处理过程中有更高优先级的任务需要运行，系统会在中断结束后切换到该任务
  // 所以我们需要使用 xHigherPriorityTaskWoken 来指示是否需要切换任务
  // 先假设没有更高优先级的任务，所以把 xHigherPriorityTaskWoken 初始化为 pdFALSE
  // 如果有更高优先级的任务，系统会把 xHigherPriorityTaskWoken 设置为 pdTRUE，表示需要切换到该任务
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;                                                          // 用于中断处理任务切换，如果有更高优先级的任务需要运行，则切换到该任务（系统自动改为pdTRUE）
  isH_BridgeFault                     = digitalRead(H_BridgeFault_pin) == LOW;                            // 读取引脚状态，更新故障标志位
  xTaskNotifyFromISR(faultTaskHandle, H_BRIDGE_FAULT, eSetValueWithOverwrite, &xHigherPriorityTaskWoken); // 发送通知给 fault_task，通知值为 H_BRIDGE_FAULT，使用 eSetValueWithOverwrite 模式（如果之前有未处理的通知，会被覆盖），并传入 xHigherPriorityTaskWoken 来指示是否需要切换任务
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);                                                           // 切换任务，如果有更高优先级的任务需要运行，则切换到该任务
}

/** TMC2209故障引脚 DIAG
    在不启用StallGuard4功能，当发生以下情况时，DIAG 引脚会被拉高：
            1、短路保护：电机绕组对 GND 短路或对 VS 短路。
            2、过热关断：芯片温度超过安全上限。
            3、欠压：电荷泵电压过低。
    恢复：通过将 ENN 引脚拉高然后再次拉低（或者向芯片的全局状态寄存器 GSTAT 的相应位写入1）可以复位错误状态。

    注：该引脚为推挽模式，无需上下拉。如设置为StallGuard4功能，当电机堵转时，DIAG引脚会输出一个脉冲，即短暂的、有特定时长的高电平信号。
*/
#define TMC2209_FAULT 3
static const uint8_t stepperFault_pin = 48;
volatile bool        isStepperFault   = false;

void IRAM_ATTR stepperFault_ISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  isStepperFault                      = digitalRead(stepperFault_pin) == HIGH; // 读取引脚状态，更新故障标志位
  xTaskNotifyFromISR(faultTaskHandle, TMC2209_FAULT, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/** INA226故障引脚 ALERT
     当发生以下情况时，ALERT 引脚会被拉低：
            1、过流：电流超过设定的过流阈值。
            2、过压：总线和检测电阻电压超过设定的过压阈值。
            3、欠压：总线和检测电阻电压低于设定的欠压阈值。
    恢复：故障报警可设置为透明模式（默认）和锁存模式。透明模式下，故障条件消除后 ALERT 引脚会自动恢复；
          锁存模式下，需要通过 I2C 写入寄存器来清除故障状态并释放 ALERT 引脚。

    注：该引脚为开漏模式，需外部上拉。
*/
#define INA226_FAULT 4
static const uint8_t alertPin      = 8;
volatile bool        isINA226Fault = false;
void IRAM_ATTR       INA226Fault_ISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  isINA226Fault                       = digitalRead(alertPin) == LOW; // 读取引脚状态，更新故障标志位
  xTaskNotifyFromISR(faultTaskHandle, INA226_FAULT, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static uint32_t getModeColor(ControlMode mode) {
  switch (mode) {
  case HAND_MODE:
    return COLOR_GREEN;
  case FOOT_MODE:
    return COLOR_CYAN;
  case CRUISE_MODE:
    return COLOR_YELLOW;
  case STANDBY_MODE:
    return COLOR_RED;
  default:
    return COLOR_WHITE;
  }
}

// 清除 TMC2209 故障标志
void clearTMC2209Fault() {
  stepperEmergencyStop(); // 先紧急停止步进电机，确保安全
  myStepper.GSTAT(GSTAT_DRV_ERR | GSTAT_UV_CP);
  stepperFaultType = STEPPER_FAULT_NONE;
  vTaskDelay(pdMS_TO_TICKS(10)); // 等待50ms，确保故障状态被清除并稳定下
}

void fault_init() {
  pinMode(H_BridgeFault_pin, INPUT); // 已外部上拉
  pinMode(chop_pin, INPUT);          // 已外部上拉
  pinMode(stepperFault_pin, INPUT);  // 推挽输出，无需上拉
  pinMode(alertPin, INPUT);

  attachInterrupt(digitalPinToInterrupt(H_BridgeFault_pin), H_BridgeFault_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(stepperFault_pin), stepperFault_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(alertPin), INA226Fault_ISR, CHANGE);
}

/**
 * @brief 诊断 TMC2209 故障（通过 UART 读取寄存器）
 * @note  忽略复位标志（GSTAT_RESET），因为 FastAccelStepper 会频繁复位 TMC2209 来实现无声启动
 *        这会导致 GSTAT_RESET 位频繁被置位，可能会误判为严重故障。
 * @return true  表示存在严重故障（过热、短路等）
 * @return false 表示无严重故障或仅有欠压/复位等轻微问题
 */
static bool diagnoseTMC2209Fault() {
  uint8_t gstat   = myStepper.GSTAT();
  bool    serious = false;
  // 欠压（电荷泵欠压）
  if (gstat & GSTAT_UV_CP) {
    ESP_LOGW(TAG, "TMC2209 欠压 (UV_CP)");
  }
  // 驱动器错误（短路/过热）
  if (gstat & GSTAT_DRV_ERR) {
    serious = true;
    ESP_LOGE(TAG, "TMC2209 驱动器错误 (DRV_ERR)");
    uint32_t drv_status = myStepper.DRV_STATUS();
    ESP_LOGI(TAG, "DRV_STATUS: 0x%08X", drv_status);
    // 过热（OT 位）
    if (drv_status & (1 << 1)) {
      ESP_LOGE(TAG, "→ 过热 (OT)");
    }
    // 短路至地（S2G 位）
    if (drv_status & (1 << 2)) {
      ESP_LOGE(TAG, "→ 短路 (S2G)");
    }
    // 可以根据数据手册补充其他位，如过流、开路等
  }
  return serious;
}

void fault_task(void* pvParameters) {
  uint32_t notifiedValue;
  while (1) {
    // 阻塞等待通知，收到后通知值存入 notifiedValue
    xTaskNotifyWait(0, 0, &notifiedValue, portMAX_DELAY);
    // 根据通知值判断故障源
    switch (notifiedValue) {
    case H_BRIDGE_FAULT:
      if (isH_BridgeFault) {
        ESP_LOGE(TAG, "DRV8701报错!");
        motorEmergencyStop(); // 立即停止电机
        buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        ledSetMode(modeRGB, LED_BLINK, COLOR_RED, SHORT_FLASH_DURATION, SHORT_FLASH_INTERVAL); // H桥故障时，模式灯闪烁红色
      } else {
        ESP_LOGI(TAG, "DRV8701故障已清除");
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        // 故障清除后恢复模式灯
        ControlMode currentMode = getCurrentCtrlMode();
        ledSetMode(modeRGB, LED_ON, getModeColor(currentMode), 0, 0);
      }
      break;
    case TMC2209_FAULT:
      if (isStepperFault) {
        diagnoseTMC2209Fault(); // 进一步诊断故障类型（过热、短路等）
        ESP_LOGE(TAG, "TMC2209报错!");
        stepperEmergencyStop();
        buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        ledSetMode(sysRGB, LED_BLINK, COLOR_RED, SHORT_FLASH_DURATION, SHORT_FLASH_INTERVAL); // TMC2209故障时，系统灯闪烁红色
      } else {
        ESP_LOGI(TAG, "TMC2209故障已清除");
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
      }
      break;
    case INA226_FAULT:
      if (isINA226Fault) {
        ESP_LOGE(TAG, "INA226报错!");
        buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        ledSetMode(sysRGB, LED_BLINK, COLOR_YELLOW, SHORT_FLASH_DURATION, SHORT_FLASH_INTERVAL);
      } else {
        ESP_LOGI(TAG, "INA226故障已清除");
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
      }
    default:
      break;
    }
  }
}

void onChopping(bool enable) {
  if (enable) {
    isChopping = digitalRead(chop_pin) == LOW;
    if (isChopping) {
      ESP_LOGW(TAG, "电流斩波触发，正在限流...");
      ledSetMode(sysRGB, LED_BLINK, COLOR_WHITE, SHORT_FLASH_DURATION, SHORT_FLASH_INTERVAL); // 斩波触发时，系统灯闪烁白色
    }
  }
}