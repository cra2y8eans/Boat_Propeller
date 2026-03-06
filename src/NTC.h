#pragma once

#include <Arduino.h>
#include <Filters.h>

/**
 * @brief NTC 10K 3950 在 0~120℃ 对应的 ADC 读数（12位）
 *
 * 电路：Vref(3.3V) —— 10K固定电阻 —— ADC —— NTC —— GND
 * 数组下标 = 温度值（℃），数组元素 = 该温度下 ADC 的理论读数（已四舍五入）
 * 例如：adc_table[25] 是 25℃ 时的 ADC 值 2048
 *
 * 注意：数组是严格递减的，因为温度升高时 NTC 电阻减小，ADC 读数减小。
 */
const uint16_t adc_table[121] = {
  3121, // 0°C
  3083, // 1°C
  3046, // 2°C
  3008, // 3°C
  2968, // 4°C
  2927, // 5°C
  2887, // 6°C
  2846, // 7°C
  2804, // 8°C
  2761, // 9°C
  2721, // 10°C
  2676, // 11°C
  2632, // 12°C
  2588, // 13°C
  2543, // 14°C
  2498, // 15°C
  2454, // 16°C
  2409, // 17°C
  2364, // 18°C
  2319, // 19°C
  2273, // 20°C
  2228, // 21°C
  2182, // 22°C
  2137, // 23°C
  2093, // 24°C
  2048, // 25°C
  2003, // 26°C
  1959, // 27°C
  1915, // 28°C
  1871, // 29°C
  1828, // 30°C
  1785, // 31°C
  1743, // 32°C
  1701, // 33°C
  1660, // 34°C
  1618, // 35°C
  1578, // 36°C
  1539, // 37°C
  1499, // 38°C
  1461, // 39°C
  1423, // 40°C
  1386, // 41°C
  1349, // 42°C
  1313, // 43°C
  1278, // 44°C
  1244, // 45°C
  1210, // 46°C
  1177, // 47°C
  1144, // 48°C
  1113, // 49°C
  1078, // 50°C
  1052, // 51°C
  1022, // 52°C
  993,  // 53°C
  965,  // 54°C
  937,  // 55°C
  910,  // 56°C
  885,  // 57°C
  859,  // 58°C
  834,  // 59°C
  810,  // 60°C
  787,  // 61°C
  764,  // 62°C
  742,  // 63°C
  720,  // 64°C
  699,  // 65°C
  679,  // 66°C
  659,  // 67°C
  640,  // 68°C
  621,  // 69°C
  603,  // 70°C
  585,  // 71°C
  568,  // 72°C
  552,  // 73°C
  536,  // 74°C
  520,  // 75°C
  505,  // 76°C
  490,  // 77°C
  476,  // 78°C
  462,  // 79°C
  449,  // 80°C
  436,  // 81°C
  423,  // 82°C
  411,  // 83°C
  399,  // 84°C
  388,  // 85°C
  376,  // 86°C
  365,  // 87°C
  355,  // 88°C
  345,  // 89°C
  335,  // 90°C
  325,  // 91°C
  316,  // 92°C
  307,  // 93°C
  298,  // 94°C
  290,  // 95°C
  282,  // 96°C
  274,  // 97°C
  266,  // 98°C
  259,  // 99°C
  252,  // 100°C
  245,  // 101°C
  238,  // 102°C
  232,  // 103°C
  225,  // 104°C
  219,  // 105°C
  213,  // 106°C
  208,  // 107°C
  202,  // 108°C
  197,  // 109°C
  191,  // 110°C
  186,  // 111°C
  181,  // 112°C
  176,  // 113°C
  172,  // 114°C
  167,  // 115°C
  163,  // 116°C
  159,  // 117°C
  155,  // 118°C
  150,  // 119°C
  147   // 120°C
};

const uint8_t ntc_pcb_pin      = 6;
const uint8_t ntc_h_bridge_pin = 4;

// inline Filter PCB_NTC_Filter;
// inline Filter H_Bridge_NTC_Filter;


// /**
//  * @brief 根据 ADC 读数计算当前温度（查表 + 线性插值）
//  *
//  * @param filtedValue 从 ADC 读取的原始值（0~4095）
//  * @return float   对应的温度值（摄氏度），若超出范围则返回边界值
//  *
//  * @note 使用全局数组 adc_table[121]（0~120℃对应的ADC值）
//  *       假设数组严格递减（温度越高ADC越小）
//  */
// float getTemperature(Filter &filter, uint8_t pin) {
//     float filtedValue = filter.updateMovingAverageRaw(analogRead(pin));
//   // 1. 处理超出测量范围的情况
//   //    如果 ADC 读数大于等于 0℃ 的 ADC 值（3121），说明温度 ≤ 0℃
//   if (filtedValue >= adc_table[0]) {
//     return 0.0f; // 返回下边界 0℃（可根据需要改为外推或返回0）
//   }
//   //    如果 ADC 读数小于等于 120℃ 的 ADC 值（147），说明温度 ≥ 120℃
//   if (filtedValue <= adc_table[120]) {
//     return 120.0f; // 返回上边界 120℃
//   }

//   // 2. 查找 filtedValue 所在的区间
//   //    因为数组是递减的，我们从 0 开始，找到第一个满足
//   //    adc_table[i] >= filtedValue >= adc_table[i+1] 的 i
//   int i = 0;
//   for (i = 0; i < 120; i++) { // i 从 0 到 119，共 120 个区间
//     // 如果当前 ADC 值落在区间 [adc_table[i], adc_table[i+1]] 内   if (filtedValue > adc_table[i+1] && filtedValue <= adc_table[i])
//     if (filtedValue <= adc_table[i] && filtedValue >= adc_table[i + 1]) {
//       break; // 找到区间，退出循环
//     }
//   }

//   // 3. 线性插值计算温度
//   //    已知：
//   //      T_low  = i               （区间左端点温度）
//   //      T_high = i+1             （区间右端点温度）
//   //      ADC_low  = adc_table[i]   （左端点 ADC 值，较大）
//   //      ADC_high = adc_table[i+1] （右端点 ADC 值，较小）
//   //    当前 filtedValue 在两者之间，温度可用比例计算：
//   //      T = T_low + (ADC_low - filtedValue) / (ADC_low - ADC_high) * (T_high - T_low)
//   //    由于 T_high - T_low = 1，公式简化为：
//   //      T = i + (adc_table[i] - filtedValue) / (adc_table[i] - adc_table[i+1])

//   float t = i + (float)(adc_table[i] - filtedValue) / (adc_table[i] - adc_table[i + 1]);

//   return t; // 返回插值后的温度
// }