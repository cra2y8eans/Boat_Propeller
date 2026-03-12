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
#define LOG_OUTPUT_INTERVAL 2000 // 日志输出间隔，单位毫秒

static portMUX_TYPE   motor_mutex       = portMUX_INITIALIZER_UNLOCKED; // 定义临界区变量
static const char*    TAG               = "motor";                      // 日志标签
static const uint8_t  on_foot_pin       = 1;                            // 模式引脚
static const uint8_t  on_hand_pin       = 2;                            // 模式引脚
static const uint8_t  motor_pin         = 9;                            // 电机引脚
static const uint8_t  dir_pin           = 18;                           // 转向引脚
static const uint8_t  motor_channel     = 4;                            // 电机PWM通道
static const uint8_t  resolution        = 8;                            // 电机PWM精度
static const uint16_t frequency         = 15000;                        // 电机频率
static const uint8_t  min_speed_change  = 20;                           // 最小可安全切换方向的速度
static const uint8_t  speed_step        = 10;                           // 电机速度变化步长
static int            target_speed      = 0;                            // 目标速度
static int            current_speed     = 0;                            // 当前速度
static bool           motor_move        = false;                        // 电机是否移动标志位
static bool           motor_dir         = false;                        // 电机方向标志位，用来接收脚控数据
static bool           current_dir       = false;                        // 当前方向标志位，代表当前电机旋转方向，用来跟motor_dir作比较
static ControlMode    current_ctrl_mode = FOOT_MODE;                    // 默认为脚控模式
static ControlMode    last_ctrl_mode    = FOOT_MODE;                    // 默认为脚控模式
static uint8_t        motor_speed       = 0;                            // 手动挡电机转速
volatile uint8_t      stepSpeed         = 0;                            // 步进电机转速

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
  switch (current_ctrl_mode) {
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
    int  stepperMicros;
    bool turnLeft, turnRight, motorGo, dirReverse;
    taskENTER_CRITICAL(&motor_mutex);
    target_speed = map(FootPadData.speed, 0, 4095, 0, 255);

    motor_move = FootPadData.data[2];
    dirReverse = FootPadData.data[3]; // 反向
    taskEXIT_CRITICAL(&motor_mutex);

    switch (current_ctrl_mode) {

    case FOOT_MODE: {           // 脚控模式：motor_move为真时运转
      bool enable = motor_move; // 使能信号
      int  decel_step;          // 减速步长
      // 缓启缓停
      if (enable) {
        if (current_speed < target_speed) {
          current_speed += SPEED_STEP;
          if (current_speed > target_speed) current_speed = target_speed;
        } else if (current_speed > target_speed) {
          current_speed -= SPEED_STEP;
          if (current_speed < target_speed) current_speed = target_speed;
        }
      } else {
        if (current_speed > 0) {
          decel_step = max(2, current_speed / SPEED_SCALE_FACTOR); // 计算减速步长。
          current_speed -= decel_step;
          if (current_speed < 0) current_speed = 0;
        }
      }
      // 方向切换保护（仅在低速时允许改变方向）
      if (current_speed <= DIR_SWITCH_THRESH) {
        if (dirReverse != current_dir) {
          current_dir = dirReverse;
          digitalWrite(dir_pin, current_dir ? LOW : HIGH); // 修正为正确的引脚
        }
      }
      ledcWrite(motor_channel, current_speed);
      break;
    }

    case CRUISE_MODE: {          // 巡航模式：motor_move为假时运转（即默认转，按下停止）
      bool enable = !motor_move; // 使能信号取反
      // 速度缓变
      if (enable) {
        if (current_speed < target_speed) {
          current_speed += SPEED_STEP;
          if (current_speed > target_speed) current_speed = target_speed;
        } else if (current_speed > target_speed) {
          current_speed -= SPEED_STEP;
          if (current_speed < target_speed) current_speed = target_speed;
        }
      } else {
        if (current_speed > 0) {
          int decel_step = max(2, current_speed / SPEED_SCALE_FACTOR);
          current_speed -= decel_step;
          if (current_speed < 0) current_speed = 0;
        }
      }
      // 方向切换保护
      if (current_speed <= DIR_SWITCH_THRESH) {
        if (dirReverse != current_dir) {
          current_dir = dirReverse;
          digitalWrite(dir_pin, current_dir ? LOW : HIGH);
        }
      }
      ledcWrite(motor_channel, current_speed);
      break;
    }
    case HAND_MODE:
    case STANDBY_MODE:
      // 手控模式和待机模式：电机不转，步进电机也不工作（根据之前逻辑）
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
// void motor() {
//   /*---------------- test version -----------------------*/

//   if (motor_move) {
//     int  duty           = analogRead(motor_speed_pin);
//     bool button_pressed = (digitalRead(MOVE_BUTTON_PIN) == LOW);
//     target_speed        = map(filtered_duty, 0, 4095, 0, 255);
//     if (current_speed < target_speed) {
//       current_speed += SPEED_STEP;
//       if (current_speed > target_speed) {
//         current_speed = target_speed;
//       }
//     } else {
//       if (current_speed > target_speed) {
//         current_speed -= SPEED_STEP;
//         if (current_speed < target_speed) {
//           current_speed = target_speed;
//         }
//       }
//     }
//   } else {
//     if (current_speed > 0) {
//       // 智能减速：速度高时减得多，速度低时减得少
//       int decel_step = max(2, current_speed / SPEED_SCALE_FACTOR); // 至少减2
//       current_speed -= decel_step;
//       if (current_speed <= 0) {
//         current_speed = 0;
//       }
//     }
//   }
//   ledcWrite(motor_channel, current_speed);
// }

/*---------------- slowly start -----------------------*/

// void motor() {
//   // 读取输入
//   int   duty           = analogRead(motor_speed_pin);
//   bool  button_pressed = (digitalRead(MOVE_BUTTON_PIN) == LOW);
//   float filtered_duty  = potentiometerFilter.updateMovingAverageRaw(duty);
//   // 计算目标速度
//   target_speed = map(filtered_duty, 0, 4095, 0, 255);
//   // 状态机逻辑
//   switch (motorState) {
//   case STANDBY:
//     if (button_pressed) {
//       motorState = PRESSED;
//     }
//     current_speed = 0;
//     break;
//   case PRESSED:
//     if (!button_pressed) {
//       motorState = RELEASED;
//     } else {
//       // 加速到目标速度
//       if (current_speed < target_speed) {
//         current_speed += SPEED_STEP;
//         if (current_speed > target_speed) {
//           current_speed = target_speed;
//         }
//       }
//     }
//     break;
//   case RELEASED:
//     if (button_pressed) {
//       motorState = PRESSED;
//     } else {
//       // 减速到0
//       if (current_speed > 0) {
//         // 智能减速：速度高时减得多，速度低时减得少
//         int decel_step = max(2, current_speed / SPEED_SCALE_FACTOR); // 至少减2
//         current_speed -= decel_step;
//         if (current_speed <= 0) {
//           current_speed = 0;
//           motorState    = STANDBY; // 完全停止后回到待机状态
//         }
//       }
//     }
//     break;
//   }
//   // 方向控制：只有在低速时才允许切换方向
//   if (current_speed <= MIN_SPEED_CHANGE) {
//     digitalWrite(DIR_PIN, motor_dir ? HIGH : LOW);
//   }
//   // 输出PWM
//   ledcWrite(motor_channel, current_speed);
// #ifdef DEBUG
//   // 调试输出
//   static unsigned long last_debug = 0;
//   if (millis() - last_debug > 100) {
//     const char* state_str = "";
//     switch (motorState) {
//     case STANDBY:
//       state_str = "待机";
//       break;
//     case PRESSED:
//       state_str = "加速";
//       break;
//     case RELEASED:
//       state_str = "减速";
//       break;
//     }
//     Serial.printf("状态: %s | 目标: %d | 当前: %d | 按钮: %s\n",
//         state_str, target_speed, current_speed,
//         button_pressed ? "按下" : "松开");
//     last_debug = millis();
//   }
// #endif
//   delay(10); // 控制循环速度
// }