#pragma once

/* 常用API:
    begin()                                 // 初始化传感器
    reset()                                 // 复位传感器到默认状态
    setMaxCurrentShunt()                    // 校准传感器，参数：最大电流（A）、检测电阻（Ω）和LSB归一化选项
    getBusVoltage()                         // 获取总线电压 返回float，单位伏特
    getShuntVoltage()                       // 获取检测电阻电压 返回float，单位伏特
    getShuntVoltage_mV()                    // 获取检测电阻电压 返回float，单位毫伏
    getCurrent()                            // 获取电流 返回float，单位安培
    getCurrent_mA()                         // 获取电流 返回float，单位毫安
    getPower()                              // 获取功率 返回float，单位瓦特
    getPower_mW()                           // 获取功率 返回float，单位毫瓦
    setAverage()                            // 设置平均采样次数，参数为INA226_AVERAGE_x_SAMPLES，x可以是1, 4, 16, 64, 128, 256, 512或1024
    setBusVoltageConversionTime()           // 设置总线电压转换时间，时间越长精度越高，参数为INA226_xxx_us，例如INA226_1100_us（默认）
    setShuntVoltageConversionTime()         // 设置检测电阻电压转换时间，参数同上
    setMode()                               // 设置工作模式，例如连续测量、触发测量等，参数为0-7，默认是7（连续测量）
    waitConversionReady()                   // 等待转换完成，返回true表示完成，false表示超时（须设置为触发测量模式）
    isConversionReady()                     // 检查转换是否完成，返回true表示完成，false表示未完成（须设置为触发测量模式）

    注：
        1、在使用 getCurrent() 和 getPower() 之前，必须先调用 setMaxCurrentShunt() 进行校准，否则读数将不准确。
        2、LSB归一化选项会自动调整当前LSB以适应最大电流和检测电阻，确保测量范围和分辨率的最佳平衡。通常不需要填写，默认即可。
        3、设置采样平均次数和转换时间可以提高测量精度，但会增加测量延迟。
        4、setMode的参数分别为:  INA226_MODE_POWER_DOWN
                                INA226_MODE_SHUT_TRIGGERED（仅检测电阻电压，触发）
                                INA226_MODE_BUS_TRIGGERED（仅总线电压，触发）
                                INA226_MODE_SHUNT_BUS_TRIGGERED（两者都测，触发）
                                INA226_MODE_SHUT_CONTINUOUS（仅检测电阻电压，连续）
                                INA226_MODE_BUS_CONTINUOUS（仅总线电压，连续）
                                INA226_MODE_SHUNT_BUS_CONTINUOUS（两者都测，连续，即默认模式）
        5、waitConversionReady 会占用MCU时间，在需要实时响应其他任务的应用中，用 isConversionReady 轮询更合适。
*/

#include "ina226.h"
#include <Arduino.h>
#include <Wire.h>

#define INA226_INIT_SUCCESS 0
#define INA226_INIT_FAIL 1
#define INA226_CALIBRATION_FAIL 2

const uint8_t sda_pin       = 13;
const uint8_t scl_pin       = 12;
const float   maxCurrent    = 10.0;       // 最大电流，单位安培
const float   H_BridgeShunt = 0.5 / 1000; // 检测电阻，单位欧

inline float busVoltage = 0.00, // 总线电压，单位伏特
    shuntVoltage        = 0.00, // 检测电阻电压，单位伏特
    H_BridgeCurrent     = 0.00, // 电流，单位安培
    power               = 0.00; // 功率，单位瓦特

inline INA226 ina(0x40); // address between 0x40 and 0x4F

/**  初始化:
 * @brief     ina226初始化函数，设置I2C引脚并校准传感器
 * @param     sda_pin:  I2C SDA引脚
 * @param     scl_pin:  I2C SCL引脚
 * @param     maxCurrent: 最大电流，单位安培
 * @param     shunt: 检测电阻，单位欧姆
 * @param     INA226_AVERAGE_x_SAMPLES: 平均采样次数，x为2的幂次方，范围1-1024
 * @return    初始化结果，0表示成功，1表示初始化失败，2表示校准失败
 */

uint8_t ina226Setup() {
  Wire.begin(sda_pin, scl_pin);
  if (ina.begin() && ina.setMaxCurrentShunt(maxCurrent, H_BridgeShunt)) {
    ina.setMode();
    ina.setAverage(INA226_64_SAMPLES);                 // 启用64次采样平均，以获得更稳定的读数
    ina.setBusVoltageConversionTime(INA226_8300_us);   // 设置最长转换时间以获得最高精度
    ina.setShuntVoltageConversionTime(INA226_8300_us); // 设置最长转换时间以获得最高精度
    return INA226_INIT_SUCCESS;
  } else if (!ina.begin()) {
    return INA226_INIT_FAIL;
  } else if (!ina.setMaxCurrentShunt(maxCurrent, H_BridgeShunt)) {
    return INA226_CALIBRATION_FAIL;
  }
  return 3; // 未知错误
}

void ina226_init() {
  while (ina226Setup() != INA226_INIT_SUCCESS) {
    if (ina226Setup() == INA226_INIT_FAIL) {
      Serial.println("INA226 初始化失败，请检查连接和参数！");
    } else if (ina226Setup() == INA226_CALIBRATION_FAIL) {
      Serial.println("INA226 校准失败，请检查参数！");
    }
    Serial.print(".");
    delay(1000);
  }
  Serial.println("INA226 初始化成功！");
}

void ina226_task(void* pvParameters) {
  while (1) {
    busVoltage      = ina.getBusVoltage();
    shuntVoltage    = ina.getShuntVoltage_mV();
    H_BridgeCurrent = ina.getCurrent();
    power           = ina.getPower();
    vTaskDelay(1000 / portTICK_PERIOD_MS); // 每秒更新一次数据
  }
}

void ina226_print_task(void* pvParameters) {
  while (1) {
    Serial.printf("Bus Voltage: %.3f V\n", busVoltage);
    Serial.printf("Shunt Voltage: %.3f mV\n", shuntVoltage);
    Serial.printf("Current: %.3f A\n", H_BridgeCurrent);
    Serial.printf("Power: %.3f W\n", power);
    Serial.println("-----------------------");
    vTaskDelay(2000 / portTICK_PERIOD_MS); // 每两秒打印一次数据
  }
}