// 在.cpp 文件的最开头（#include 之前）
// #define LOG_LOCAL_LEVEL ESP_LOG_INFO // 设置日志级别为 ESP_LOG_INFO，即使main.cpp或者ini中设置了日志级别，也会被覆盖
#include "ESPNOW.h"
#include "NTC.h"
#include "buzzer.h"
#include "current.h"
#include "esp_log.h"
#include "led.h"
#include "motor.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define ARDUINO_IDE

portMUX_TYPE esp_now_Mux = portMUX_INITIALIZER_UNLOCKED;

esp_now_peer_info_t BoatPropeller;

static const char*            TAG               = "ESPNOW";
static const uint16_t         RECV_TIMEOUT      = 500;
static const uint8_t          FootPadMacAddr[6] = { 0x08, 0xa6, 0xf7, 0x1b, 0xb2, 0xcc }; // footpad ???  08:a6:f7:1b:b2:cc
static volatile unsigned long lastRecvFromPad   = 0;
volatile bool                 isFootPadOnline   = false;

volatile RecvFromFootPad_t FootPadData; // 接收来自脚控的数据

// 接收回调
static void OnDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
  taskENTER_CRITICAL(&esp_now_Mux);
  memcpy((void*)&FootPadData, data, sizeof(FootPadData));
  isFootPadOnline = true;
  taskEXIT_CRITICAL(&esp_now_Mux);
  lastRecvFromPad = millis();
}

// 发送回调
static void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
}

void esp_now_setup() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
#ifdef ARDUINO_IDE
    Serial.println("ESP NOW 初始化失败");
#else
    ESP_LOGE(TAG, "ESP NOW 初始化失败");
#endif
    ledSetMode(sysRGB, LED_BLINK, COLOR_RED, SHORT_FLASH_DURATION, SHORT_FLASH_INTERVAL);
    buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
    return;
  } else {
#ifdef ARDUINO_IDE
    Serial.println("ESP NOW 初始化成功");
#else
    ESP_LOGI(TAG, "ESP NOW 初始化成功");
#endif
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  BoatPropeller.channel = 1;
  // 添加脚控对等节点
  memcpy(BoatPropeller.peer_addr, FootPadMacAddr, 6);
  esp_now_add_peer(&BoatPropeller);
  ledSetMode(sysRGB, LED_BLINK, COLOR_YELLOW, LONG_FLASH_DURATION, LONG_FLASH_INTERVAL); // 闪黄灯，等待连接
}

/**  ESPNOW连接检查任务
 * @brief     通过接收数据的时间戳来判断是否有设备在线
 * @param     foot_last_connection_state:     脚控上次连接状态
 * @param     debug_last_connection_state:    调试设备上次连接状态
 * @param     last_disconnect_alert:          上次掉线报警时间
 * @param     DISCONNECT_ALERT_INTERVAL:      每2秒报警一次
 * @note      通过保存和对比上次连接状态，判断设备是否重连，并调用蜂鸣器短鸣提示重连
 */
void esp_now_connection_check(void* pvParameters) {
  TickType_t           xLastWakeTime               = xTaskGetTickCount();
  const TickType_t     xPeriod                     = pdMS_TO_TICKS(100); // 延时 100ms，频率 = 1000 / 100 = 10 Hz，即每秒执行 10 次。
  static bool          foot_last_connection_state  = false;
  static bool          debug_last_connection_state = false;
  static unsigned long last_disconnect_alert       = 0;
  const unsigned long  DISCONNECT_ALERT_INTERVAL   = 2000;

  uint32_t lastCheck = 0;
  while (1) {
    // 每 1000 次循环或每 5 秒检查一次栈水位
    if (millis() - lastCheck > 5000) {
      UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(NULL);
#ifdef ARDUINO_IDE
      Serial.printf("ESP NOW 连接检查任务 Stack left: %d\n", stackHighWater);
#else
      ESP_LOGI(TAG, "Stack left: %d words", stackHighWater);
#endif
      lastCheck = millis();
    }

    unsigned long currentTime = millis();
    isFootPadOnline           = (currentTime - lastRecvFromPad <= RECV_TIMEOUT);

    // 脚控掉线：只在刚掉线时报警
    if (!isFootPadOnline && foot_last_connection_state) {
      foot_last_connection_state = false;
      ledSetMode(sysRGB, LED_BLINK, COLOR_RED, SHORT_FLASH_DURATION, SHORT_FLASH_INTERVAL);
      buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
      last_disconnect_alert = currentTime;
    }
    // 脚控重连：只在刚重连时报警
    else if (isFootPadOnline && !foot_last_connection_state) {
      foot_last_connection_state = true;
      ledSetMode(sysRGB, LED_ON, COLOR_BLUE, 0, 0);
      buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
    }
    // 脚控一直掉线：间隔性提醒
    else if (!isFootPadOnline && (currentTime - last_disconnect_alert >= DISCONNECT_ALERT_INTERVAL)) {
      ledSetMode(sysRGB, LED_BLINK, COLOR_RED, SHORT_FLASH_DURATION, SHORT_FLASH_INTERVAL);
      last_disconnect_alert = currentTime;
    }
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}

void dataSent(void* pvParameters) {
  TickType_t       xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriod       = pdMS_TO_TICKS(100); // 延时 100ms，频率 = 1000 / 100 = 10 Hz，即每秒执行 10 次。
  uint32_t         lastCheck     = 0;
  while (1) { // 每 1000 次循环或每 5 秒检查一次栈水位
    if (millis() - lastCheck > 5000) {
      UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(NULL);
#ifdef ARDUINO_IDE
      Serial.printf("数据发送 Stack left: %d\n", stackHighWater);
#else
      ESP_LOGI(TAG, "Stack left: %d words", stackHighWater);
#endif
      lastCheck = millis();
    }

    if (isFootPadOnline) {
      ControlMode mode = getCurrentCtrlMode();
      esp_now_send(FootPadMacAddr, (uint8_t*)&mode, sizeof(mode));
    }
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}