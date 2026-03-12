#include "button.h"
#include "buzzer.h"
#include "esp_log.h"
#include <Arduino.h>
#include <OneButton.h>

#define LONG_PRESS_DEBOUNCE_MS 800
static const char*   TAG                      = "button";
static const uint8_t ACCEL_BUTTON_PIN         = 40;
static const uint8_t DECEL_BUTTON_PIN         = 41;
volatile bool        isAccelButtonLongPressed = false; // 加速按钮长按标志位
volatile bool        isDecelButtonLongPressed = false; // 减速按钮长按标志位

static OneButton accelButton, decelButton;

QueueHandle_t buttonQueue = xQueueCreate(10, sizeof(ButtonEvent_t));

// 回调函数：使用局部结构体变量，避免共享
// 短按回调
static void accelButtonShortPressed() { // 加速
  ButtonEvent_t evt = { ACCEL_BUTTON, BUTTON_EVENT_SHORT_PRESS };
  xQueueSend(buttonQueue, &evt, 0);
  buzzer(1, SHORT_BEEP_DURATION, 0);
}
static void decelButtonShortPressed() { // 减速
  ButtonEvent_t evt = { DECEL_BUTTON, BUTTON_EVENT_SHORT_PRESS };
  xQueueSend(buttonQueue, &evt, 0);
  buzzer(1, SHORT_BEEP_DURATION, 0);
}

// 长按回调
static void accelButtonLongPressed() { // 加速
  isAccelButtonLongPressed = !isAccelButtonLongPressed;
  ButtonEvent_t evt        = { ACCEL_BUTTON, BUTTON_EVENT_LONG_PRESS };
  xQueueSend(buttonQueue, &evt, 0);
  buzzer(1, LONG_BEEP_DURATION, 0);
}
static void decelButtonLongPressed() { // 减速
  isDecelButtonLongPressed = !isDecelButtonLongPressed;
  ButtonEvent_t evt        = { DECEL_BUTTON, BUTTON_EVENT_LONG_PRESS };
  xQueueSend(buttonQueue, &evt, 0);
  buzzer(1, LONG_BEEP_DURATION, 0);
}

void buttonInit() {
  accelButton.setup(ACCEL_BUTTON_PIN, INPUT_PULLDOWN);
  decelButton.setup(DECEL_BUTTON_PIN, INPUT_PULLDOWN);
  accelButton.attachClick(accelButtonShortPressed);
  accelButton.attachLongPressStart(accelButtonLongPressed);
  decelButton.attachClick(decelButtonShortPressed);
  decelButton.attachLongPressStart(decelButtonLongPressed);
  accelButton.setPressMs(LONG_PRESS_DEBOUNCE_MS);
  decelButton.setPressMs(LONG_PRESS_DEBOUNCE_MS);
}

void buttonTask(void* pvParameters) {
  while (1) {
    accelButton.tick();
    decelButton.tick();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}