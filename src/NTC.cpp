#include "Filters.h"
#include "esp_log.h"
#include <Arduino.h>

#define ARDUINO_IDE
#define FILTER_WINDOW_SIZE 8
#define LOW_PASS_ALPHA 0.3f
#define ADC_WIDTH ADC_WIDTH_BIT_12
#define ADC_ATTEN ADC_ATTEN_DB_11

static const char* TAG = "NTC";

/**  MF51-103 1.2头NTC
 * @brief 0~120℃ 对应的 NTC 电阻值（中心值，单位 KΩ）
 *        数据来源：MF51-103F3950L60 规格书 R/T 表
 *        索引 = 温度值（℃）
 */
const float res_table_mos[121] = {
  31.898, // 0°C
  30.377, // 1°C
  28.939, // 2°C
  27.579, // 3°C
  26.292, // 4°C
  25.074, // 5°C
  23.886, // 6°C
  22.762, // 7°C
  21.699, // 8°C
  20.692, // 9°C
  19.739, // 10°C
  18.835, // 11°C
  17.978, // 12°C
  17.166, // 13°C
  16.396, // 14°C
  15.665, // 15°C
  14.960, // 16°C
  14.290, // 17°C
  13.655, // 18°C
  13.052, // 19°C
  12.480, // 20°C
  11.932, // 21°C
  11.411, // 22°C
  10.917, // 23°C
  10.447, // 24°C
  10.000, // 25°C
  9.576,  // 26°C
  9.173,  // 27°C
  8.789,  // 28°C
  8.423,  // 29°C
  8.075,  // 30°C
  7.737,  // 31°C
  7.415,  // 32°C
  7.109,  // 33°C
  6.816,  // 34°C
  6.538,  // 35°C
  6.275,  // 36°C
  6.024,  // 37°C
  5.785,  // 38°C
  5.556,  // 39°C
  5.338,  // 40°C
  5.127,  // 41°C
  4.925,  // 42°C
  4.733,  // 43°C
  4.549,  // 44°C
  4.373,  // 45°C
  4.203,  // 46°C
  4.041,  // 47°C
  3.886,  // 48°C
  3.737,  // 49°C
  3.596,  // 50°C
  3.461,  // 51°C
  3.332,  // 52°C
  3.208,  // 53°C
  3.090,  // 54°C
  2.977,  // 55°C
  2.868,  // 56°C
  2.764,  // 57°C
  2.664,  // 58°C
  2.568,  // 59°C
  2.477,  // 60°C
  2.390,  // 61°C
  2.308,  // 62°C
  2.228,  // 63°C
  2.151,  // 64°C
  2.078,  // 65°C
  2.007,  // 66°C
  1.938,  // 67°C
  1.872,  // 68°C
  1.809,  // 69°C
  1.748,  // 70°C
  1.690,  // 71°C
  1.635,  // 72°C
  1.582,  // 73°C
  1.531,  // 74°C
  1.482,  // 75°C
  1.432,  // 76°C
  1.384,  // 77°C
  1.339,  // 78°C
  1.295,  // 79°C
  1.252,  // 80°C
  1.212,  // 81°C
  1.173,  // 82°C
  1.136,  // 83°C
  1.100,  // 84°C
  1.066,  // 85°C
  1.033,  // 86°C
  1.001,  // 87°C
  0.970,  // 88°C
  0.940,  // 89°C
  0.912,  // 90°C
  0.884,  // 91°C
  0.857,  // 92°C
  0.831,  // 93°C
  0.806,  // 94°C
  0.781,  // 95°C
  0.758,  // 96°C
  0.737,  // 97°C
  0.715,  // 98°C
  0.695,  // 99°C
  0.675,  // 100°C
  0.655,  // 101°C
  0.636,  // 102°C
  0.618,  // 103°C
  0.601,  // 104°C
  0.583,  // 105°C
  0.567,  // 106°C
  0.550,  // 107°C
  0.534,  // 108°C
  0.519,  // 109°C
  0.504,  // 110°C
  0.491,  // 111°C
  0.477,  // 112°C
  0.465,  // 113°C
  0.452,  // 114°C
  0.440,  // 115°C
  0.429,  // 116°C
  0.417,  // 117°C
  0.406,  // 118°C
  0.396,  // 119°C
  0.386   // 120°C
};

