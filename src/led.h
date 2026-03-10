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

#define COLOR_RED 0xFF0000
#define COLOR_GREEN 0x00FF00
#define COLOR_BLUE 0x0000FF
#define COLOR_YELLOW 0xFF2800
#define COLOR_WHITE 0xFFFFFF
#define COLOR_OFF 0x000000

// 对外暴露LED对象
extern Adafruit_NeoPixel sysRGB;
extern Adafruit_NeoPixel modeRGB;

extern QueueHandle_t ledQueue;

enum sysLedMode {
  STANDBY,
  ESP_NOW_INIT,
  ESP_NOW_INIT_FAIL,
  ESP_NOW_CONNECTED,
  ESP_NOW_DISCONNECTED,
  H_BRIDGE_FAULT,
  H_BRIDGE_CHOPPING,
  H_BRIDGE_OVER_HEAT,
  STEP_DIAG,
  STEP_OVER_HEAT,
};
extern sysLedMode LedMode;

void ledInit();
void sysLedTask(void* pvParameters);
void ModeLedTask(void* pvParameters);
