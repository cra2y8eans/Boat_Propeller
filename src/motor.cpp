#include "motor.h"
#include "ESPNOW.h"
#include "button.h"
#include "buzzer.h"
#include "esp_log.h"
#include "led.h"
#include <Arduino.h>

#define SPEED_STEP 8             // 每次调整的速度步长，值越大加速越快
#define SPEED_SCALE_FACTOR 10    // 减速比例系数（用于智能减速），分母，值越小减速越快
#define DIR_SWITCH_THRESH 5      // 允许切换方向的速度阈值（越小越安全）
#define SWITCH_DEBOUNCE_DELAY 20 // 按键消抖延时，单位毫秒
#define PWM_MIN_DUTY 50          // 最小档位对应的PWM值（1档）
#define PWM_MAX_DUTY 255         // 最大档位对应的PWM值（5档/3档）

static portMUX_TYPE    motor_mutex       = portMUX_INITIALIZER_UNLOCKED; // 定义临界区变量
static const char*     TAG               = "motor";                      // 日志标签
static const uint8_t   on_foot_pin       = 1;                            // 模式引脚
static const uint8_t   on_hand_pin       = 2;                            // 模式引脚
static const uint8_t   motor_pin         = 9;                            // 电机引脚
static const uint8_t   dir_pin           = 18;                           // 转向引脚
static const uint8_t   motor_channel     = 4;                            // 电机PWM通道
static const uint8_t   resolution        = 8;                            // 电机PWM精度
static const uint16_t  frequency         = 15000;                        // 电机频率
static const uint8_t   speed_step        = 10;                           // 电机速度变化步长
static int             target_speed      = 0;                            // 目标速度
static int             current_speed     = 0;                            // 当前速度
static bool            motor_move        = false;                        // 电机是否移动标志位
static bool            motor_dir         = false;                        // 电机方向标志位，用来接收脚控数据
static bool            current_dir       = false;                        // 当前方向标志位，代表当前电机旋转方向，用来跟motor_dir作比较
static ControlMode     current_ctrl_mode = FOOT_MODE;                    // 默认为脚控模式
static ControlMode     last_ctrl_mode    = FOOT_MODE;                    // 默认为脚控模式
volatile static int8_t motor_speed       = 0;                            // 手动挡电机转速（范围-3~5）
volatile uint8_t       stepSpeed         = 0;                            // 步进电机速度（范围1~5）

// 消抖读取当前模式
ControlMode readCurrentModeWithDebounce() {
  int readHand_1 = digitalRead(on_hand_pin);
  int readFoot_1 = digitalRead(on_foot_pin);
  vTaskDelay(SWITCH_DEBOUNCE_DELAY / portTICK_PERIOD_MS); // 延时20ms，消抖
  int readHand_2 = digitalRead(on_hand_pin);
  int readFoot_2 = digitalRead(on_foot_pin);

  if (readHand_1 != readHand_2 || readFoot_1 != readFoot_2) return last_ctrl_mode; // 如果两次读取的值不一样，说明有抖动，返回上次的模式
  if (readHand_1 == LOW && readFoot_1 == HIGH) return HAND_MODE;                   // 手控模式
  if (readHand_1 == HIGH && readFoot_1 == LOW) return FOOT_MODE;                   // 脚控模式
  if (readHand_1 == HIGH && readFoot_1 == HIGH) return CRUISE_MODE;                // 巡航模式

  return HAND_MODE; // 默认返回手动模式
}

void modeChangeOperation(ControlMode newMode) {
  ButtonEvent_t buttonEvent;
  switch (newMode) {
  case HAND_MODE: // 手控模式。该模式下电推转速由按钮控制。步进电机不工作。
    if (xQueueReceive(buttonQueue, &buttonEvent, 0) == pdTRUE) {
      if (buttonEvent.button == ACCEL_BUTTON) {
        motor_speed++;
      } else if (buttonEvent.button == DECEL_BUTTON) {
        motor_speed--;
      }
      motor_speed = constrain(motor_speed, -3, 5);
    }
    ledSetMode(modeRGB, LED_ON, COLOR_GREEN, 0, 0);
    buzzer(1, SHORT_BEEP_DURATION, 0);
    break;
  case FOOT_MODE:    // 脚控模式。该模式下电推转速由脚控控制。按钮可以控制步进电机转速
    motor_speed = 0; // 电推速度清零，防止切换手控模式时电机突然启动
    if (xQueueReceive(buttonQueue, &buttonEvent, 0) == pdTRUE) {
      if (buttonEvent.button == ACCEL_BUTTON) {
        stepSpeed++;
        stepSpeed = constrain(stepSpeed, 1, 5);
      } else if (buttonEvent.button == DECEL_BUTTON) {
        stepSpeed--;
        stepSpeed = constrain(stepSpeed, 1, 5);
      }
    }
    ledSetMode(modeRGB, LED_ON, COLOR_BLUE, 0, 0);
    buzzer(1, SHORT_BEEP_DURATION, 0);
    break;
  case CRUISE_MODE: // 巡航模式。该模式下电推转速由脚控控制。按钮可以控制步进电机转速
    motor_speed = 0;
    if (xQueueReceive(buttonQueue, &buttonEvent, 0) == pdTRUE) {
      if (buttonEvent.button == ACCEL_BUTTON) {
        stepSpeed++;
        stepSpeed = constrain(stepSpeed, 1, 5);
      } else if (buttonEvent.button == DECEL_BUTTON) {
        stepSpeed--;
        stepSpeed = constrain(stepSpeed, 1, 5);
      }
    }
    ledSetMode(modeRGB, LED_ON, COLOR_YELLOW, 0, 0);
    buzzer(1, SHORT_BEEP_DURATION, 0);
    break;
  case STANDBY_MODE:
    motor_speed = 0;
    ledSetMode(modeRGB, LED_ON, COLOR_RED, 0, 0);
    buzzer(1, SHORT_BEEP_DURATION, 0);
    break;
  default:
    break;
  }
}

