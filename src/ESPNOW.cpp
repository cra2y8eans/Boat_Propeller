#include "ESPNOW.h"
#include "NTC.h"
#include "buzzer.h"
#include "current.h"
#include "esp_log.h"
#include "led.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 定义临界区变量
// portMUX_TYPE myMux = portMUX_INITIALIZER_UNLOCKED;

esp_now_peer_info_t BoatPropeller;

static const char*    TAG                 = "ESPNOW";
static const uint16_t RECV_TIMEOUT        = 500;
static const uint8_t  FootPadMacAddr[6]   = { };
static const uint8_t  DebugMacAddr[6]     = { 0xF0, 0x9E, 0x9E, 0xAE, 0x14, 0x64 }; // C3 super mini 无排针
static volatile unsigned long  lastRecvFromDebug   = 0;
static volatile unsigned long  lastRecvFromPad     = 0;
static uint8_t        sendToPad           = 0; // 发送到脚控的数据，只是用来验证连接状态，无实际用途
static uint8_t        RecvFromDebug       = 0; // 接收来自调试设备的数据，只是用来验证连接状态，无实际用途
volatile bool         isDebugDeviceOnline = false;
volatile bool         isFootPadOnline     = false;

RecvFromFootPad_t FootPadData; // 接收来自脚控的数据

// 发送到调试设备的数据
struct sendToDebug_t {
  float vBus_MV,
      vPad_mv,
      current_MA,
      power_MW,
      temp_PCB,
      temp_h_mos,
      temp_l_mos;
};
sendToDebug_t toDebug;

// 接收回调
static void OnDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
  const uint8_t* fromMac = mac;
  // memcmp函数比较两个内存区域的前n个字节是否相同，参数为比较对象1、比较对象2、比较长度。如果相同，返回0，否则返回非0值
  if (memcmp(fromMac, FootPadMacAddr, 6) == 0) {
    // taskENTER_CRITICAL(&myMux);
    memcpy(&FootPadData, data, sizeof(FootPadData));
    isFootPadOnline = true; // 如果是脚控发来的数据，说明脚控在线
    // taskEXIT_CRITICAL(&myMux);
    lastRecvFromPad = millis();
  } else if (memcmp(fromMac, DebugMacAddr, 6) == 0) {
    // taskENTER_CRITICAL(&myMux);
    memcpy(&RecvFromDebug, data, sizeof(RecvFromDebug));
    isDebugDeviceOnline = true; // 如果是调试设备发来的数据，说明调试设备在线
    // taskEXIT_CRITICAL(&myMux);
    lastRecvFromDebug = millis();
  }
}

// 发送回调
static void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
}

void esp_now_setup() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    ESP_LOGE(TAG, "ESP NOW 初始化失败");
    ledSetMode(sysRGB, LED_BLINK, COLOR_RED, SHORT_FLASH_DURATION, SHORT_FLASH_INTERVAL);
    buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
    return;
  } else {
    ESP_LOGI(TAG, "ESP NOW 初始化成功");
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  BoatPropeller.channel = 1;
  // 添加脚控对等节点
  memcpy(BoatPropeller.peer_addr, FootPadMacAddr, 6);
  esp_now_add_peer(&BoatPropeller);
  // 添加debug对等节点
  memcpy(BoatPropeller.peer_addr, DebugMacAddr, 6);
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
  static bool          foot_last_connection_state  = false;
  static bool          debug_last_connection_state = false;
  static unsigned long last_disconnect_alert       = 0;
  const unsigned long  DISCONNECT_ALERT_INTERVAL   = 2000;
  while (1) {
    unsigned long currentTime = millis();
    isFootPadOnline           = (currentTime - lastRecvFromPad <= RECV_TIMEOUT);
    isDebugDeviceOnline       = (currentTime - lastRecvFromDebug <= RECV_TIMEOUT);

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
    // Debug 设备连接状态变化
    if (isDebugDeviceOnline && !debug_last_connection_state) {
      buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
      debug_last_connection_state = true;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void dataSent(void* pvParameters) {
  while (1) {
    if (isFootPadOnline) {
      esp_now_send(FootPadMacAddr, (uint8_t*)&sendToPad, sizeof(sendToPad));
    }
    if (isDebugDeviceOnline) {
      toDebug.vBus_MV    = getBusVoltageMV();
      toDebug.current_MA = getCurrentMA();
      toDebug.power_MW   = getPowerMW();
      toDebug.temp_PCB   = getPCBtemp();
      toDebug.temp_h_mos = getHighMosTemp();
      toDebug.temp_l_mos = getLowMosTemp();
      toDebug.vPad_mv    = FootPadData.batVoltage;
      esp_now_send(DebugMacAddr, (uint8_t*)&toDebug, sizeof(toDebug));
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}