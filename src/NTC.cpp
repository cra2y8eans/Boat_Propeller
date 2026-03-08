#include "esp_log.h"
#include <Arduino.h>
#include <Filters.h>

#define FILTER_WINDOW_SIZE 8
#define LOW_PASS_ALPHA 0.3f
#define ADC_WIDTH ADC_WIDTH_BIT_12
#define ADC_ATTEN ADC_ATTEN_DB_11

static const char* TAG = "NTC";

/**
 * @brief 0~120℃ 对应的 NTC 电阻值（中心值，单位 KΩ）
 *        R25=10KΩ，B25/85=4020K
 *        索引 = 温度值（℃）
 */
static const float res_table[121] = {
  32.040, // 0°C
  30.490, // 1°C
  29.022, // 2°C
  27.633, // 3°C
  26.317, // 4°C
  25.071, // 5°C
  23.889, // 6°C
  22.769, // 7°C
  21.707, // 8°C
  20.700, // 9°C
  19.788, // 10°C
  18.838, // 11°C
  17.977, // 12°C
  17.160, // 13°C
  16.383, // 14°C
  15.646, // 15°C
  14.945, // 16°C
  14.280, // 17°C
  13.647, // 18°C
  13.045, // 19°C
  12.472, // 20°C
  11.928, // 21°C
  11.409, // 22°C
  10.916, // 23°C
  10.447, // 24°C
  10.000, // 25°C
  9.574,  // 26°C
  9.168,  // 27°C
  8.781,  // 28°C
  8.413,  // 29°C
  8.062,  // 30°C
  7.727,  // 31°C
  7.407,  // 32°C
  7.103,  // 33°C
  6.812,  // 34°C
  6.534,  // 35°C
  6.270,  // 36°C
  6.017,  // 37°C
  5.775,  // 38°C
  5.545,  // 39°C
  5.324,  // 40°C
  5.114,  // 41°C
  4.913,  // 42°C
  4.720,  // 43°C
  4.536,  // 44°C
  4.360,  // 45°C
  4.192,  // 46°C
  4.031,  // 47°C
  3.877,  // 48°C
  3.730,  // 49°C
  3.572,  // 50°C
  3.454,  // 51°C
  3.324,  // 52°C
  3.201,  // 53°C
  3.082,  // 54°C
  2.968,  // 55°C
  2.859,  // 56°C
  2.755,  // 57°C
  2.654,  // 58°C
  2.558,  // 59°C
  2.466,  // 60°C
  2.378,  // 61°C
  2.293,  // 62°C
  2.212,  // 63°C
  2.134,  // 64°C
  2.059,  // 65°C
  1.987,  // 66°C
  1.918,  // 67°C
  1.851,  // 68°C
  1.788,  // 69°C
  1.726,  // 70°C
  1.668,  // 71°C
  1.611,  // 72°C
  1.557,  // 73°C
  1.504,  // 74°C
  1.454,  // 75°C
  1.406,  // 76°C
  1.359,  // 77°C
  1.314,  // 78°C
  1.271,  // 79°C
  1.230,  // 80°C
  1.190,  // 81°C
  1.151,  // 82°C
  1.114,  // 83°C
  1.079,  // 84°C
  1.045,  // 85°C
  1.011,  // 86°C
  0.980,  // 87°C
  0.949,  // 88°C
  0.919,  // 89°C
  0.891,  // 90°C
  0.863,  // 91°C
  0.837,  // 92°C
  0.811,  // 93°C
  0.786,  // 94°C
  0.763,  // 95°C
  0.740,  // 96°C
  0.718,  // 97°C
  0.696,  // 98°C
  0.675,  // 99°C
  0.656,  // 100°C
  0.636,  // 101°C
  0.618,  // 102°C
  0.600,  // 103°C
  0.582,  // 104°C
  0.566,  // 105°C
  0.549,  // 106°C
  0.534,  // 107°C
  0.519,  // 108°C
  0.504,  // 109°C
  0.490,  // 110°C
  0.476,  // 111°C
  0.463,  // 112°C
  0.450,  // 113°C
  0.438,  // 114°C
  0.426,  // 115°C
  0.414,  // 116°C
  0.403,  // 117°C
  0.392,  // 118°C
  0.381,  // 119°C
  0.371   // 120°C
};

static uint8_t ntc_pcb_pin      = 6;
static uint8_t ntc_high_mos_pin = 7;
static uint8_t ntc_low_mos_pin  = 4;

static float PCB_Temperature      = 0.0f;
static float High_Mos_Temperature = 0.0f;
static float Low_Mos_Temperature  = 0.0f;

Filters ::LowPass PCB_NTC_Filter(LOW_PASS_ALPHA);
Filters ::LowPass High_Mos_NTC_Filter(LOW_PASS_ALPHA);
Filters ::LowPass Low_Mos_NTC_Filter(LOW_PASS_ALPHA);

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
 * @return       温度值（℃），若超出 0~120℃ 则返回 -1
 */
float TemperatureReading(Filters::LowPass& filter, uint8_t pin) {
  // 1. 读取毫伏值并滤波
  float raw_mv      = analogReadMilliVolts(pin);
  float filtered_mv = filter.update(raw_mv);
  // 2. 计算 NTC 电阻值（单位 KΩ）
  //    电路：3.3V —— 10K 固定电阻 —— ADC —— NTC —— GND
  //    公式：R_ntc = 10K * V_mv / (3300 - V_mv)
  if (filtered_mv <= 0 || filtered_mv >= 3300) {
    // 无效的电压值，直接返回 -1 表示异常
    return -1.0f;
  }
  float r_ntc = 10000.0f * filtered_mv / (3300.0f - filtered_mv) / 1000.0f; // 转换为 KΩ
  // 3. 边界检查：超出电阻表范围则返回 -1
  if (r_ntc >= res_table[0]) { // 温度 ≤ 0℃
    return -1.0f;
  }
  if (r_ntc <= res_table[120]) { // 温度 ≥ 120℃
    return -1.0f;
  }
  // 4. 查找区间（电阻值递减）
  int i = 0;
  for (i = 0; i < 120; i++) {
    if (r_ntc <= res_table[i] && r_ntc >= res_table[i + 1]) {
      break;
    }
  }
  // 5. 线性插值
  float t = i + (res_table[i] - r_ntc) / (res_table[i] - res_table[i + 1]);
  return t;
}

void temperatureRead(void* pvParameters) {
  NTC_Init();
  while (1) {
    PCB_Temperature      = TemperatureReading(PCB_NTC_Filter, ntc_pcb_pin);
    High_Mos_Temperature = TemperatureReading(High_Mos_NTC_Filter, ntc_high_mos_pin);
    Low_Mos_Temperature  = TemperatureReading(Low_Mos_NTC_Filter, ntc_low_mos_pin);
    ESP_LOGI(TAG, "PCB_Temperature: %.2f\n°C, High_Mos_Temperature: %.2f\n°C, Low_Mos_Temperature: %.2f\n°C", PCB_Temperature, High_Mos_Temperature, Low_Mos_Temperature);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
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