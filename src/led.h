#pragma once

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define STANDARD_BRIGHTNESS 100
#define SHORT_FLASH_DURATION 200
#define SHORT_FLASH_INTERVAL 200
#define LONG_FLASH_DURATION 500
#define LONG_FLASH_INTERVAL 500

// 对外暴露LED对象（如果需要）
extern Adafruit_NeoPixel sysRGB;
extern Adafruit_NeoPixel modeRGB;

// 颜色结构体：在头文件中直接定义
struct LedColors {
  const uint32_t RED    = 0xFF0000;
  const uint32_t GREEN  = 0x00FF00;
  const uint32_t BLUE   = 0x0000FF;
  const uint32_t YELLOW = 0xFF2800;
  const uint32_t WHITE  = 0xFFFFFF;
  const uint32_t OFF    = 0x000000;
};

// 声明全局变量
extern LedColors COLORS;

void ledInit();
void rgbBlink(Adafruit_NeoPixel& myRGB, uint8_t times, uint16_t duration, uint16_t interval, uint32_t color);
void mutipleColorBlink(Adafruit_NeoPixel& myRGB, uint32_t colors[], uint8_t colorNum, uint16_t duration, uint16_t interval);