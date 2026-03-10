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

static const uint8_t FootPadMacAddr[6]     = {};
static const uint8_t DebugMacAddr[6]       = {};
static unsigned long lastRecvTime          = 0;
bool                 isDebugDeviceOnline   = false;
bool                 isFootPadOnline       = false;

static uint8_t sendToPad = 0;
// 接收来自脚控的数据
struct RecvFromFootPad_t {
  uint16_t speed;
  bool     footPadData[4] = {};
};
static RecvFromFootPad_t RecvFromFootPad;

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
