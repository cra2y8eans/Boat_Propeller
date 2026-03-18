#include "fan.h"
#include "NTC.h"
#include "buzzer.h"
#include "esp_log.h"
#include "led.h"

static const char* TAG = "FAN";

static const uint8_t fan_out_pin  = 36;
static const uint8_t fan_in_pin   = 37;
static const uint8_t fan_heat_pin = 38;

static const uint8_t  fan_inout_channel = 3;
static const uint8_t  fan_heat_channel  = 2;
static const uint16_t fan_frequency     = 15000;
static const uint8_t  fan_resolution    = 8;

// 温度阈值
static const float HEAT_START    = 40.0; // 散热片风扇启动温度
static const float HEAT_FULL     = 70.0; // 散热片风扇全速温度
static const float CHANNEL_START = 50.0; // 风道风扇启动温度
static const float CHANNEL_FULL  = 70.0; // 风道风扇全速温度
static const float HYSTERESIS    = 2.0;  // 迟滞范围
static const float ALARM_TEMP    = 70.0; // 报警阈值（可独立设定）

// 风扇控制结构体
struct FanController {
  float start_temp; // 启动温度
  float full_temp;  // 全速温度
  float hysteresis; // 迟滞范围
  bool  is_on;      // 当前开关状态（用于迟滞）
};

void Fan_Init() {
  ledcSetup(fan_inout_channel, fan_frequency, fan_resolution);
  ledcAttachPin(fan_out_pin, fan_inout_channel);
  ledcAttachPin(fan_in_pin, fan_inout_channel);
  ledcSetup(fan_heat_channel, fan_frequency, fan_resolution);
  ledcAttachPin(fan_heat_pin, fan_heat_channel);
  ledcWrite(fan_inout_channel, 0);
  ledcWrite(fan_heat_channel, 0);
}

/**
 * 更新单个风扇的占空比（纯计算，无报警）
 */
static int updateFan(FanController& fc, float temp) {
  // 迟滞判断开关状态
  if (temp > fc.start_temp + fc.hysteresis) {
    fc.is_on = true;
  } else if (temp < fc.start_temp - fc.hysteresis) {
    fc.is_on = false;
  }
  // 否则保持

  int duty = 0;
  if (fc.is_on) {
    if (temp >= fc.full_temp) {
      duty = 255;
    } else if (temp <= fc.start_temp) {
      duty = 0; // 理论上不会进入
    } else {
      // 线性映射，保留一位小数精度
      duty = map((int)(temp * 10), (int)(fc.start_temp * 10), (int)(fc.full_temp * 10), 0, 255);
    }
  }
  duty = constrain(duty, 0, 255);
  return duty;
}

// 声明芯片温度读取函数（需在别处实现）
float getChipTemp(); // 例如使用 temperatureRead()

void Fan_task(void* pvParameters) {
  static FanController heatFan   = { HEAT_START, HEAT_FULL, HYSTERESIS, false };
  static FanController chanFan   = { CHANNEL_START, CHANNEL_FULL, HYSTERESIS, false };
  uint32_t             lastCheck = 0;
  while (1) {
    // 每 1000 次循环或每 5 秒检查一次栈水位
    if (millis() - lastCheck > 5000) {
      UBaseType_t stackHighWater = uxTaskGetStackHighWaterMark(NULL);
      ESP_LOGI(TAG, "Stack left: %d words", stackHighWater);
      lastCheck = millis();
    }
    // 读取所有温度
    float pcb_temp     = getPCBtemp();
    float highMos_temp = getHighMosTemp();
    float lowMos_temp  = getLowMosTemp();
    float chip_temp    = getChipTemp();

    // 计算最大值
    float mos_max = max(pcb_temp, max(highMos_temp, lowMos_temp));
    float all_max = max(mos_max, chip_temp);

    // 更新风扇占空比
    int duty_heat = updateFan(heatFan, mos_max); // 散热片风扇只由MOS温度控制
    int duty_chan = updateFan(chanFan, all_max); // 风道风扇由所有温度控制

    ledcWrite(fan_heat_channel, duty_heat);
    ledcWrite(fan_inout_channel, duty_chan);

    // 统一报警：当最高温度（含芯片）超过阈值时触发
    if (all_max >= ALARM_TEMP) {
      buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
      ledSetMode(sysRGB, LED_BLINK, COLOR_RED, SHORT_FLASH_DURATION, SHORT_FLASH_INTERVAL);
      ESP_LOGE(TAG, "设备过热，请检查 (%.1f°C)", all_max);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}