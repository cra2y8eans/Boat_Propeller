#pragma once

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

// ===== 常量定义 =====
#define SHORT_FLASH_DURATION 200
#define SHORT_FLASH_INTERVAL 200
#define LONG_FLASH_DURATION 500
#define LONG_FLASH_INTERVAL 500

#define COLOR_RED    0xFF0000
#define COLOR_GREEN  0x00FF00
#define COLOR_BLUE   0x0000FF
#define COLOR_YELLOW 0xFF2800
#define COLOR_WHITE  0xFFFFFF
#define COLOR_CYAN   0x00FFFF
#define COLOR_OFF    0x000000

// ===== LED对象 =====
extern Adafruit_NeoPixel sysRGB;
extern Adafruit_NeoPixel modeRGB;

// ===== LED模式枚举 =====
enum LEDMode {
  LED_ON,      // 常亮
  LED_OFF,     // 关闭
  LED_BLINK,   // 持续闪烁
  LED_IDLE,    // 空闲
};

// ===== 公共接口 =====
void ledInit();

/**
 * @brief 设置LED模式（非阻塞）
 * @param myRGB    LED对象引用
 * @param mode     LED模式
 * @param color    颜色值
 * @param duration 持续时间(ms)
 * @param interval 间隔时间(ms)
 */
void ledSetMode(Adafruit_NeoPixel& myRGB, enum LEDMode mode, uint32_t color, uint16_t duration, uint16_t interval);

/**
 * @brief LED状态机更新函数（非阻塞）
 * @param pvParameter 任务参数（可传NULL）
 * @note 必须在FreeRTOS任务中持续调用
 */
void ledUpdate(void* pvParameter);
