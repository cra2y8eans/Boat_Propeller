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

void setup() {
  Serial.begin(115200);
  vTaskDelay(1000 / portTICK_PERIOD_MS); // 等待串口稳定

  bool initSuccess = true;

  // 逐个初始化，失败则报错
  if (!ledInit()) {
    ESP_LOGE("MAIN", "LED初始化失败");
    initSuccess = false;
  }
  if (!buzzerInit()) {
    ESP_LOGE("MAIN", "蜂鸣器初始化失败");
    initSuccess = false;
  }
  if (!motorInit()) {
    ESP_LOGE("MAIN", "电机初始化失败");
    initSuccess = false;
  }
  if (!ina226_init()) {
    ESP_LOGE("MAIN", "INA226初始化失败");
    initSuccess = false;
  }
  if (!NTC_Init()) {
    ESP_LOGE("MAIN", "NTC初始化失败");
    initSuccess = false;
  }

  // ESP-NOW初始化（内部有错误处理）
  esp_now_setup();

  // 系统初始化完成提示
  if (initSuccess) {
    ESP_LOGI("MAIN", "系统初始化完成");
    ledSetMode(sysRGB, LED_ON, COLOR_GREEN, 0, 0);  // 绿色常亮表示系统就绪
    buzzer(2, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);  // 两声短鸣
    vTaskDelay(500 / portTICK_PERIOD_MS);  // 提示显示500ms
  } else {
    ESP_LOGE("MAIN", "系统初始化失败");
    ledSetMode(sysRGB, LED_BLINK, COLOR_RED, SHORT_FLASH_DURATION, SHORT_FLASH_INTERVAL);  // 红色闪烁表示失败
    buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);  // 三声短鸣
    while(1) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);  // 初始化失败，停在这里
    }
  }

  // 创建各个任务
  xTaskCreate(buzzerUpdate, "buzzerUpdate", 1024 * 2, NULL, 1, NULL);
  xTaskCreate(ina226_task, "ina226_task", 1024 * 2, NULL, 1, NULL);
  xTaskCreate(esp_now_connection_check, "esp_now_connection_check", 1024 * 2, NULL, 2, NULL);
  xTaskCreate(dataSent, "dataSent", 1024 * 2, NULL, 2, NULL);
  xTaskCreate(motorControl, "motorControl", 1024 * 2, NULL, 2, NULL);
  xTaskCreate(modeIdentify, "modeIdentify", 1024 * 2, NULL, 1, NULL);
  xTaskCreate(temperatureRead, "temperatureRead", 1024 * 2, NULL, 1, NULL);
  xTaskCreate(ledUpdate, "ledUpdate", 1024 * 2, NULL, 1, NULL);
}
void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
