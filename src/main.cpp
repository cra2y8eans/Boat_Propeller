#include "ESPNOW.h"
#include "NTC.h"
#include "button.h"
#include "buzzer.h"
#include "current.h"
#include "esp_log.h"
#include "fan.h"
#include "fault.h"
#include "freertos/task.h"
#include "led.h"
#include "motor.h"
#include "step.h"
#include <Arduino.h>
#include <FreeRTOS.h>

void setup() {
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(1000)); // 等待串口稳定

  ledInit();
  buzzerInit();
  esp_now_setup();
  buttonInit();
  stepper_init();
  motorInit();
  // ina226_init();
  // NTC_Init();
  // Fan_Init();
  // fault_init();

  xTaskCreatePinnedToCore(ledUpdate, "ledUpdate", 1024 * 3, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(buzzerUpdate, "buzzerUpdate", 1024 * 3, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(dataSent, "dataSent", 1024 * 4, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(esp_now_connection_check, "esp_now_connection_check", 1024 * 4, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(buttonTask, "ButtonTask", 1024 * 3, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(modeIdentify, "modeIdentify", 1024 * 3, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(motorControl, "motorControl", 1024 * 4, NULL, 2, NULL, 1);
  // xTaskCreatePinnedToCore(NTC_task, "temperatureRead", 1024 * 2, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(stepper_control_task, "stepper_control_task", 1024 * 4, NULL, 3, NULL, 1);
  // xTaskCreatePinnedToCore(ina226_task, "ina226_task", 1024 * 3, NULL, 1, NULL, 1);
  // xTaskCreatePinnedToCore(Fan_task, "Fan_Task", 1024 * 2, NULL, 1, NULL, 1);
  // xTaskCreatePinnedToCore(fault_task, "fault_task", 1024 * 3, NULL, 3, &faultTaskHandle, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
