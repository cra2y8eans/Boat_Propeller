#include "led.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#define ARDUINO_IDE
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define STANDARD_BRIGHTNESS 10

static const char* TAG = "LED";

// ===== 硬件配置 =====
static const uint8_t SYS_WS2812_PIN  = 16;
static const uint8_t MODE_WS2812_PIN = 15;

// ===== LED对象 =====
Adafruit_NeoPixel sysRGB(1, SYS_WS2812_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel modeRGB(1, MODE_WS2812_PIN, NEO_GRB + NEO_KHZ800);

// ===== LED状态机 =====

typedef enum {
  LED_STATE_IDLE,
  LED_STATE_BLINK_ON,
  LED_STATE_BLINK_OFF,
} LedState;

struct LedController {
  LedState state;
  uint32_t stateStartTime;
  uint32_t color;
  uint16_t duration;
  uint16_t interval;
};

static LedController sysLedCtrl  = { LED_STATE_IDLE, 0, 0, 0, 0 };
static LedController modeLedCtrl = { LED_STATE_IDLE, 0, 0, 0, 0 };

// ===== 公共接口 =====

/**
 * @brief 初始化LED
 */
void ledInit() {
  sysRGB.begin();
  modeRGB.begin();
  sysRGB.setBrightness(STANDARD_BRIGHTNESS);
  modeRGB.setBrightness(STANDARD_BRIGHTNESS);
  sysRGB.clear();
  modeRGB.clear();
}

/**
 * @brief 设置LED模式（非阻塞）
 */
void ledSetMode(Adafruit_NeoPixel& myRGB, enum LEDMode mode, uint32_t color, uint16_t duration, uint16_t interval) {
  LedController* ctrl = (&myRGB == &sysRGB) ? &sysLedCtrl : &modeLedCtrl;

  ctrl->color    = color;
  ctrl->duration = duration;
  ctrl->interval = interval;

  switch (mode) {
  case LED_ON:
    myRGB.clear();
    myRGB.setPixelColor(0, color);
    myRGB.show();
    ctrl->state = LED_STATE_IDLE;
    break;

  case LED_OFF:
    myRGB.clear();
    myRGB.show();
    ctrl->state = LED_STATE_IDLE;
    break;

  case LED_BLINK:
    ctrl->state          = LED_STATE_BLINK_ON;
    ctrl->stateStartTime = millis();
    myRGB.clear();
    myRGB.setPixelColor(0, color);
    myRGB.show();
    break;

  case LED_IDLE:
    ctrl->state = LED_STATE_IDLE;
    break;
  }
}

/**
 * @brief LED状态机更新函数（非阻塞）
 */
void ledUpdate(void* pvParameter) {
  uint32_t lastCheck = 0;
  while (1) {
    // 每 1000 次循环或每 5 秒检查一次栈水位
    if (millis() - lastCheck > 5000) {
      UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(NULL);
#ifdef ARDUINO_IDE
      Serial.printf("led更新任务 Stack left: %d\n", stackHighWater);
#else
      ESP_LOGI(TAG, "Stack left: %d words", stackHighWater);
#endif
      lastCheck = millis();
    }

    // 更新系统LED
    LedController* sysCtrl        = &sysLedCtrl;
    uint32_t       sysElapsedTime = millis() - sysCtrl->stateStartTime;

    switch (sysCtrl->state) {
    case LED_STATE_BLINK_ON:
      if (sysElapsedTime >= sysCtrl->duration) {
        sysRGB.clear();
        sysRGB.show();
        sysCtrl->state          = LED_STATE_BLINK_OFF;
        sysCtrl->stateStartTime = millis();
      }
      break;
    case LED_STATE_BLINK_OFF:
      if (sysElapsedTime >= sysCtrl->interval) {
        sysRGB.clear();
        sysRGB.setPixelColor(0, sysCtrl->color);
        sysRGB.show();
        sysCtrl->state          = LED_STATE_BLINK_ON;
        sysCtrl->stateStartTime = millis();
      }
      break;

    default:
      break;
    }
    // 更新模式LED
    LedController* modeCtrl        = &modeLedCtrl;
    uint32_t       modeElapsedTime = millis() - modeCtrl->stateStartTime;

    switch (modeCtrl->state) {
    case LED_STATE_BLINK_ON:
      if (modeElapsedTime >= modeCtrl->duration) {
        modeRGB.clear();
        modeRGB.show();
        modeCtrl->state          = LED_STATE_BLINK_OFF;
        modeCtrl->stateStartTime = millis();
      }
      break;
    case LED_STATE_BLINK_OFF:
      if (modeElapsedTime >= modeCtrl->interval) {
        modeRGB.clear();
        modeRGB.setPixelColor(0, modeCtrl->color);
        modeRGB.show();
        modeCtrl->state          = LED_STATE_BLINK_ON;
        modeCtrl->stateStartTime = millis();
      }
      break;
    default:
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
