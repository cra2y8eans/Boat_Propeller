#pragma once
#include <Arduino.h>

/*
  filters/Filters.h - 组件化的滤波模块头文件（仅声明与文档）
  使用方式：
    #include <filters/Filters.h>
  说明：
    - 组件位置： components/filters/
    - include 目录： components/filters/include/
    - 实现位于： components/filters/src/Filters.cpp
    - Filter 只负责滤波（不做硬件读取），调用者负责读取 ADC / 传感器 / 网络数据后将值传入 updateXXX 方法
*/

class Filter {
  public:
  // 构造：默认 movingAverageWindow=8, medianWindow=5, lowPassCutoffHz=5.0Hz, deltaTimeSeconds=0.01s
  Filter();

  // 配置
  void setMovingAverageWindow(uint8_t movingAverageWindow);
  void setMedianWindow(uint8_t medianWindow);
  void setLowPassCutoff(float lowPassCutoffHz, float deltaTimeSeconds = 0.01f);

  // 滤波接口（调用者传入数据）
  float updateMovingAverageRaw(int raw);
  float updateMovingAverageVolts(float volts);
  float updateMedianRaw(int raw);
  float updateLowPassRaw(int raw);
  float updateLowPassVolts(float volts);
  float updateMovingAverageThenLowPassRaw(int raw);

  // 便捷别名
  float movingAverageRaw(int raw);
  float lowPassRaw(int raw);
  float movingAverageVolts(float volts);
  float lowPassVolts(float volts);

  // 获取上次结果（不触发新采样）
  float getLastMovingAverageRaw() const;
  float getLastMovingAverageVolts() const;
  float getLastMedianRaw() const;
  float getLastLowPassRaw() const;
  float getLastLowPassVolts() const;
  float getLastMovingAverageThenLowPassRaw() const;

  private:
  static const uint8_t MAX_WINDOW = 64;
  // ...internal buffers and state...
  int     _movingAverageRawBuffer[MAX_WINDOW];
  float   _movingAverageMvBuffer[MAX_WINDOW];
  uint8_t _movingAverageWindow        = 8;
  uint8_t _movingAverageIndex         = 0;
  uint8_t _movingAverageCount         = 0;
  long    _movingAverageSum           = 0;
  float   _movingAverageMilliVoltsSum = 0.0f;

  int     _medianBuffer[MAX_WINDOW];
  uint8_t _medianWindow = 5;
  uint8_t _medianIndex  = 0;
  uint8_t _medianCount  = 0;

  float _lowPassAlpha     = 1.0f;
  float _lowPassPrevious  = 0.0f;
  bool  _lowPassFirstCall = true;

  float _lastMovingAverageRaw            = 0.0f;
  float _lastMovingAverageVolts          = 0.0f;
  float _lastMedianRaw                   = 0.0f;
  float _lastLowPassRaw                  = 0.0f;
  float _lastLowPassVolts                = 0.0f;
  float _lastMovingAverageThenLowPassRaw = 0.0f;
};