// PCB 贴片 NTC (HNTC0805-103F3950FB)
const float res_table_pcb[121] = {
  32.814, // 0°C
  31.179, // 1°C
  29.636, // 2°C
  28.178, // 3°C
  26.800, // 4°C
  25.497, // 5°C
  24.263, // 6°C
  23.096, // 7°C
  21.992, // 8°C
  20.947, // 9°C
  19.958, // 10°C
  19.022, // 11°C
  18.135, // 12°C
  17.294, // 13°C
  16.498, // 14°C
  15.742, // 15°C
  15.025, // 16°C
  14.345, // 17°C
  13.699, // 18°C
  13.086, // 19°C
  12.504, // 20°C
  11.951, // 21°C
  11.426, // 22°C
  10.926, // 23°C
  10.452, // 24°C
  10.000, // 25°C
  9.570,  // 26°C
  9.162,  // 27°C
  8.773,  // 28°C
  8.402,  // 29°C
  8.049,  // 30°C
  7.713,  // 31°C
  7.393,  // 32°C
  7.088,  // 33°C
  6.797,  // 34°C
  6.520,  // 35°C
  6.255,  // 36°C
  6.003,  // 37°C
  5.762,  // 38°C
  5.532,  // 39°C
  5.313,  // 40°C
  5.103,  // 41°C
  4.903,  // 42°C
  4.711,  // 43°C
  4.529,  // 44°C
  4.354,  // 45°C
  4.187,  // 46°C
  4.027,  // 47°C
  3.874,  // 48°C
  3.728,  // 49°C
  3.588,  // 50°C
  3.454,  // 51°C
  3.326,  // 52°C
  3.203,  // 53°C
  3.086,  // 54°C
  2.973,  // 55°C
  2.865,  // 56°C
  2.761,  // 57°C
  2.662,  // 58°C
  2.567,  // 59°C
  2.476,  // 60°C
  2.388,  // 61°C
  2.304,  // 62°C
  2.224,  // 63°C
  2.146,  // 64°C
  2.072,  // 65°C
  2.001,  // 66°C
  1.932,  // 67°C
  1.866,  // 68°C
  1.803,  // 69°C
  1.742,  // 70°C
  1.684,  // 71°C
  1.628,  // 72°C
  1.574,  // 73°C
  1.522,  // 74°C
  1.472,  // 75°C
  1.424,  // 76°C
  1.378,  // 77°C
  1.333,  // 78°C
  1.290,  // 79°C
  1.249,  // 80°C
  1.209,  // 81°C
  1.171,  // 82°C
  1.134,  // 83°C
  1.099,  // 84°C
  1.065,  // 85°C
  1.032,  // 86°C
  1.000,  // 87°C
  0.969,  // 88°C
  0.940,  // 89°C
  0.911,  // 90°C
  0.884,  // 91°C
  0.857,  // 92°C
  0.831,  // 93°C
  0.807,  // 94°C
  0.783,  // 95°C
  0.760,  // 96°C
  0.738,  // 97°C
  0.716,  // 98°C
  0.695,  // 99°C
  0.675,  // 100°C
  0.656,  // 101°C
  0.637,  // 102°C
  0.619,  // 103°C
  0.602,  // 104°C
  0.585,  // 105°C
  0.569,  // 106°C
  0.553,  // 107°C
  0.538,  // 108°C
  0.523,  // 109°C
  0.508,  // 110°C
  0.495,  // 111°C
  0.481,  // 112°C
  0.468,  // 113°C
  0.456,  // 114°C
  0.443,  // 115°C
  0.432,  // 116°C
  0.420,  // 117°C
  0.409,  // 118°C
  0.399,  // 119°C
  0.388   // 120°C
};

