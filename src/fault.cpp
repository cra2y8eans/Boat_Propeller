#include "fault.h"
#include "buzzer.h"
#include "esp_log.h"
#include "led.h"
#include "motor.h"
#include "step.h"

static const char* TAG = "FAULT";

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
  BaseType_t xHigherPriorityTaskWoken = pdFALSE; // 用于中断处理任务切换，如果有更高优先级的任务需要运行，则切换到该任务（系统自动改为pdTRUE）
  isH_BridgeFault                     = !isH_BridgeFault;
  xTaskNotifyFromISR(faultTaskHandle, H_BRIDGE_FAULT, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken); // 切换任务，如果有更高优先级的任务需要运行，则切换到该任务
}

void IRAM_ATTR chop_ISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  isChopping                          = !isChopping;
  xTaskNotifyFromISR(faultTaskHandle, SNSOUT_CHOPPING, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

#ifdef TMC2209
/** TMC2209故障引脚 DIAG
    在不启用StallGuard4功能，当发生以下情况时，DIAG 引脚会被拉高：
            1、短路保护：电机绕组对 GND 短路或对 VS 短路。
            2、过热关断：芯片温度超过安全上限。
            3、欠压：电荷泵电压过低。
    恢复：通过将 ENN 引脚拉高然后再次拉低，可以复位错误状态。

    注：该引脚为推挽模式，无需上下拉。如设置为StallGuard4功能，当电机堵转时，DIAG引脚会输出一个脉冲，即短暂的、有特定时长的高电平信号。
*/
#define TMC2209_FAULT 3
static const uint8_t stepperFault_pin = 48;
volatile bool        isStepperFault   = false;

void IRAM_ATTR stepperFault_ISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  isStepperFault                      = !isStepperFault;
  xTaskNotifyFromISR(faultTaskHandle, TMC2209_FAULT, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
#elif defined(DRV8872)
/** DRV8872故障引脚 nFAULT
    nFAULT 引脚会在以下三种保护电路被触发时变为低电平：
            1、VM 欠压锁定 （UVLO）：电源电压低于欠压锁定阈值。
            2、过流保护 （OCP）：输出电流超过过流保护阈值。
            3、热关断 （TSD）：芯片内部结温超过安全限值（约175°C）。
    该芯片具有自动故障恢复功能。当故障条件被移除后，芯片会自动恢复正常工作，同时 nFAULT 引脚会释放（即回到高阻抗状态，由外部上拉电阻拉高）。

    注：该引脚为开漏输出，需外部上拉。
*/
#define DRV8872_FAULT 4
static const uint8_t drv8872_Fault_pin = 17; // DRV8872 故障引脚
volatile bool        isDrv8872Fault    = false;
void IRAM_ATTR       drv8872_Fault_ISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  isDrv8872Fault                      = !isDrv8872Fault;
  xTaskNotifyFromISR(faultTaskHandle, DRV8872_FAULT, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
#endif

volatile bool isH_BridgeFault = false;
volatile bool isChopping      = false;

TaskHandle_t faultTaskHandle = NULL;

void fault_init() {
  pinMode(H_BridgeFault_pin, INPUT); // 已外部上拉
  pinMode(chop_pin, INPUT);          // 已外部上拉
  attachInterrupt(digitalPinToInterrupt(H_BridgeFault_pin), H_BridgeFault_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(chop_pin), chop_ISR, CHANGE);
#ifdef TMC2209
  pinMode(stepperFault_pin, INPUT); // 推挽输出，无需外部上下拉
  attachInterrupt(digitalPinToInterrupt(stepperFault_pin), stepperFault_ISR, CHANGE);
#elif defined(DRV8872)
  pinMode(drv8872_Fault_pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(drv8872_Fault_pin), drv8872_Fault_ISR, CHANGE);
#endif
}

void fault_task(void* pvParameters) {
  uint32_t notifiedValue;
  uint32_t lastCheck = 0;
  while (1) {
    // 每 1000 次循环或每 5 秒检查一次栈水位
    if (millis() - lastCheck > 5000) {
      UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(NULL);
      ESP_LOGI(TAG, "Stack left: %d words", stackHighWater);
      lastCheck = millis();
    }
    // 阻塞等待通知，收到后通知值存入 notifiedValue
    xTaskNotifyWait(0, 0, &notifiedValue, portMAX_DELAY);

    // 根据通知值判断故障源
    switch (notifiedValue) {
    case H_BRIDGE_FAULT:
      if (isH_BridgeFault) {
        ESP_LOGI(TAG, "DRV8701报错!");
        motorEmergencyStop(); // 立即停止电机
        buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        ledSetMode(sysRGB, LED_BLINK, COLOR_RED, 200, 200);
      } else {
        // 故障已清除，恢复正常状态
        ESP_LOGI(TAG, "DRV8701故障已清除");
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
      }
      break;
    case SNSOUT_CHOPPING:
      if (isChopping) {
        ESP_LOGE(TAG, "正在斩波!");
        buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        ledSetMode(sysRGB, LED_BLINK, COLOR_YELLOW, 200, 200);
      } else {
        // 斩波已停止，恢复正常状态
        ESP_LOGI(TAG, "斩波已停止");
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
      }
      break;
    case TMC2209_FAULT:
      if (isStepperFault) {
        ESP_LOGE(TAG, "TMC2209报错!");
        stepperEmergencyStop();
        buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        ledSetMode(sysRGB, LED_BLINK, COLOR_RED, 200, 200);
      } else {
        ESP_LOGI(TAG, "TMC2209故障已清除");
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
      }
      break;
    default:
      break;
    }
  }
}