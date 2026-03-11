#include "button.h"
#include "esp_log.h"
#include <Arduino.h>
#include <OneButton.h>

#define LONG_PRESS_DEBOUNCE_MS 800
static const char*   TAG              = "button";
static const uint8_t ACCEL_BUTTON_PIN = 40;
static const uint8_t DECEL_BUTTON_PIN = 41;

static OneButton accelButton, decelButton;
QueueHandle_t    buttonQueue = xQueueCreate(10, sizeof(ButtonEvent_t));

// 回调函数：使用局部结构体变量，避免共享
static void accelButtonShortPressed() {
  ButtonEvent_t evt = { ACCEL_BUTTON, BUTTON_EVENT_SHORT_PRESS };
  xQueueSend(buttonQueue, &evt, 0);
}

static void accelButtonLongPressed() {
  ButtonEvent_t evt = { ACCEL_BUTTON, BUTTON_EVENT_LONG_PRESS };
  xQueueSend(buttonQueue, &evt, 0);
}

static void decelButtonShortPressed() {
  ButtonEvent_t evt = { DECEL_BUTTON, BUTTON_EVENT_SHORT_PRESS };
  xQueueSend(buttonQueue, &evt, 0);
}

static void decelButtonLongPressed() {
  ButtonEvent_t evt = { DECEL_BUTTON, BUTTON_EVENT_LONG_PRESS };
  xQueueSend(buttonQueue, &evt, 0);
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
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}