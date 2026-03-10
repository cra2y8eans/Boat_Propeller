#include "ESPNOW.h"
#include "buzzer.h"
#include "esp_log.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

esp_now_peer_info_t BoatPropeller;

static const char* TAG = "ESPNOW";

static const uint8_t SWITCH_DEBOUNCE_DELAY = 20; // 按键消抖延时，单位毫秒
static const uint8_t LOG_TIME_INTERVAL     = 2000;
static const uint8_t ON_FOOT_PIN           = 8;
static const uint8_t ON_HAND_PIN           = 5;
static const uint8_t FootPadMacAddr[6]     = {};
static const uint8_t DebugMacAddr[6]       = {};
static bool          isDebugDeviceOnline   = false;
static bool          isFootPadOnline       = false;
static unsigned long lastRecvTime          = 0;

ContorlMode current_ctrl_mode = FOOT_MODE; // 默认为脚控模式
ContorlMode last_ctrl_mode    = FOOT_MODE;

QueueHandle_t control_mode_Queue = xQueueCreate(2, sizeof(current_ctrl_mode));

// 接收来自脚控的数据
struct RecvFromFootPad_t {
  uint16_t speed;
  bool     footPadData[4] = {};
};
static RecvFromFootPad_t RecvFromFootPad;

// 发送给脚控的数据
struct SendToFootPad_t {
  ContorlMode mode;
};
static SendToFootPad_t SendToFootPad;

// 接收来自调试设备的数据
static uint8_t RecvFromDebug = 0;

// 接收回调
static void OnDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
  lastRecvTime           = millis();
  const uint8_t* fromMac = mac;
  // memcmp函数比较两个内存区域的前n个字节是否相同，参数为比较对象1、比较对象2、比较长度。如果相同，返回0，否则返回非0值
  if (memcmp(fromMac, FootPadMacAddr, 6) == 0) {
    memcpy(&RecvFromFootPad, data, sizeof(RecvFromFootPad));
    isFootPadOnline = true; // 如果是脚控发来的数据，说明脚控在线
  } else if (memcmp(fromMac, DebugMacAddr, 6) == 0) {
    memcpy(&RecvFromDebug, data, sizeof(RecvFromDebug));
    isDebugDeviceOnline = true; // 如果是调试设备发来的数据，说明调试设备在线
  }
}

// 发送回调
static void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  const uint8_t* receiverMac = mac_addr;
  if (status != ESP_NOW_SEND_SUCCESS) { // 如果发送失败，说明有设备不在线
    // 通过mac地址判断是哪个设备不在线
    if (memcmp(receiverMac, DebugMacAddr, 6) == 0) isDebugDeviceOnline = false;
    if (memcmp(receiverMac, FootPadMacAddr, 6) == 0) isFootPadOnline = false;
  } else {
    // 判断哪个在线
    if (memcmp(receiverMac, DebugMacAddr, 6) != 0) isDebugDeviceOnline = true;
    if (memcmp(receiverMac, FootPadMacAddr, 6) != 0) isFootPadOnline = true;
  }
}

// 消抖读取当前模式
ContorlMode readCurrentModeWithDebounce() {
  int readHand_1 = digitalRead(ON_HAND_PIN);
  int readFoot_1 = digitalRead(ON_FOOT_PIN);
  vTaskDelay(SWITCH_DEBOUNCE_DELAY / portTICK_PERIOD_MS); // 延时20ms，消抖
  int readHand_2 = digitalRead(ON_HAND_PIN);
  int readFoot_2 = digitalRead(ON_FOOT_PIN);

  if (readHand_1 != readHand_2 || readFoot_1 != readFoot_2) return last_ctrl_mode; // 如果两次读取的值不一样，说明有抖动，返回上次的模式
  if (readHand_1 == LOW && readFoot_1 == HIGH) return HAND_MODE;                   // 手控模式
  if (readHand_1 == HIGH && readFoot_1 == LOW) return FOOT_MODE;                   // 脚控模式
  if (readHand_1 == HIGH && readFoot_1 == HIGH) return CRUISE_MODE;                // 巡航模式

  return HAND_MODE; // 默认返回手动模式
}

void modeChange() {
  if (isFootPadOnline) {
    current_ctrl_mode = readCurrentModeWithDebounce();
  } else {
    current_ctrl_mode                  = STANDBY_MODE; // 如果断线，返回待机模式
    static unsigned long lastDebugTime = 0;
    if (millis() - lastDebugTime > LOG_TIME_INTERVAL) { // 每2秒打印一次，避免刷屏
      ESP_LOGE(TAG, "脚控不在线，返回待机模式");
      lastDebugTime = millis();
    }
  }
  if (current_ctrl_mode != last_ctrl_mode) {
    SendToFootPad.mode = current_ctrl_mode; // 更新模式
    last_ctrl_mode     = current_ctrl_mode;
    if (xQueueOverwrite(control_mode_Queue, &current_ctrl_mode) != pdPASS) ESP_LOGE(TAG, "发送模式失败");
    const char* modeNames[] = { "手动模式", "脚控模式", "巡航模式", "待机模式" };
    ESP_LOGI("led模式任务", "%s\n", modeNames[newMode]);
  }
}
