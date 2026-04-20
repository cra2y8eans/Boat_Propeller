#include "fan.h"
#include "NTC.h"
#include "buzzer.h"
#include "esp_log.h"
#include "led.h"

static const char* TAG = "FAN";

// 引脚定义
static const uint8_t fan_out_pin  = 36; // 通道风扇（开关）
static const uint8_t fan_heat_pin = 37; // 散热风扇（PWM）
static const uint8_t fan_in_pin   = 38; // 通道风扇另一个引脚（同步）

// PWM 配置
static const uint8_t  fan_heat_channel = 2; // LEDC 通道
static const uint16_t fan_frequency    = 15000;
static const uint8_t  fan_resolution   = 8;

// 散热风扇温度阈值（带迟滞）
#define HEAT_START 40.0f // 启动温度
#define HEAT_FULL 70.0f  // 全速温度
#define HYSTERESIS 2.0f  // 迟滞范围

// 通道风扇开关阈值
#define CHANNEL_ON_TEMP 70.0f  // 开启温度
#define CHANNEL_OFF_TEMP 50.0f // 关闭温度

// 报警阈值
#define ALARM_TEMP 80.0f

static int  duty_heat;               // 散热风扇 PWM 占空比
static bool channelFanState = false; // 通道风扇当前状态

// 散热风扇控制结构体
struct HeatFanController {
  float start_temp;
  float full_temp;
  float hysteresis;
  bool  is_on;
};

// 初始化
void Fan_Init() {
  // 散热风扇 PWM
  ledcSetup(fan_heat_channel, fan_frequency, fan_resolution);
  ledcAttachPin(fan_heat_pin, fan_heat_channel);
  ledcWrite(fan_heat_channel, 0);

  // 通道风扇 GPIO
  pinMode(fan_out_pin, OUTPUT);
  pinMode(fan_in_pin, OUTPUT);
  digitalWrite(fan_out_pin, LOW);
  digitalWrite(fan_in_pin, LOW);
}

// 更新散热风扇占空比（线性调速 + 迟滞）
static int updateHeatFan(HeatFanController& fc, float temp) {
  // 迟滞控制开关状态
  if (temp > fc.start_temp + fc.hysteresis) {
    fc.is_on = true;
  } else if (temp < fc.start_temp - fc.hysteresis) {
    fc.is_on = false;
  }

  int duty = 0;
  if (fc.is_on) {
    if (temp >= fc.full_temp) {
      duty = 255;
    } else if (temp <= fc.start_temp) {
      duty = 0;
    } else {
      // 线性映射
      duty = map((int)(temp * 10),
          (int)(fc.start_temp * 10),
          (int)(fc.full_temp * 10),
          0, 255);
    }
  }
  duty = constrain(duty, 0, 255);
  return duty;
}

// 获取通道风扇开关状态
bool getFanChanState() {
  return channelFanState;
}

// 获取散热风扇速度百分比
float getFanHeatSpeed() {
  return (float)duty_heat / 255.0f * 100.0f;
}

// 主任务
void Fan_task(void* pvParameters) {
  static HeatFanController heatFan = { HEAT_START, HEAT_FULL, HYSTERESIS, false };

  while (1) {
    // 读取所有温度
    float pcb_temp     = getPCBtemp();
    float highMos_temp = getHighMosTemp();
    float lowMos_temp  = getLowMosTemp();
    float chip_temp    = getChipTemp();

    // 计算最大值
    float mos_max = max(pcb_temp, max(highMos_temp, lowMos_temp));
    float all_max = max(mos_max, chip_temp);

    // 1. 散热风扇：由 MOS 温度控制 PWM
    duty_heat = updateHeatFan(heatFan, mos_max);
    ledcWrite(fan_heat_channel, duty_heat);

    // 2. 通道风扇：开关控制，带迟滞（利用 channelFanState 保持状态）
    if (all_max >= CHANNEL_ON_TEMP) {
      channelFanState = true;
    } else if (all_max <= CHANNEL_OFF_TEMP) {
      channelFanState = false;
    }
    // 中间温度保持原状态
    digitalWrite(fan_out_pin, channelFanState ? HIGH : LOW);
    digitalWrite(fan_in_pin, channelFanState ? HIGH : LOW);

    // 3. 统一报警
    if (all_max >= ALARM_TEMP) {
      buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
      ledSetMode(sysRGB, LED_BLINK, COLOR_RED, SHORT_FLASH_DURATION, SHORT_FLASH_INTERVAL);
      ESP_LOGE(TAG, "设备过热，请检查 (%.1f°C)", all_max);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}