#pragma once

#include <Arduino.h>

namespace Filters {

/**
 * @brief 一阶低通滤波器（RC低通 / 指数移动平均）
 *
 * 适用于高频实时信号（如电位器、陀螺仪等），每次调用 update() 只需传入一个原始值，
 * 内部递归计算滤波值。alpha 决定平滑程度：alpha 越大，新值权重越大，响应越快，但平滑度降低；
 * alpha 越小，平滑效果越强，但滞后增加。
 */
class LowPass {
  private:
  float filteredValue; ///< 上一次滤波后的值（状态）
  float alpha;         ///< 滤波系数，范围 (0, 1)
  bool  first;         ///< 是否为第一次更新

  public:
  /**
   * @brief 构造函数
   * @param alphaVal 滤波系数，取值范围 0.0 ~ 1.0（通常 0.01 ~ 0.5）
   *
   * 取值影响：
   * - 越接近 0，滤波越平滑，但响应越慢（滞后大）。
   * - 越接近 1，响应越快，但平滑度越低（可能残留噪声）。
   * 推荐从 0.2 开始调试，根据实际效果增减。
   */
  LowPass(float alphaVal)
      : alpha(alphaVal)
      , first(true) { }

  /**
   * @brief 更新滤波器，输入原始值，返回滤波后的值
   * @param raw 本次采样原始值（如 analogRead 结果）
   * @return float 滤波后的估计值
   */
  float update(float raw) {
    if (first) {
      filteredValue = raw;
      first         = false;
    } else {
      filteredValue = alpha * raw + (1 - alpha) * filteredValue;
    }
    return filteredValue;
  }

  /**
   * @brief 重置滤波器（清除历史状态）
   */
  void reset() { first = true; }

  /**
   * @brief 获取当前滤波值（不更新）
   */
  float getValue() const { return filteredValue; }
};

/**
 * @brief 滑动平均滤波器（非阻塞）
 *
 * 维护一个固定长度的循环缓冲区，每次添加新样本时自动更新总和。
 * 可随时调用 getAverage() 获取当前平均值（缓冲区未满时返回已有样本的平均值）。
 * 适合低频传感器（如温度、湿度），在任务中定期添加样本，需要时读取平均值。
 *
 * 注意：缓冲区在构造函数中动态分配，使用完后需调用 freeBuffer() 释放内存，
 * 或利用析构函数自动释放。
 */
class MovingAverage {
  private:
  int*    buffer; ///< 循环缓冲区
  uint8_t size;   ///< 缓冲区大小（窗口长度）
  uint8_t index;  ///< 当前写入位置
  bool    filled; ///< 缓冲区是否已填满
  long    sum;    ///< 当前缓冲区中所有样本的总和

  public:
  /**
   * @brief 构造函数
   * @param windowSize 窗口大小（采样次数），取值范围 1~255
   *
   * 窗口大小决定了平滑程度：
   * - 窗口越大，平滑效果越好，但滞后越明显（需要更多样本才能反映变化）。
   * - 窗口越小，响应越快，但可能残留噪声。
   * 对于每秒采样一次的温度，窗口取 8~16 通常足够。
   */
  MovingAverage(uint8_t windowSize)
      : size(windowSize)
      , index(0)
      , filled(false)
      , sum(0) {
    buffer = new int[size];
  }

  /**
   * @brief 析构函数，释放缓冲区内存
   */
  ~MovingAverage() {
    delete[] buffer;
  }

  /**
   * @brief 手动释放缓冲区（可选）
   */
  void freeBuffer() {
    delete[] buffer;
    buffer = nullptr;
  }

  /**
   * @brief 添加一个新样本
   * @param sample 原始采样值（如 analogRead 结果）
   */
  void addSample(int sample) {
    if (!filled) {
      // 缓冲区未满：直接添加
      buffer[index] = sample;
      sum += sample;
      index++;
      if (index >= size) {
        index  = 0;
        filled = true;
      }
    } else {
      // 缓冲区已满：覆盖最旧样本
      sum -= buffer[index];
      buffer[index] = sample;
      sum += sample;
      index = (index + 1) % size;
    }
  }

  /**
   * @brief 获取当前平均值
   * @return float 当前平均值（如果缓冲区未满，返回已有样本的平均值）
   */
  float getAverage() const {
    if (!filled) {
      return (index == 0) ? 0 : (float)sum / index;
    } else {
      return (float)sum / size;
    }
  }

  /**
   * @brief 重置滤波器（清空缓冲区）
   */
  void reset() {
    index  = 0;
    filled = false;
    sum    = 0;
  }
};

/**
 * @brief 阻塞式均值滤波器（简单平均）
 *
 * 每次调用 read() 时会连续采样指定次数并返回平均值。采样期间会阻塞当前任务。
 * 适用于低频、非实时传感器，代码简单，无需维护状态。
 * 注意：阻塞时间 = samples * analogRead 耗时（通常很短），若在实时任务中需谨慎。
 */
class BlockingAverage {
  public:
  /**
   * @brief 读取指定引脚并返回多次采样的平均值
   * @param pin     ADC 引脚编号
   * @param samples 采样次数，取值范围 1~255
   * @return float  平均值
   *
   * 采样次数越多，平滑度越高，但阻塞时间越长。通常取 5~20 次即可。
   */
  static float read(uint8_t pin, uint8_t samples) {
    long sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
      sum += analogRead(pin);
      // 可选微小延时，让 ADC 稳定（通常不需要）
      // delayMicroseconds(10);
    }
    return (float)sum / samples;
  }
};
}