#include "led.h"
#include "ESPNOW.h"
#include "buzzer.h"
#include "esp_log.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

static const uint8_t SYS_WS2812_PIN  = 16;
static const uint8_t MODE_WS2812_PIN = 15;

Adafruit_NeoPixel sysRGB(1, SYS_WS2812_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel modeRGB(1, MODE_WS2812_PIN, NEO_GRB + NEO_KHZ800);

sysLedMode currentSysLedMode = STANDBY;
sysLedMode lastSysLedMode    = STANDBY;

QueueHandle_t ledQueue = xQueueCreate(10, sizeof(LedMode));

void ledInit() {
  sysRGB.begin();
  modeRGB.begin();
  sysRGB.setBrightness(STANDARD_BRIGHTNESS);
  modeRGB.setBrightness(STANDARD_BRIGHTNESS);
  sysRGB.clear();
  modeRGB.clear();
}

void sysLedTask(void* pvParameters) {
  LedMode = STANDBY;
  while (1) {
    if (xQueueReceive(ledQueue, &LedMode, portMAX_DELAY) == pdTRUE) {
      if (LedMode != lastSysLedMode) {
        switch (LedMode) {
        case STANDBY:
          sysRGB.clear();
          sysRGB.show();
          break;
        case ESP_NOW_INIT:
          /* code */
          break;
        case ESP_NOW_INIT_FAIL:
          /* code */
          break;
        case ESP_NOW_CONNECTED:
          /* code */
          break;
        case ESP_NOW_DISCONNECTED:
          /* code */
          break;
        case H_BRIDGE_FAULT:
          /* code */
          break;
        case H_BRIDGE_CHOPPING:
          /* code */
          break;
        case H_BRIDGE_OVER_HEAT:
          /* code */
          break;
        case STEP_DIAG:
          /* code */
          break;
        case STEP_OVER_HEAT:
          /* code */
          break;

        default:
          break;
        }
        lastSysLedMode = LedMode;
      }
    }
  }
}

void ModeLedTask(void* pvParameters) {
  while (1) {
    if (xQueueReceive(ledQueue, &LedMode, portMAX_DELAY) == pdTRUE) {
      sysRGB.clear();
      switch (current_ctrl_mode) {
      case HAND_MODE:
        sysRGB.setPixelColor(0, GREEN);
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        break;
      case FOOT_MODE:
        sysRGB.setPixelColor(0, BLUE);
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        break;
      case CRUISE_MODE:
        sysRGB.setPixelColor(0, YELLOW);
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        break;
      case STANDBY_MODE:
        sysRGB.setPixelColor(0, RED);
        buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        break;
      default:
        break;
      }
    }
    sysRGB.show();
  }
}
