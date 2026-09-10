#include "motor.h"
#include "ESPNOW.h"
#include "button.h"
#include "buzzer.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "fault.h"
#include "led.h"
#include <Arduino.h>

#define SPEED_STEP 2             // 每次调整的速度步长，值越大加速越快
#define SPEED_SCALE_FACTOR 50    // 减速比例系数（用于智能减速），分母，值越小减速越快
#define DIR_SWITCH_PAUSE_MS 200  // 换向停顿的时间（ms）
#define SWITCH_DEBOUNCE_DELAY 20 // 按键消抖延时，单位毫秒
#define PWM_MIN_DUTY 50          // 最小档位对应的PWM值（1档）
#define PWM_MAX_DUTY 255         // 最大档位对应的PWM值（5档/3档）

static portMUX_TYPE   motor_mutex       = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE   footpad_mutex     = portMUX_INITIALIZER_UNLOCKED; // 定义临界区变量
static const char*    TAG               = "motor";                      // 日志标签
static const uint8_t  on_foot_pin       = 1;                            // 模式引脚
static const uint8_t  on_hand_pin       = 2;                            // 模式引脚
static const uint8_t  motor_pin         = 9;                            // 电机引脚
static const uint8_t  dir_pin           = 18;                           // 转向引脚
static const uint8_t  sleepPin          = 16;                           // 睡眠引脚
static const uint8_t  motor_channel     = 4;                            // 电机PWM通道
static const uint8_t  resolution        = 8;                            // 电机PWM精度
static const uint16_t frequency         = 15000;                        // 电机频率
static int            target_speed      = 0;                            // 目标速度
static int            current_speed     = 0;                            // 当前速度
static bool           motor_move        = false;                        // 电机是否移动标志位
static bool           current_dir       = false;                        // 当前方向标志位，代表当前电机旋转方向，用来跟之前的作比较
static ControlMode    current_ctrl_mode = STANDBY_MODE;                 // 默认为待机模式
static ControlMode    last_ctrl_mode    = STANDBY_MODE;                 // 默认为待机模式

TaskHandle_t modeHandle = NULL;