ControlMode getCurrentCtrlMode() {
  return current_ctrl_mode;
}

uint8_t getStepSpeed() {
  return stepSpeed;
}

// 缓启缓停与方向控制统一处理函数
static void handleMotorRamp(bool enable, uint8_t target_pwm, bool target_dir) {
  // 速度缓变
  if (enable) {
    if (current_speed < target_pwm) {
      current_speed += SPEED_STEP;
      if (current_speed > target_pwm) current_speed = target_pwm;
    } else if (current_speed > target_pwm) {
      current_speed -= SPEED_STEP;
      if (current_speed < target_pwm) current_speed = target_pwm;
    }
  } else {
    if (current_speed > 0) {
      int decel_step = max(2, current_speed / SPEED_SCALE_FACTOR);
      current_speed -= decel_step;
      if (current_speed < 0) current_speed = 0;
    }
  }
  // 方向切换保护（仅在低速时允许改变方向）
  if (current_speed <= DIR_SWITCH_THRESH) {
    if (target_dir != current_dir) {
      current_dir = target_dir;
      digitalWrite(dir_pin, current_dir ? LOW : HIGH);
    }
  }
  // 输出 PWM
  ledcWrite(motor_channel, current_speed);
}

void modeIdentify(void* pvParameters) {
  while (1) {
    if (isFootPadOnline) {
      current_ctrl_mode = readCurrentModeWithDebounce();
      if (current_ctrl_mode != last_ctrl_mode) {
        last_ctrl_mode = current_ctrl_mode;
        modeChangeOperation(current_ctrl_mode);
        const char* modeNames[] = { "手动模式", "脚控模式", "巡航模式", "待机模式" };
        ESP_LOGI(TAG, "%s\n", modeNames[current_ctrl_mode]);
      }
    } else {
      current_ctrl_mode = STANDBY_MODE; // 如果断线，返回待机模式
      ESP_LOGE(TAG, "脚控不在线，返回待机模式");
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void motorControl(void* pvParameters) {
  while (1) {
    bool dirReverse;
    taskENTER_CRITICAL(&motor_mutex);
    target_speed = map(FootPadData.speed, 0, 4095, 0, 255);
    motor_move   = FootPadData.data[2];
    dirReverse   = FootPadData.data[3]; // 反向
    taskEXIT_CRITICAL(&motor_mutex);
    switch (current_ctrl_mode) {
    case FOOT_MODE: { // motor_move为真时运转
      handleMotorRamp(motor_move, target_speed, dirReverse);
      break;
    }
    case CRUISE_MODE: { // motor_move为假时运转（即默认转，踩下停止）
      handleMotorRamp(!motor_move, target_speed, dirReverse);
      break;
    }
    case HAND_MODE: {
      // ------------------------------------------------------------
      // 根据档位确定使能状态、目标 PWM 和目标方向
      //    档位规则：
      //      - 0: 停止
      //      - 正转档位 1~5: 1 最慢，5 最快
      //      - 反转档位 -1~-3: -1 最慢，-3 最快
      //
      //    我们将正转档位线性映射到 PWM 50~255（可根据电机启动电压调整下限）
      //    反转档位的绝对值同样映射到 PWM 50~255，但方向标志置为反转。
      //    映射方式：档位 1 对应 50，档位 5 对应 255；反转 -1 对应 50，-3 对应 255。
      // ------------------------------------------------------------
      int8_t  local_speed = motor_speed;
      bool    enable      = (local_speed != 0); // 非零档位才使能电机
      uint8_t target_pwm  = 0;
      bool    target_dir  = false; // false = 正转, true = 反转（与硬件引脚逻辑匹配）

      if (local_speed > 0) {
        // 正转：档位 1~5 映射到 PWM 50~255
        target_pwm = map(local_speed, 1, 5, PWM_MIN_DUTY, PWM_MAX_DUTY);
        target_dir = false; // 正转方向
      } else if (local_speed < 0) {
        // 反转：取绝对值，档位 1~3 映射到 PWM 50~255
        uint8_t abs_speed = abs(local_speed); // 绝对值 1~3
        target_pwm        = map(abs_speed, 1, 3, PWM_MIN_DUTY, PWM_MAX_DUTY);
        target_dir        = true; // 反转方向
      }
      handleMotorRamp(enable, target_pwm, target_dir);
      break;
    }
    case STANDBY_MODE:             // 待机模式：电机不转，步进电机也不工作
      ledcWrite(motor_channel, 0); // 停止电机
      break;
    default:
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // 添加短延时，避免任务占用过高
  }
}

void motorInit() {
  ledcSetup(motor_channel, frequency, resolution); // 配置PWM通道
  ledcAttachPin(motor_pin, motor_channel);         // 将电机引脚绑定到PWM通道
  ledcWrite(motor_channel, 0);                     // 初始占空比为0
  pinMode(dir_pin, OUTPUT);                        // 设置转向引脚为输出模式
  pinMode(on_hand_pin, INPUT_PULLDOWN);            // 设置手控模式引脚为输入下拉模式
  pinMode(on_foot_pin, INPUT_PULLDOWN);            // 设置脚控模式引脚为输入下拉模式
  digitalWrite(dir_pin, LOW);                      // 设置引脚为默认方向（正转）
  current_dir    = false;                          // 与引脚状态一致
  last_ctrl_mode = readCurrentModeWithDebounce();
}
