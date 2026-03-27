#include "web.h"
#include "ESPNOW.h"
#include "NTC.h"
#include "button.h"
#include "buzzer.h"
#include "current.h"
#include "esp_log.h"
#include "fan.h"
#include "fault.h"
#include "motor.h"
#include <WebServer.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG         = "web";
static const char* ap_ssid     = "crazybeans";
static const char* ap_password = "12345678";
static const int   ap_channel  = 1;
static bool        webActive   = false;

WebServer server(80);

// 注意：不再定义任何静态变量来覆盖外部全局变量

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>电推状态监控</title>
    <style>
        * { box-sizing: border-box; }
        body { font-family: 'Segoe UI', Arial, sans-serif; background: #f0f2f5; margin: 0; padding: 20px; }
        .container { max-width: 1200px; margin: auto; }
        h1 { color: #2c3e50; text-align: center; margin-bottom: 30px; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .card { background: white; border-radius: 16px; padding: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }
        .card h2 { margin-top: 0; border-bottom: 2px solid #eee; padding-bottom: 8px; color: #2c3e50; font-size: 1.3rem; }
        .value { font-size: 1.6rem; font-weight: bold; margin: 8px 0; color: #2980b9; }
        .unit { font-size: 0.8rem; color: #7f8c8d; }
        .fault { color: #e74c3c; font-weight: bold; }
        .ok { color: #27ae60; }
        .row { display: flex; justify-content: space-between; margin: 8px 0; }
    </style>
</head>
<body>
<div class="container">
    <h1>🔧 电推状态监控面板</h1>
    <div class="grid">
        <div class="card">
            <h2>⚡ 功率 & 电压</h2>
            <div class="row">总线电压: <span id="vBus">0</span> mV</div>
            <div class="row">脚踏电压: <span id="vPad">0</span> mV (<span id="vPadPct">0</span>%)</div>
            <div class="row">电流: <span id="current">0</span> mA</div>
            <div class="row">功率: <span id="power">0</span> mW</div>
        </div>
        <div class="card">
            <h2>🌡️ 温度</h2>
            <div class="row">PCB: <span id="tempPCB">0</span> °C</div>
            <div class="row">高侧MOS: <span id="tempH">0</span> °C</div>
            <div class="row">低侧MOS: <span id="tempL">0</span> °C</div>
            <div class="row">MCU: <span id="tempMCU">0</span> °C</div>
            <div class="row">脚踏MCU: <span id="tempFoot">0</span> °C</div>
        </div>
        <div class="card">
            <h2>⚠️ 故障状态</h2>
            <div class="row">H桥故障: <span id="faultH">无</span></div>
            <div class="row">斩波: <span id="chopping">--</span></div>
            <div class="row">步进故障: <span id="faultStepper">无</span></div>
            <div class="row">DRV8872故障: <span id="faultDrv">无</span></div>
        </div>
        <div class="card">
            <h2>🌀 风扇 & 电机</h2>
            <div class="row">步进速度: <span id="stepperSpeed">0</span></div>
            <div class="row">风道风扇: <span id="fanChan">0</span> %</div>
            <div class="row">散热风扇: <span id="fanHeat">0</span> %</div>
        </div>
    </div>
</div>
<script>
    function fetchData() {
        fetch('/data')
            .then(response => response.json())
            .then(data => {
                document.getElementById('vBus').innerText = data.vBus_MV;
                document.getElementById('vPad').innerText = data.vPad_mv;
                document.getElementById('vPadPct').innerText = data.vPad_percentage;
                document.getElementById('current').innerText = data.current_MA;
                document.getElementById('power').innerText = data.power_MW;
                document.getElementById('tempPCB').innerText = data.temp_PCB;
                document.getElementById('tempH').innerText = data.temp_h_mos;
                document.getElementById('tempL').innerText = data.temp_l_mos;
                document.getElementById('tempMCU').innerText = data.temp_MCU;
                document.getElementById('tempFoot').innerText = data.temp_footPadMCU;
                document.getElementById('stepperSpeed').innerText = data.stepperSpeed;
                document.getElementById('fanChan').innerText = data.fanChanSpeed;
                document.getElementById('fanHeat').innerText = data.fanHeatSpeed;
                // 故障显示
                const faultH = document.getElementById('faultH');
                faultH.innerHTML = data.isH_BridgeFault ? '<span class="fault">故障</span>' : '<span class="ok">正常</span>';
                const choppingSpan = document.getElementById('chopping');
                choppingSpan.innerHTML = data.isChopping ? '斩波中' : '未斩波';
                const faultStepper = document.getElementById('faultStepper');
                faultStepper.innerHTML = data.isStepperFault ? '<span class="fault">故障</span>' : '<span class="ok">正常</span>';
                const faultDrv = document.getElementById('faultDrv');
                faultDrv.innerHTML = data.isDrv8872Fault ? '<span class="fault">故障</span>' : '<span class="ok">正常</span>';
            })
            .catch(err => console.error(err));
    }
    setInterval(fetchData, 1000);
    fetchData();
</script>
</body>
</html>
)rawliteral";

// ---------- 处理根路由 ----------
void handleRoot() {
  server.send(200, "text/html", index_html);
}

// ---------- 处理数据路由 ----------
void handleData() {
  // 直接从各模块获取最新数据
  float vBus_MV      = getBusVoltageMV();
  float current_MA   = getCurrentMA();
  float power_MW     = getPowerMW();
  float temp_PCB     = getPCBtemp();
  float temp_h_mos   = getHighMosTemp();
  float temp_l_mos   = getLowMosTemp();
  float temp_MCU     = getChipTemp();
  float fanChanSpeed = getFanChanSpeed(); // 风道风扇速度百分比
  float fanHeatSpeed = getFanHeatSpeed(); // 散热风扇速度百分比

  uint8_t stepperSpeed = getStepSpeed(); // 步进电机速度档位

  // 读取脚踏板数据（使用与写入相同的互斥锁）
  float vPad_mv, vPad_percentage, temp_footPadMCU;
  taskENTER_CRITICAL(&esp_now_Mux); // 使用 ESPNOW.cpp 中定义的互斥锁
  vPad_mv         = FootPadData.batVoltage;
  vPad_percentage = FootPadData.batPercentage;
  temp_footPadMCU = FootPadData.footPadChipTemp;
  taskEXIT_CRITICAL(&esp_now_Mux);

  // 故障标志：根据预编译宏分别处理
  bool isH_BridgeFault_val = isH_BridgeFault;
  bool isChopping_val      = isChopping;

#ifdef TMC2209
  bool isStepperFault_val = isStepperFault;
  bool isDrv8872Fault_val = false; // 使用 DRV8872 时不存在此故障
#elif defined(DRV8872)
  bool isStepperFault_val = false; // 使用 TMC2209 时不存在此故障
  bool isDrv8872Fault_val = isDrv8872Fault;
#endif

  String json = "{";
  json += "\"vBus_MV\":" + String(vBus_MV) + ",";
  json += "\"vPad_mv\":" + String(vPad_mv) + ",";
  json += "\"vPad_percentage\":" + String(vPad_percentage) + ",";
  json += "\"current_MA\":" + String(current_MA) + ",";
  json += "\"power_MW\":" + String(power_MW) + ",";
  json += "\"temp_PCB\":" + String(temp_PCB) + ",";
  json += "\"temp_h_mos\":" + String(temp_h_mos) + ",";
  json += "\"temp_l_mos\":" + String(temp_l_mos) + ",";
  json += "\"temp_MCU\":" + String(temp_MCU) + ",";
  json += "\"temp_footPadMCU\":" + String(temp_footPadMCU) + ",";
  json += "\"isH_BridgeFault\":" + String(isH_BridgeFault_val ? 1 : 0) + ",";
  json += "\"isChopping\":" + String(isChopping_val ? 1 : 0) + ",";
  json += "\"isStepperFault\":" + String(isStepperFault_val ? 1 : 0) + ",";
  json += "\"isDrv8872Fault\":" + String(isDrv8872Fault_val ? 1 : 0) + ",";
  json += "\"stepperSpeed\":" + String(stepperSpeed) + ",";
  json += "\"fanChanSpeed\":" + String(fanChanSpeed) + ",";
  json += "\"fanHeatSpeed\":" + String(fanHeatSpeed);
  json += "}";
  server.send(200, "application/json", json);
}

// ---------- 启动 Web 服务 ----------
void startWeb() {
  if (webActive) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password, ap_channel);
  delay(100);
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  webActive = true;
  ESP_LOGI(TAG, "Web server started");
}

// ---------- 停止 Web 服务 ----------
void stopWeb() {
  if (!webActive) return;
  server.stop();
  WiFi.softAPdisconnect(true);
  webActive = false;
  ESP_LOGI(TAG, "Web server stopped");
}

// ---------- FreeRTOS 任务 ----------
void webTask(void* pvParameters) {
  while (1) {
    if (isAccelButtonLongPressed) {
      if (!webActive) startWeb();
      server.handleClient();
      vTaskDelay(pdMS_TO_TICKS(10));
    } else {
      if (webActive) stopWeb();
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}