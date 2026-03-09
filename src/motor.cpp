#include "motor.h"
#include "esp_log.h"
#include <Arduino.h>

#define DIR_BUTTON_PIN 4      // 方向按钮引脚
#define MOVE_BUTTON_PIN 10    // 移动按钮引脚
#define SPEED_STEP 10         // 电机速度变化步长
#define MIN_SPEED_CHANGE 20   // 最小可安全切换方向的速度
#define SPEED_SCALE_FACTOR 10 // 减速比

static const uint8_t  motor_pin          = 2;     // 电机引脚
static const uint8_t  dir_pin            = 3;     // 转向引脚
static const uint8_t  motor_channel      = 4;     // 电机PWM通道
static const uint8_t  resolution         = 8;     // 电机PWM精度
static const uint16_t frequency          = 15000; // 电机频率
static const uint8_t  speed_scale_factor = 10;    // 减速比
static const uint8_t  min_speed_change   = 20;    // 最小可安全切换方向的速度
static const uint8_t  speed_step         = 10;    // 电机速度变化步长
static int            filtered_duty;

static int  target_speed  = 0;
static int  current_speed = 0;
static bool motor_move    = false;
static bool motor_dir     = false;

void motorInit() {
  ledcSetup(motor_channel, frequency, resolution); // 配置PWM通道
  ledcAttachPin(motor_pin, motor_channel);         // 将电机引脚绑定到PWM通道
  ledcWrite(motor_channel, 0);                     // 初始占空比为0
  pinMode(dir_pin, OUTPUT);                        // 设置转向引脚为输出模式
}

void motorControl(void* pvParameters) {
  motorInit();
  while (1) {
    /* code */
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