void IRAM_ATTR modeChange_ISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(modeHandle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

ControlMode readModeWhenSystemStart() {
  int readHand_1 = digitalRead(on_hand_pin);
  int readFoot_1 = digitalRead(on_foot_pin);
  if (readHand_1 == LOW && readFoot_1 == HIGH) return HAND_MODE;   // 手控模式
  if (readHand_1 == LOW && readFoot_1 == LOW) return FOOT_MODE;    // 脚控模式
  if (readHand_1 == HIGH && readFoot_1 == LOW) return CRUISE_MODE; // 巡航模式

  return HAND_MODE; // 默认返回手动模式
}

// 消抖读取当前模式
ControlMode readCurrentModeWithDebounce() {
  int readHand_1 = digitalRead(on_hand_pin);
  int readFoot_1 = digitalRead(on_foot_pin);
  vTaskDelay(pdMS_TO_TICKS(SWITCH_DEBOUNCE_DELAY)); // 延时20ms，消抖
  int readHand_2 = digitalRead(on_hand_pin);
  int readFoot_2 = digitalRead(on_foot_pin);

  if (readHand_1 != readHand_2 || readFoot_1 != readFoot_2) return last_ctrl_mode; // 如果两次读取的值不一样，说明有抖动，返回上次的模式
  if (readHand_1 == LOW && readFoot_1 == HIGH) return HAND_MODE;                   // 手控模式
  if (readHand_1 == LOW && readFoot_1 == LOW) return FOOT_MODE;                    // 脚控模式
  if (readHand_1 == HIGH && readFoot_1 == LOW) return CRUISE_MODE;                 // 巡航模式

  return HAND_MODE; // 默认返回手动模式
}

void modeChangeOperation(ControlMode newMode) {
  switch (newMode) {
  case HAND_MODE: // 手控模式。该模式下电推转速由按钮控制。步进电机不工作。
    ESP_LOGI(TAG, "手控模式");
    buzzer(1, SHORT_BEEP_DURATION, 0);
    break;
  case FOOT_MODE: // 脚控模式。该模式下电推转速由脚控控制。按钮可以控制电机换相
    ESP_LOGI(TAG, "脚控模式");
    buzzer(1, SHORT_BEEP_DURATION, 0);
    break;
  case CRUISE_MODE: // 巡航模式。该模式下电推转速由脚控控制。按钮可以控制步进电机转速
    ESP_LOGI(TAG, "巡航模式");
    buzzer(1, SHORT_BEEP_DURATION, 0);
    break;
  case STANDBY_MODE:
    ESP_LOGI(TAG, "待机模式");
    buzzer(1, SHORT_BEEP_DURATION, 0);
    break;
  default:
    break;
  }
}

ControlMode getCurrentCtrlMode() {
  return current_ctrl_mode;
}

uint8_t getMotorCurrentSpeed() {
  uint8_t speed;
  taskENTER_CRITICAL(&motor_mutex);
  speed = current_speed;
  taskEXIT_CRITICAL(&motor_mutex);
  return speed;
}

// 缓启缓停 + 换向保护 + 换向停顿
static void handleMotorRamp(bool enable, uint8_t target_pwm, bool target_dir) {
  uint32_t now = millis(); // 临界区外读取，避免临界区里调用 millis()
  taskENTER_CRITICAL(&motor_mutex);

  bool needDirSwitch = (target_dir != current_dir);

  // 换向等待状态（函数内 static，保持跨调用）
  static bool     waitingDirSwitch  = false;
  static uint32_t dirSwitchWaitTime = 0;

  if (needDirSwitch) {
    // ==================== 换向流程 ====================
    // 第一阶段：强制减速到 0
    if (current_speed > 0) {
      int decel_step;
      if (current_speed <= 5) {
        decel_step = 1; // 最后几步平滑减速
      } else {
        decel_step = max(1, current_speed / SPEED_SCALE_FACTOR);
      }
      current_speed -= decel_step;
      if (current_speed < 0) current_speed = 0;

      // 刚开始减速，重置等待状态
      waitingDirSwitch  = false;
      dirSwitchWaitTime = 0;
    }
    // 第二阶段：速度已归零，进入等待
    else {
      if (!waitingDirSwitch) {
        // 首次进入等待，记录时间
        waitingDirSwitch  = true;
        dirSwitchWaitTime = now;
      }

      // 等待时间到，切换方向
      if (now - dirSwitchWaitTime >= DIR_SWITCH_PAUSE_MS) {
        current_dir = target_dir;
        digitalWrite(dir_pin, current_dir ? HIGH : LOW);
        waitingDirSwitch  = false;
        dirSwitchWaitTime = 0;
      }
      // 等待期间保持 current_speed = 0，不加速
    }
  } else {
    // ==================== 方向一致：正常缓启缓停 ====================
    // 一旦方向相同，清掉等待状态（可能用户在等待期间反悔了）
    waitingDirSwitch  = false;
    dirSwitchWaitTime = 0;

    if (enable) {
      if (current_speed < target_pwm) {
        current_speed += SPEED_STEP;
        if (current_speed > target_pwm) current_speed = target_pwm;
      } else if (current_speed > target_pwm) {
        current_speed -= SPEED_STEP;
        if (current_speed < target_pwm) current_speed = target_pwm;
      }
    } else {
      // 减速
      if (current_speed > 0) {
        int decel_step;
        if (current_speed <= 5) {
          decel_step = 1;
        } else {
          decel_step = max(1, current_speed / SPEED_SCALE_FACTOR);
        }
        current_speed -= decel_step;
        if (current_speed < 0) current_speed = 0;
      }
    }
  }
  // 输出 PWM
  ledcWrite(motor_channel, current_speed);
  taskEXIT_CRITICAL(&motor_mutex);
}

// 急停函数：立即停止电机，无缓启缓停
void motorEmergencyStop() {
  handleMotorRamp(0, 0, 0);
  ESP_LOGI(TAG, "电机急停");
}

// 刷新动力灯（方向+斩波）
static void updateModeLed(bool motorDirection) {
  uint32_t baseColor = motorDirection ? COLOR_GREEN : COLOR_BLUE;
  if (isChopping) {
    ledSetMode(modeRGB, LED_BLINK, baseColor, SHORT_FLASH_DURATION, SHORT_FLASH_INTERVAL);
  } else {
    ledSetMode(modeRGB, LED_ON, baseColor, 0, 0);
  }
}

void modeIdentify(void* pvParameters) {
  modeHandle = xTaskGetCurrentTaskHandle();
  attachInterrupt(digitalPinToInterrupt(on_hand_pin), modeChange_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(on_foot_pin), modeChange_ISR, CHANGE);
  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // 等待通知
    if (isFootPadOnline) {
      current_ctrl_mode = readCurrentModeWithDebounce();
      if (current_ctrl_mode != last_ctrl_mode) {
        last_ctrl_mode = current_ctrl_mode;
        modeChangeOperation(current_ctrl_mode);
      }
    } else {
      current_ctrl_mode = HAND_MODE; // 如果断线，返回手动模式
      modeChangeOperation(current_ctrl_mode);
      ESP_LOGW(TAG, "脚控不在线，返回手动模式");
    }
  }
}

