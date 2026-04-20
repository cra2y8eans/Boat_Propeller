#include "web.h"
#include "ESPNOW.h"
#include "NTC.h"
#include "button.h"
#include "current.h"
#include "esp_log.h"
#include "fan.h"
#include "fault.h"
#include "led.h"
#include "motor.h"
#include "step.h"
#include <WebServer.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG         = "web";
static const char* ap_ssid     = "壮士～且慢";
static const char* ap_password = "12345678";
static const int   ap_channel  = 1;
static bool        webActive   = false;

static int ledBrightness = 20;

WebServer server(80);

void setLedBrightness(int brightness) {
  ledBrightness = constrain(brightness, 0, 255);
  sysRGB.setBrightness(ledBrightness);
  modeRGB.setBrightness(ledBrightness);
  sysRGB.show();
  modeRGB.show();
}

const char* getModeName(ControlMode mode) {
  switch (mode) {
  case HAND_MODE:
    return "手动模式";
  case FOOT_MODE:
    return "脚控模式";
  case CRUISE_MODE:
    return "巡航模式";
  case STANDBY_MODE:
    return "待机模式";
  default:
    return "未知";
  }
}

// ---------- HTML 页面 ----------
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
        .row { display: flex; justify-content: space-between; margin: 8px 0; }
        input[type=range] { width: 60%; margin-left: 10px; }
        button { background: #2980b9; color: white; border: none; padding: 5px 10px; border-radius: 4px; cursor: pointer; }
        button:hover { background: #1c5980; }
        .fault { color: #e74c3c; font-weight: bold; }
        .ok { color: #27ae60; }
    </style>
</head>
<body>
<div class="container">
    <h1>🔧 电推状态监控面板</h1>
    <div class="grid">
        <!-- 功率与电压 -->
        <div class="card">
            <h2>⚡ 功率 & 电压</h2>
            <div class="row">总线电压: <span id="vBus">0</span> V</div>
            <div class="row">脚控电压: <span id="vPad">0</span> V (<span id="vPadPct">0</span>%)</div>
            <div class="row">电流: <span id="current">0</span> mA</div>
            <div class="row">功率: <span id="power">0</span> mW</div>
        </div>

        <!-- 温度 + 风扇 -->
        <div class="card">
            <h2>🌡️ 温度 & 风扇</h2>
            <div class="row">PCB: <span id="tempPCB">0</span> °C</div>
            <div class="row">高侧MOS: <span id="tempH">0</span> °C</div>
            <div class="row">低侧MOS: <span id="tempL">0</span> °C</div>
            <div class="row">MCU: <span id="tempMCU">0</span> °C</div>
            <div class="row">脚控MCU: <span id="tempFoot">0</span> °C</div>
            <div class="row" style="margin-top:10px; border-top:1px dashed #ccc; padding-top:8px;">风道风扇: <span id="fanChan">关闭</span></div>
            <div class="row">散热风扇: <span id="fanHeat">0</span> %</div>
        </div>

        <!-- 控制状态 -->
        <div class="card">
            <h2>🎮 控制状态</h2>
            <div class="row">操控模式: <span id="ctrlMode">--</span></div>
            <div class="row">电机档位: <span id="motorSpeed">--</span></div>
            <div class="row">步进速度: <span id="stepperSpeed">0</span></div>
            <div class="row">LED亮度: <input type="range" id="ledSlider" min="0" max="255" value="20"> <span id="ledVal">20</span></div>
        </div>

        <!-- 步进电机高级设置 -->
        <div class="card">
            <h2>⚙️ 步进电机设置</h2>
            <div class="row">SG负载值: <span id="sgResult">0</span></div>
            <div class="row">实时电流: <span id="stepRealCurrent">0</span> mA</div>
            <div class="row">设定电流: <input type="range" id="stepCurrentSlider" min="0" max="2000" value="2000"> <span id="stepCurrentVal">2000</span> mA</div>
            <div class="row">模式: <span id="stepModeStatus">静音模式</span> <button id="toggleModeBtn">切换</button></div>
        </div>

        <!-- 故障状态 -->
        <div class="card">
            <h2>⚠️ 故障状态</h2>
            <div class="row">H桥故障: <span id="faultH">无</span></div>
            <div class="row">斩波: <span id="chopping">--</span></div>
            <div class="row">步进故障: <span id="faultStepper">无</span></div>
            <div class="row">DRV8872故障: <span id="faultDrv">无</span></div>
        </div>
    </div>
</div>
<script>
    const ledSlider = document.getElementById('ledSlider');
    const ledVal = document.getElementById('ledVal');
    const stepCurrentSlider = document.getElementById('stepCurrentSlider');
    const stepCurrentVal = document.getElementById('stepCurrentVal');
    const toggleModeBtn = document.getElementById('toggleModeBtn');

    ledSlider.oninput = function() {
        let val = this.value;
        ledVal.innerText = val;
        fetch('/setLedBrightness?value=' + val);
    };

    stepCurrentSlider.oninput = function() {
        let val = this.value;
        stepCurrentVal.innerText = val;
        fetch('/setStepCurrent?current=' + val);
    };

    toggleModeBtn.onclick = function() {
        let currentMode = document.getElementById('stepModeStatus').innerText === '静音模式' ? 1 : 0;
        let newMode = currentMode ? 0 : 1;
        fetch('/setStealthMode?mode=' + newMode).then(() => fetchData());
    };

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
                document.getElementById('fanChan').innerText = data.fanChanState ? "开启" : "关闭";
                document.getElementById('fanHeat').innerText = data.fanHeatSpeed;
                document.getElementById('ctrlMode').innerText = data.ctrlMode;
                document.getElementById('motorSpeed').innerText = data.motorSpeed;
                document.getElementById('stepperSpeed').innerText = data.stepperSpeed;
                ledSlider.value = data.ledBrightness;
                ledVal.innerText = data.ledBrightness;
                // 步进电机数据
                document.getElementById('sgResult').innerText = data.sg_result;
                document.getElementById('stepRealCurrent').innerText = data.step_real_current;
                stepCurrentSlider.value = data.step_current_setting;
                stepCurrentVal.innerText = data.step_current_setting;
                document.getElementById('stepModeStatus').innerText = data.step_stealth_mode ? "静音模式" : "高速模式";
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

// ---------- 路由处理 ----------
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleData() {
  float   vBus_MV      = getBusVoltageMV();
  float   current_MA   = getCurrentMA();
  float   power_MW     = getPowerMW();
  float   temp_PCB     = getPCBtemp();
  float   temp_h_mos   = getHighMosTemp();
  float   temp_l_mos   = getLowMosTemp();
  float   temp_MCU     = getChipTemp();
  float   fanHeatSpeed = getFanHeatSpeed();
  bool    fanChanState = getFanChanState();
  uint8_t stepperSpeed = getStepSpeed();

  // 脚控数据（使用互斥锁）
  float vPad_mv, vPad_percentage, temp_footPadMCU;
  taskENTER_CRITICAL(&esp_now_Mux);
  vPad_mv         = FootPadData.batVoltage;
  vPad_percentage = FootPadData.batPercentage;
  temp_footPadMCU = FootPadData.footPadChipTemp;
  taskEXIT_CRITICAL(&esp_now_Mux);

  // 故障标志
  bool isH_BridgeFault_val = isH_BridgeFault;
  bool isChopping_val      = isChopping;
  bool isStepperFault_val  = isStepperFault;
  bool isDrv8872Fault_val  = false;

  // 控制状态
  ControlMode mode     = getCurrentCtrlMode();
  String      modeName = getModeName(mode);
  String      motorSpeedStr;
  if (mode == HAND_MODE) {
    int8_t motorSpeed = getMotorSpeed();
    if (motorSpeed == 0)
      motorSpeedStr = "0 (停止)";
    else
      motorSpeedStr = String(motorSpeed);
  } else if (mode == FOOT_MODE || mode == CRUISE_MODE) {
    uint8_t duty    = getMotorCurrentSpeed();
    int     percent = duty * 100 / 255;
    motorSpeedStr   = String(percent) + "% (" + String(duty) + "/255)";
  } else {
    motorSpeedStr = "停止";
  }

  // 步进电机额外数据
  uint16_t sg_result            = getSG_RESULT();
  uint16_t step_real_current    = getStepCurrent();
  bool     step_stealth_mode    = getStealthChopMode();
  uint16_t step_current_setting = getStepCurrentSetting();

  // 构建 JSON
  String json = "{";
  json += "\"vBus_MV\":" + String(vBus_MV / 1000.0, 1) + ",";
  json += "\"vPad_mv\":" + String(vPad_mv / 1000.0, 1) + ",";
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
  json += "\"fanChanState\":" + String(fanChanState ? 1 : 0) + ",";
  json += "\"fanHeatSpeed\":" + String(fanHeatSpeed) + ",";
  json += "\"ctrlMode\":\"" + modeName + "\",";
  json += "\"motorSpeed\":\"" + motorSpeedStr + "\",";
  json += "\"ledBrightness\":" + String(ledBrightness) + ",";
  json += "\"sg_result\":" + String(sg_result) + ",";
  json += "\"step_real_current\":" + String(step_real_current) + ",";
  json += "\"step_stealth_mode\":" + String(step_stealth_mode ? 1 : 0) + ",";
  json += "\"step_current_setting\":" + String(step_current_setting);
  json += "}";

  server.send(200, "application/json", json);
}

void handleSetLedBrightness() {
  if (server.hasArg("value")) {
    int val = server.arg("value").toInt();
    setLedBrightness(val);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleSetStepCurrent() {
  if (server.hasArg("current")) {
    int current = server.arg("current").toInt();
    if (current >= 0 && current <= 2000) {
      setStepCurrent(current);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid current");
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleSetStealthMode() {
  if (server.hasArg("mode")) {
    int mode = server.arg("mode").toInt();
    setStealthChopMode(mode == 1);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// ---------- 启动/停止服务 ----------
void startWeb() {
  if (webActive) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password, ap_channel);
  delay(100);
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/setLedBrightness", handleSetLedBrightness);
  server.on("/setStepCurrent", handleSetStepCurrent);
  server.on("/setStealthMode", handleSetStealthMode);
  server.begin();
  webActive = true;
  ESP_LOGI(TAG, "启动Web服务器");
}

void stopWeb() {
  if (!webActive) return;
  server.stop();
  WiFi.softAPdisconnect(true);
  webActive = false;
  ESP_LOGI(TAG, "关闭Web服务器");
}

void webTask(void* pvParameters) {
  while (1) {
    if (isAccelButtonLongPressed) {
      startWeb();
      server.handleClient();
      vTaskDelay(pdMS_TO_TICKS(10));
    } else {
      stopWeb();
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}