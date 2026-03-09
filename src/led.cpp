#include "led.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

static const uint8_t SYS_WS2812_PIN  = 16;
static const uint8_t MODE_WS2812_PIN = 15;

Adafruit_NeoPixel sysRGB(1, SYS_WS2812_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel modeRGB(1, MODE_WS2812_PIN, NEO_GRB + NEO_KHZ800);

LedColors COLORS;

void ledInit() {
  sysRGB.begin();
  modeRGB.begin();
  sysRGB.setBrightness(STANDARD_BRIGHTNESS);
  modeRGB.setBrightness(STANDARD_BRIGHTNESS);
  sysRGB.clear();
  modeRGB.clear();
}

/**  指示灯
 * @brief     适用于单个颜色闪烁
 * @param     times:    闪烁次数
 * @param     duration: 持续时间，单位毫秒
 * @param     interval: 每次闪烁的间隔时间，单位毫秒
 * @param     color:    颜色值
 */
void rgbBlink(Adafruit_NeoPixel& myRGB, uint8_t times, uint16_t duration, uint16_t interval, uint32_t color) {
  if (times == 1) interval = 0; // 如果只闪烁一次则不间隔
  for (int i = 0; i < times; i++) {
    myRGB.clear();
    myRGB.setPixelColor(0, color); // led编号和颜色，编号从0开始。
    myRGB.show();
    vTaskDelay(duration / portTICK_PERIOD_MS);
    myRGB.clear();
    myRGB.show();
    vTaskDelay(interval / portTICK_PERIOD_MS);
  }
}
// 多色闪烁
void mutipleColorBlink(Adafruit_NeoPixel& myRGB, uint32_t colors[], uint8_t colorNum, uint16_t duration, uint16_t interval) {
  for (int i = 0; i < colorNum; i++) {
    myRGB.clear();
    myRGB.setPixelColor(0, colors[i]);
    myRGB.show();
    vTaskDelay(duration / portTICK_PERIOD_MS);
    myRGB.clear();
    myRGB.show();
    vTaskDelay(interval / portTICK_PERIOD_MS);
  }
}