static uint8_t ntc_pcb_pin      = 5;
static uint8_t ntc_high_mos_pin = 6;
static uint8_t ntc_low_mos_pin  = 4;

static volatile float PCB_Temperature      = 0.0f;
static volatile float High_Mos_Temperature = 0.0f;
static volatile float Low_Mos_Temperature  = 0.0f;

Filters::LowPass PCB_NTC_Filter(LOW_PASS_ALPHA);
Filters::LowPass High_Mos_NTC_Filter(LOW_PASS_ALPHA);
Filters::LowPass Low_Mos_NTC_Filter(LOW_PASS_ALPHA);

void NTC_Init() {
  analogReadResolution(12);
  analogSetPinAttenuation(ntc_pcb_pin, ADC_11db);
  analogSetPinAttenuation(ntc_high_mos_pin, ADC_11db);
  analogSetPinAttenuation(ntc_low_mos_pin, ADC_11db);
}

/**
 * @brief 根据毫伏读数计算当前温度（查电阻表 + 线性插值）
 * @param filter 低通滤波器对象（引用）
 * @param pin    ADC 引脚
 * @param table  电阻表数组（需包含 0~120℃ 共 121 个值）
 * @return       温度值（℃），若超出 0~120℃ 则返回 -1
 */
float TemperatureReading(Filters::LowPass& filter, uint8_t pin, const float* table) {
  float raw_mv      = analogReadMilliVolts(pin);
  float filtered_mv = filter.update(raw_mv);
  if (filtered_mv <= 0 || filtered_mv >= 3300) {
    return -1.0f;
  }
  float r_ntc = 10000.0f * filtered_mv / (3300.0f - filtered_mv) / 1000.0f; // KΩ

  // 边界检查使用传入的表
  if (r_ntc >= table[0]) return -1.0f;   // ≤0℃
  if (r_ntc <= table[120]) return -1.0f; // ≥120℃

  int i = 0;
  for (i = 0; i < 120; i++) {
    if (r_ntc <= table[i] && r_ntc >= table[i + 1]) break;
  }
  float t = i + (table[i] - r_ntc) / (table[i] - table[i + 1]);
  return t;
}

void NTC_task(void* pvParameters) {
  TickType_t       xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriod       = pdMS_TO_TICKS(1000); // 延时 1000ms，频率 = 1000 / 1000 = 1 Hz，即每秒执行 1 次。
  uint32_t         lastCheck     = 0;
  while (1) {
    // 每 1000 次循环或每 5 秒检查一次栈水位
    if (millis() - lastCheck > 5000) {
      UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(NULL);
#ifdef ARDUINO_IDE
      Serial.printf("NTC任务 Stack left: %d\n", stackHighWater);
#else
      ESP_LOGI(TAG, "Stack left: %d", stackHighWater);
#endif
      lastCheck = millis();
    }
    PCB_Temperature      = TemperatureReading(PCB_NTC_Filter, ntc_pcb_pin, res_table_pcb);
    High_Mos_Temperature = TemperatureReading(High_Mos_NTC_Filter, ntc_high_mos_pin, res_table_mos);
    Low_Mos_Temperature  = TemperatureReading(Low_Mos_NTC_Filter, ntc_low_mos_pin, res_table_mos);
#ifdef ARDUINO_IDE
    Serial.printf("PCB_Temperature: %.2f\n°C, High_Mos_Temperature: %.2f\n°C, Low_Mos_Temperature: %.2f\n°C", PCB_Temperature, High_Mos_Temperature, Low_Mos_Temperature);
#else
    ESP_LOGI(TAG, "PCB_Temperature: %.2f\n°C, High_Mos_Temperature: %.2f\n°C, Low_Mos_Temperature: %.2f\n°C", PCB_Temperature, High_Mos_Temperature, Low_Mos_Temperature);
#endif
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}

float getPCBtemp() {
  return PCB_Temperature;
}
float getHighMosTemp() {
  return High_Mos_Temperature;
}
float getLowMosTemp() {
  return Low_Mos_Temperature;
}
float getChipTemp() {
  float temp = temperatureRead();
  return temp;
}