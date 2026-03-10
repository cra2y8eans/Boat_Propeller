#pragma once

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#define SHORT_FLASH_DURATION 200
#define SHORT_FLASH_INTERVAL 200
#define LONG_FLASH_DURATION 500
#define LONG_FLASH_INTERVAL 500

#define COLOR_RED 0xFF0000
#define COLOR_GREEN 0x00FF00
#define COLOR_BLUE 0x0000FF
#define COLOR_YELLOW 0xFF2800
#define COLOR_WHITE 0xFFFFFF
#define COLOR_CYAN 0x00FFFF
#define COLOR_OFF 0x000000

// 对外暴露LED对象
extern Adafruit_NeoPixel sysRGB;
extern Adafruit_NeoPixel modeRGB;

extern QueueHandle_t ledQueue;

enum sysLedMode {
  STANDBY,
  ESP_NOW_INIT_SUCCESS, // 红绿蓝交替闪烁
  ESP_NOW_INIT_FAIL,    // 红灯闪烁
  ESP_NOW_CONNECTED,    // 蓝灯常亮
  ESP_NOW_RECONNECTING, // 蓝灯闪烁
  ESP_NOW_DISCONNECTED, // 红灯常亮
  H_BRIDGE_FAULT,       // 红灯闪烁
  H_BRIDGE_CHOPPING,    // 黄灯闪烁
  OVER_HEAT,            // 红黄闪烁
  STEP_DIAG,            // 青色闪烁
};

void ledInit();
void sysLedTask(void* pvParameters);
void ModeLedTask(void* pvParameters);