void motorControl(void* pvParameters) {
  TickType_t       xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriod       = pdMS_TO_TICKS(10); // 延时 10ms，频率 = 1000 / 10 = 100 Hz，即每秒执行 100 次。
  while (1) {
    // 如果H桥故障，后续操作无效
    if (isH_BridgeFault) {
      vTaskDelayUntil(&xLastWakeTime, xPeriod);
      continue;
    }

    RecvFromFootPad_t recvData = getFootPadData(); // 获取脚控数据
    target_speed               = map(recvData.speed, 0, 4095, 0, 255);
    motor_move                 = recvData.data[2];

    switch (current_ctrl_mode) {
    case FOOT_MODE: {                   // motor_move为真时运转
      bool dirReverse = motorDirection; // 方向由减速按钮长按决定
      handleMotorRamp(motor_move, target_speed, dirReverse);
      onChopping(motor_move); // 根据是否运转来判断是否需要限流
      updateModeLed(dirReverse);
      break;
    }
    case CRUISE_MODE: {                 // motor_move为假时运转（即默认转，踩下停止）
      bool dirReverse = motorDirection; // 方向由减速按钮长按决定
      handleMotorRamp(!motor_move, target_speed, dirReverse);
      updateModeLed(dirReverse);
      break;
    }
    case HAND_MODE: {
      // ------------------------------------------------------------
      // 根据档位确定使能状态、目标 PWM 和目标方向
      //    档位规则：
      //      - 0: 停止
      //      - 正转档位 1~5: 1 最慢，5 最快
      //      - 反转档位 -1~-5: -1 最慢，-5 最快
      //
      //    我们将正转档位线性映射到 PWM 50~255（可根据电机启动电压调整下限）
      //    反转档位的绝对值同样映射到 PWM 50~255，但方向标志置为反转。
      //    映射方式：档位 1 对应 50，档位 5 对应 255；反转 -1 对应 50，-5 对应 255。
      // ------------------------------------------------------------
      int8_t      local_speed = getMotorSpeed();    // 获取当前手动档位速度
      bool        enable      = (local_speed != 0); // 非零档位才使能电机
      uint8_t     target_pwm  = 0;
      static bool target_dir  = false; // false = 正转, true = 反转（与硬件引脚逻辑匹配）

      if (local_speed > 0) {
        // 正转：档位 1~5 映射到 PWM 50~255
        target_pwm = map(local_speed, FORWARD_MIN_SPEED, FORWARD_MAX_SPEED, PWM_MIN_DUTY, PWM_MAX_DUTY);
        target_dir = false; // 正转方向
      } else if (local_speed < 0) {
        // 反转：取绝对值，档位 1~5 映射到 PWM 50~255
        uint8_t abs_speed = abs(local_speed); // 绝对值 1~5
        target_pwm        = map(abs_speed, FORWARD_MIN_SPEED, FORWARD_MAX_SPEED, PWM_MIN_DUTY, PWM_MAX_DUTY);
        target_dir        = true; // 反转方向
      }
      handleMotorRamp(enable, target_pwm, target_dir);
      updateModeLed(target_dir); // 方向来自档位正负号
      break;
    }
    case STANDBY_MODE:             // 待机模式：电机不转，步进电机也不工作
      ledcWrite(motor_channel, 0); // 停止电机
      // 保持动力灯现状，不修改
      break;
    default:
      break;
    }
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}

void motorInit() {
  ledcSetup(motor_channel, frequency, resolution); // 配置PWM通道
  ledcAttachPin(motor_pin, motor_channel);         // 将电机引脚绑定到PWM通道
  ledcWrite(motor_channel, 0);                     // 初始占空比为0
  pinMode(dir_pin, OUTPUT);                        // 设置转向引脚为输出模式
  pinMode(sleepPin, OUTPUT);                       // 设置睡眠引脚为输出模式
  pinMode(on_hand_pin, INPUT_PULLDOWN);            // 设置手控模式引脚为输入下拉模式
  pinMode(on_foot_pin, INPUT_PULLDOWN);            // 设置脚控模式引脚为输入下拉模式
  digitalWrite(sleepPin, HIGH);                    // 默认唤醒状态
  digitalWrite(dir_pin, LOW);                      // 设置引脚为默认方向（正转）
  current_dir    = false;                          // 与引脚状态一致
  last_ctrl_mode = STANDBY_MODE;
  modeChangeOperation(last_ctrl_mode);
}
