// #include "Filters.h"
#include "Current.h"
#include "NTC.h"
#include "freertos/task.h"
#include <Arduino.h>
#include <FreeRTOS.h>

struct ToDebugData {
  float pcb_temperature;
  float h_bridge_temperature;
};
ToDebugData deviceData;

// SemaphoreHandle_t TempMutex; // 创建互斥锁句柄

float getTemperature(int adcValue) {
  if (adcValue >= adc_table[0]) {
    return 0.0f;
  }
  if (adcValue <= adc_table[120]) {
    return 120.0f; // 返回上边界 120℃
  }
  int i = 0;
  for (i = 0; i < 120; i++) {
    if (adcValue <= adc_table[i] && adcValue >= adc_table[i + 1]) {
      break;
    }
  }
  float t = i + (float)(adc_table[i] - adcValue) / (adc_table[i] - adc_table[i + 1]);
  return t; // 返回插值后的温度
}

void temperatureRead(void* pvParameters) {
  while (1) {
    // xSemaphoreTake(TempMutex, portMAX_DELAY);
    // deviceData.pcb_temperature      = getTemperature(PCB_NTC_Filter, ntc_pcb_pin);
    // deviceData.h_bridge_temperature = getTemperature(H_Bridge_NTC_Filter, ntc_h_bridge_pin);
    int   value       = analogRead(ntc_pcb_pin);
    float temperature = getTemperature(value);
    Serial.printf("pcb_temperature: %.2f°C\n", temperature);
    Serial.printf("adc: %d\n", value);
    // xSemaphoreGive(TempMutex);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void dataPrint(void* pvParameters) {
  while (1) {
    // xSemaphoreTake(TempMutex, portMAX_DELAY);
    Serial.printf("pcb_temperature: %.2f°C\n", deviceData.pcb_temperature);
    Serial.printf("adc: %.2f°C\n", deviceData.h_bridge_temperature);
    // Serial.printf("h_bridge_temperature: %.2f°C\n", deviceData.h_bridge_temperature);
    // xSemaphoreGive(TempMutex);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  vTaskDelay(1000 / portTICK_PERIOD_MS); // 等待串口稳定
  analogReadResolution(12);
  // PCB_NTC_Filter.setMovingAverageWindow(8);
  // H_Bridge_NTC_Filter.setMovingAverageWindow(8);

  // TempMutex = xSemaphoreCreateMutex(); // 创建互斥锁

  xTaskCreate(temperatureRead, "task_ntc", 1024 * 6, NULL, 1, NULL);
  // xTaskCreate(dataPrint, "task_print", 2048, NULL, 1, NULL);
}
void loop() {
}
