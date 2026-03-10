#include "motor.h"
#include "ESPNOW.h"
#include "esp_log.h"
#include <Arduino.h>


#define SPEED_STEP 10            // 电机速度变化步长
#define MIN_SPEED_CHANGE 20      // 最小可安全切换方向的速度
#define SPEED_SCALE_FACTOR 10    // 减速比
#define SWITCH_DEBOUNCE_DELAY 20 // 按键消抖延时，单位毫秒
#define LOG_OUTPUT_INTERVAL 2000 // 日志输出间隔，单位毫秒

static const char*    TAG                = "motor";
static const uint8_t  on_foot_pin        = 1;         // 模式引脚
static const uint8_t  on_hand_pin        = 2;         // 模式引脚
static const uint8_t  motor_pin          = 9;         // 电机引脚
static const uint8_t  dir_pin            = 18;         // 转向引脚
static const uint8_t  motor_channel      = 4;         // 电机PWM通道
static const uint8_t  resolution         = 8;         // 电机PWM精度
static const uint16_t frequency          = 15000;     // 电机频率
static const uint8_t  min_speed_change   = 20;        // 最小可安全切换方向的速度
static const uint8_t  speed_step         = 10;        // 电机速度变化步长
static int            target_speed       = 0;         // 目标速度
static int            current_speed      = 0;         // 当前速度
static bool           motor_move         = false;     // 电机是否移动标志位
static bool           motor_dir          = false;     // 电机方向标志位
static ControlMode    current_ctrl_mode  = FOOT_MODE; // 默认为脚控模式
static ControlMode    last_ctrl_mode     = FOOT_MODE; // 默认为脚控模式

void motorInit() {
  ledcSetup(motor_channel, frequency, resolution); // 配置PWM通道
  ledcAttachPin(motor_pin, motor_channel);         // 将电机引脚绑定到PWM通道
  ledcWrite(motor_channel, 0);                     // 初始占空比为0
  pinMode(dir_pin, OUTPUT);                        // 设置转向引脚为输出模式
  pinMode(on_hand_pin, INPUT_PULLDOWN);            // 设置手控模式引脚为输入下拉模式
  pinMode(on_foot_pin, INPUT_PULLDOWN);            // 设置脚控模式引脚为输入下拉模式
}

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

ControlMode getCurrentCtrlMode() {
  return current_ctrl_mode;
}

void modeIdentify(void* pvParameters) {
  while (1) {
    if (isFootPadOnline) {
      current_ctrl_mode = readCurrentModeWithDebounce();
      if (current_ctrl_mode != last_ctrl_mode) {
        last_ctrl_mode          = current_ctrl_mode;
        const char* modeNames[] = { "手动模式", "脚控模式", "巡航模式", "待机模式" };
        ESP_LOGI(TAG, "%s\n", modeNames[current_ctrl_mode]);
      }
    } else {
      current_ctrl_mode = STANDBY_MODE; // 如果断线，返回待机模式
      ESP_LOGE(TAG, "脚控不在线，返回待机模式");
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void motorControl(void* pvParameters) {
  while (1) {
    /* code */
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
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