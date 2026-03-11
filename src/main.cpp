#include "ESPNOW.h"
#include "NTC.h"
#include "buzzer.h"
#include "current.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "led.h"
#include "motor.h"
#include <Arduino.h>
#include <FreeRTOS.h>
#include "button.h"

void setup() {
  Serial.begin(115200);
  vTaskDelay(1000 / portTICK_PERIOD_MS); // 等待串口稳定

  ledInit();
  buzzerInit();
  motorInit();
  ina226_init();
  NTC_Init();
  esp_now_setup();
  buttonInit();

  xTaskCreate(buzzerUpdate, "buzzerUpdate", 1024 * 2, NULL, 1, NULL);
  xTaskCreate(ina226_task, "ina226_task", 1024 * 2, NULL, 1, NULL);
  xTaskCreate(esp_now_connection_check, "esp_now_connection_check", 1024 * 2, NULL, 2, NULL);
  xTaskCreate(dataSent, "dataSent", 1024 * 2, NULL, 2, NULL);
  xTaskCreate(motorControl, "motorControl", 1024 * 2, NULL, 2, NULL);
  xTaskCreate(modeIdentify, "modeIdentify", 1024 * 2, NULL, 1, NULL);
  xTaskCreate(temperatureRead, "temperatureRead", 1024 * 2, NULL, 1, NULL);
  xTaskCreate(ledUpdate, "ledUpdate", 1024 * 2, NULL, 1, NULL);
  xTaskCreate(buttonTask, "ButtonTask", 2048, NULL, 1, NULL);
}
void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
