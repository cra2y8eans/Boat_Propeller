#include "led.h"
#include "buzzer.h"
#include "esp_log.h"
#include "motor.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define STANDARD_BRIGHTNESS 100

static const uint8_t SYS_WS2812_PIN  = 16;
static const uint8_t MODE_WS2812_PIN = 15;

Adafruit_NeoPixel sysRGB(1, SYS_WS2812_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel modeRGB(1, MODE_WS2812_PIN, NEO_GRB + NEO_KHZ800);

sysLedMode LedMode = STANDBY;

QueueHandle_t ledQueue = xQueueCreate(10, sizeof(sysLedMode));

void ledInit() {
  sysRGB.begin();
  modeRGB.begin();
  sysRGB.setBrightness(STANDARD_BRIGHTNESS);
  modeRGB.setBrightness(STANDARD_BRIGHTNESS);
  sysRGB.clear();
  modeRGB.clear();
}

void sysLedTask(void* pvParameters) {
  while (1) {
    if (xQueueReceive(ledQueue, &LedMode, portMAX_DELAY) == pdTRUE) {
      switch (LedMode) {
      case STANDBY:
        sysRGB.clear();
        sysRGB.show();
        break;
      case ESP_NOW_INIT_SUCCESS:
        sysRGB.clear();
        sysRGB.setPixelColor(0, COLOR_RED);
        sysRGB.show();
        vTaskDelay(LONG_FLASH_DURATION / portTICK_PERIOD_MS);
        sysRGB.clear();
        sysRGB.show();
        vTaskDelay(LONG_FLASH_INTERVAL / portTICK_PERIOD_MS);
        sysRGB.clear();
        sysRGB.setPixelColor(0, COLOR_GREEN);
        sysRGB.show();
        vTaskDelay(LONG_FLASH_DURATION / portTICK_PERIOD_MS);
        sysRGB.clear();
        sysRGB.show();
        vTaskDelay(LONG_FLASH_INTERVAL / portTICK_PERIOD_MS);
        sysRGB.clear();
        sysRGB.setPixelColor(0, COLOR_BLUE);
        sysRGB.show();
        vTaskDelay(LONG_FLASH_DURATION / portTICK_PERIOD_MS);
        sysRGB.clear();
        sysRGB.show();
        vTaskDelay(LONG_FLASH_INTERVAL / portTICK_PERIOD_MS);
        break;
      case ESP_NOW_INIT_FAIL:
        sysRGB.clear();
        sysRGB.setPixelColor(0, COLOR_RED);
        sysRGB.show();
        vTaskDelay(LONG_FLASH_DURATION / portTICK_PERIOD_MS);
        sysRGB.clear();
        sysRGB.show();
        vTaskDelay(LONG_FLASH_INTERVAL / portTICK_PERIOD_MS);
        break;
      case ESP_NOW_CONNECTED:
        sysRGB.clear();
        sysRGB.setPixelColor(0, COLOR_BLUE);
        sysRGB.show();
        break;
      case ESP_NOW_DISCONNECTED:
        sysRGB.clear();
        sysRGB.setPixelColor(0, COLOR_RED);
        sysRGB.show();
        break;
      case H_BRIDGE_FAULT:
        sysRGB.clear();
        sysRGB.setPixelColor(0, COLOR_RED);
        sysRGB.show();
        vTaskDelay(SHORT_FLASH_DURATION / portTICK_PERIOD_MS);
        sysRGB.clear();
        sysRGB.show();
        vTaskDelay(SHORT_FLASH_INTERVAL / portTICK_PERIOD_MS);
        break;
      case H_BRIDGE_CHOPPING:
        sysRGB.clear();
        sysRGB.setPixelColor(0, COLOR_YELLOW);
        sysRGB.show();
        vTaskDelay(LONG_FLASH_DURATION / portTICK_PERIOD_MS);
        sysRGB.clear();
        sysRGB.show();
        vTaskDelay(LONG_FLASH_INTERVAL / portTICK_PERIOD_MS);
        break;
      case OVER_HEAT:
        sysRGB.clear();
        sysRGB.setPixelColor(0, COLOR_RED);
        sysRGB.show();
        vTaskDelay(SHORT_FLASH_DURATION / portTICK_PERIOD_MS);
        sysRGB.clear();
        sysRGB.show();
        vTaskDelay(SHORT_FLASH_INTERVAL / portTICK_PERIOD_MS);
        sysRGB.clear();
        sysRGB.setPixelColor(0, COLOR_YELLOW);
        sysRGB.show();
        vTaskDelay(SHORT_FLASH_DURATION / portTICK_PERIOD_MS);
        sysRGB.clear();
        sysRGB.show();
        vTaskDelay(SHORT_FLASH_INTERVAL / portTICK_PERIOD_MS);
        break;
      case STEP_DIAG:
        sysRGB.clear();
        sysRGB.setPixelColor(0, COLOR_CYAN);
        sysRGB.show();
        vTaskDelay(LONG_FLASH_DURATION / portTICK_PERIOD_MS);
        sysRGB.clear();
        sysRGB.show();
        vTaskDelay(LONG_FLASH_INTERVAL / portTICK_PERIOD_MS);
        break;

      default:
        break;
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void ModeLedTask(void* pvParameters) {
  while (1) {
    static ControlMode currentMode, lastMode;
    currentMode = getCurrentCtrlMode();
    if (currentMode != lastMode) {
      modeRGB.clear();
      switch (currentMode) {
      case HAND_MODE:
        modeRGB.setPixelColor(0, COLOR_GREEN);
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        break;
      case FOOT_MODE:
        modeRGB.setPixelColor(0, COLOR_BLUE);
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        break;
      case CRUISE_MODE:
        modeRGB.setPixelColor(0, COLOR_YELLOW);
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        break;
      case STANDBY_MODE:
        modeRGB.setPixelColor(0, COLOR_RED);
        buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
        break;
      default:
        break;
      }
      lastMode = currentMode;
    }
    modeRGB.show();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
