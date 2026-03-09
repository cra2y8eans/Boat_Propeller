#include "ESPNOW.h"
#include "NTC.h"
#include "buzzer.h"
#include "current.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "led.h"
#include <Arduino.h>
#include <FreeRTOS.h>

void setup() {
  Serial.begin(115200);
  vTaskDelay(1000 / portTICK_PERIOD_MS); // 等待串口稳定
  // xTaskCreatePinnedToCore(ina226_task, "task_ina226", 1024 * 6, NULL, 1, NULL, 1);
  // xTaskCreatePinnedToCore(ina226_print_task, "task_ina_print", 1024 * 2, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(temperatureRead, "task_ntc", 1024 * 6, NULL, 1, NULL, 1);
  // xTaskCreatePinnedToCore(TemperaturePrint, "task_print", 1024 * 2, NULL, 1, NULL, 1);
}
void loop() {
  vTaskDelete(NULL); // 删除 loop 任务，所有工作由 FreeRTOS 任务完成
}
