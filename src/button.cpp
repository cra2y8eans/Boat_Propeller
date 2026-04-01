#include "button.h"
#include "buzzer.h"
#include "esp_log.h"
#include "motor.h"
#include <Arduino.h>
#include <OneButton.h>

#define ARDUINO_IDE
#define LONG_PRESS_DEBOUNCE_MS 800

portMUX_TYPE speedMutex = portMUX_INITIALIZER_UNLOCKED;

static const char*   TAG                      = "button";
static const uint8_t ACCEL_BUTTON_PIN         = 40;
static const uint8_t DECEL_BUTTON_PIN         = 41;
static uint8_t       stepSpeed                = 0;     // 当前步进电机转速
static int8_t        motorSpeed               = 0;     // 当前电机速度，正数表示前进，负数表示后退
volatile bool        isAccelButtonLongPressed = false; // 加速按钮长按标志位
volatile bool        isDecelButtonLongPressed = false; // 减速按钮长按标志位

static OneButton accelButton, decelButton;

uint8_t getStepSpeed() {
  uint8_t local_speed;
  taskENTER_CRITICAL(&speedMutex);
  local_speed = stepSpeed;
  taskEXIT_CRITICAL(&speedMutex);
  return local_speed;
}

int8_t getMotorSpeed() {
  int8_t local_speed;
  taskENTER_CRITICAL(&speedMutex);
  local_speed = motorSpeed;
  taskEXIT_CRITICAL(&speedMutex);
  return local_speed;
}

// 短按回调
static void accelButtonShortPressed() { // 加速
  buzzer(1, SHORT_BEEP_DURATION, 0);
  ControlMode mode = getCurrentCtrlMode();
  switch (mode) {
  case HAND_MODE:
    taskENTER_CRITICAL(&speedMutex);
    motorSpeed = min(motorSpeed + 1, 5); // 增加档位，最大为5
    taskEXIT_CRITICAL(&speedMutex);
    break;
  case FOOT_MODE:
    taskENTER_CRITICAL(&speedMutex);
    stepSpeed = min(stepSpeed + 1, 5); // 增加档位，最大为5
    taskEXIT_CRITICAL(&speedMutex);
    break;
  default:
    break;
  }
}
static void decelButtonShortPressed() { // 减速
  buzzer(1, SHORT_BEEP_DURATION, 0);
  ControlMode mode = getCurrentCtrlMode();
  switch (mode) {
  case HAND_MODE:
    taskENTER_CRITICAL(&speedMutex);
    motorSpeed = max(motorSpeed - 1, -3); // 减少档位，最小为1
    taskEXIT_CRITICAL(&speedMutex);
    break;
  case FOOT_MODE:
    taskENTER_CRITICAL(&speedMutex);
    stepSpeed = max(stepSpeed - 1, 1); // 减少档位，最小为-3
    taskEXIT_CRITICAL(&speedMutex);
    break;
  default:
    break;
  }
}

// 长按回调
static void accelButtonLongPressed() { // 加速
  isAccelButtonLongPressed = !isAccelButtonLongPressed;
  buzzer(1, LONG_BEEP_DURATION, 0);
}
static void decelButtonLongPressed() { // 减速
  isDecelButtonLongPressed = !isDecelButtonLongPressed;
  buzzer(1, LONG_BEEP_DURATION, 0);
}

void buttonInit() {
  accelButton.setup(ACCEL_BUTTON_PIN, INPUT_PULLDOWN, false); // 按钮触发为高电平，参数须填false，反之为true（默认）
  decelButton.setup(DECEL_BUTTON_PIN, INPUT_PULLDOWN, false);
  accelButton.attachClick(accelButtonShortPressed);
  accelButton.attachLongPressStart(accelButtonLongPressed);
  decelButton.attachClick(decelButtonShortPressed);
  decelButton.attachLongPressStart(decelButtonLongPressed);
  accelButton.setPressMs(LONG_PRESS_DEBOUNCE_MS);
  decelButton.setPressMs(LONG_PRESS_DEBOUNCE_MS);
}

void buttonTask(void* pvParameters) {
  uint32_t lastCheck = 0;
  while (1) {
    // 每 1000 次循环或每 5 秒检查一次栈水位
    if (millis() - lastCheck > 5000) {
      UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(NULL);
#ifdef ARDUINO_IDE
      Serial.printf("按钮任务 Stack left: %d\n", stackHighWater);
#else
      ESP_LOGI(TAG, "Stack left: %d words", stackHighWater);
#endif
      lastCheck = millis();
    }
    accelButton.tick();
    decelButton.tick();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}