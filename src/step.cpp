#include "step.h"
#include "esp_log.h"

#define AUTO_DISABLE_DELAY 60000 // 超时休眠，单位毫秒

// static portMUX_TYPE  step_Mux          = portMUX_INITIALIZER_UNLOCKED; // 定义临界区变量
static const char*   TAG               = "stepper"; // 日志标签
static const uint8_t step_dir          = 14;        // 步进方向引脚
static const uint8_t step_on           = 47;        // 步进引脚
static const uint8_t step_en           = 35;        // 步进使能引脚
static unsigned long lastOperationTime = 0;         // 上一次操作时间
static bool          isSleeped         = true;      // 电机是否休眠；默认休眠。休眠时step_en为高电平

void stepper_init() {
  pinMode(step_dir, OUTPUT);
  pinMode(step_on, OUTPUT);
  pinMode(step_en, OUTPUT);
  digitalWrite(step_en, HIGH);
}

// 将速度等级转换为微秒数
uint16_t speedToMicroseconds(uint8_t level) {
  return 1000 + (level - 1) * 750;
}

/**  步进脉冲
 * @brief     步进脉冲函数
 * @param     stepSpeed: 步进速度，单位微秒
 * @note      脉冲持续时间取值范围4000到1000，值越大速度越慢
 */
static void stepper_pulse(int stepSpeed) {
  digitalWrite(step_on, HIGH);
  delayMicroseconds(stepSpeed);
  digitalWrite(step_on, LOW);
  delayMicroseconds(stepSpeed);
}

// 统一的步进电机控制函数
void stepper_control(bool turnLeft, bool turnRight, bool dirReverse, int stepSpeed) {
  if (!turnLeft && turnRight) {
    // 右转
    if (isSleeped) digitalWrite(step_en, LOW); // 如果电机处于休眠状态，则先唤醒
    digitalWrite(step_dir, dirReverse ? LOW : HIGH);
    lastOperationTime = millis();
    stepper_pulse(stepSpeed);
  } else if (turnLeft && !turnRight) {
    // 左转
    if (isSleeped) digitalWrite(step_en, LOW);
    digitalWrite(step_dir, dirReverse ? HIGH : LOW);
    lastOperationTime = millis();
    stepper_pulse(stepSpeed);
  }

  // 自动休眠检查
  if (millis() - lastOperationTime > AUTO_DISABLE_DELAY) {
    digitalWrite(step_en, HIGH);
    isSleeped = true;
    ESP_LOGI(TAG, "步进电机自动休眠");
  } else {
    isSleeped = false;
    digitalWrite(step_en, LOW);
  }
}

// void stepper_control_task(void* pvParameter) {
//   while (1) {
//     ControlMode mode;
//     bool        turnLeft, turnRight;
//     uint16_t    stepSpeedRaw;
//     taskENTER_CRITICAL(&step_Mux);
//     mode         = getCurrentCtrlMode();
//     turnLeft     = FootPadData.data[0]; // 0、左转，1、右转，2、电推，3、功能
//     turnRight    = FootPadData.data[1];
//     stepSpeedRaw = stepSpeed; // 按钮原值
//     taskEXIT_CRITICAL(&step_Mux);
//   }
// }