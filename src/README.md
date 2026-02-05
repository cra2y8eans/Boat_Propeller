# Filters 组件（ESP32 / Arduino）

概述

- Filters 提供轻量的滤波器实现（移动均值 MA、滑动中值 MED、指数低通 LPF、以及 MA->LPF 组合）。
- 设计原则：滤波与读取分离。Filter 只对外部传入的数值做滤波；硬件采样（analogRead / 网络 / I2C）由调用方负责。
- 组件位置：src/（已从 components/filters/ 移动）
  - 头文件：src/Filters.h （在代码中使用 `#include "Filters.h"` 或 `#include <Filters.h>`，推荐 `#include "Filters.h"` 用于本地文件）
  - 实现：src/Filters.cpp

特性

- 矩阵缓冲的移动均值（窗口可配置，窗口最大 64）
- 小窗口高效的插入排序中值滤波（适合去除脉冲噪声）
- 简单稳定的指数低通滤波（基于 cutoff 与采样间隔计算 alpha）
- 组合策略：先 MA 再 LPF，可同时抑制尖峰与高频抖动
- 易于单元测试与在多种数据源（ADC/电压/网络）上复用

API 快速参考（方法 / 默认 / 取值范围 / 影响说明）

- 构造
  - `Filter()`  
    默认：movingAverageWindow = 8；medianWindow = 5；lowPassCutoffHz = 5.0 Hz；deltaTimeSeconds = 0.01s（10 ms）

- 配置
  - `void setMovingAverageWindow(uint8_t window)`  
    默认 8；取值范围 [1, 64]。窗口越大，平滑越强但响应越慢。
  - `void setMedianWindow(uint8_t window)`  
    默认 5；取值范围 [1, 64]。适合抑制偶发脉冲噪声。
  - `void setLowPassCutoff(float cutoffHz, float deltaTimeSeconds=0.01f)`  
    cutoffHz <= 0 => alpha = 1（无滤波）。dt 为采样间隔（秒），例如 0.01 表示 10 ms。cutoff 越低滤波越强、响应越慢。

- 滤波（调用者负责提供输入）
  - `float updateMovingAverageRaw(int raw)` — 输入原始 ADC（整数），返回 raw 单位均值（float）。
  - `float updateMovingAverageVolts(float volts)` — 输入电压（V），返回 V 单位均值。
  - `float updateMedianRaw(int raw)` — 输入 raw，返回窗口中值（raw）。
  - `float updateLowPassRaw(int raw)` / `float updateLowPassVolts(float volts)` — 指数低通，分别返回 raw / V 单位。
  - `float updateMovingAverageThenLowPassRaw(int raw)` — 先 MA 再 LPF（raw 单位），适合同时应对尖峰与高频噪声。
  - 结果缓存访问（不触发新采样）：`getLast...()` 系列 getter。

使用示例（Arduino / ESP32）

1. 直接对 ADC raw 滤波（短期平滑）：

```cpp
#include <filters/Filters.h>
// ...setup...
int raw = analogRead(A0);
Filter f;
f.setMovingAverageWindow(8);
float ma = f.updateMovingAverageRaw(raw);
```

2. 对电压值（V）滤波（举例低通）：

```cpp
float mv = analogReadMilliVolts(A0);
float v = mv / 1000.0f;
Filter f;
f.setLowPassCutoff(5.0f, 0.01f);
float filteredV = f.updateLowPassVolts(v);
```

3. 来自外设或网络的数据（直接使用 Volts 接口）：

```cpp
float remoteVoltage = recv_from_network(); // 单位 V
Filter f;
float out = f.updateLowPassVolts(remoteVoltage);
```

参数选择建议

- 电位器 / ADC 平滑：movingAverageWindow = 8..32
- 抑制脉冲：medianWindow = 3..7
- 连续低频平滑：LPF cutoff = 1..10 Hz（dt = 10 ms），cutoff 越小抑制越强但响应更慢

注意事项

- Filter 仅处理数值，不控制采样时序。若采样间隔不恒定，请自行以时间戳调整 dt 或在外层适配 alpha。
- 在多任务 / RTOS 环境下，如果同一 Filter 实例被多个任务访问，请自行加锁（Filter 不是线程安全的）。
- 组件已移至 `src/`（头文件：`src/Filters.h`，实现：`src/Filters.cpp`）；如果你希望作为可复用组件放回 `components/filters/`，或使用 `<filters/Filters.h>` 引入，请在 `platformio.ini` 中添加相应的 include 路径或将其放到 `lib/` 目录以便 PlatformIO 自动识别。

清理与恢复

- 项目中曾存在冗余示例文件（位于 src/ 或 examples/），已删除以减少混淆。如需恢复示例，请从版本历史检出或在 `examples/` 下重新创建。
- 若要彻底删除旧的 src 示例文件，请执行：

```
git rm src/filters_example.cpp src/FiltersObjectExample.cpp
git commit -m "chore: remove redundant example files"
```

扩展建议

- 若需线程安全或基于时间戳自适应低通（根据真实 dt 动态计算 alpha），可在 Filter 上层封装或扩展新方法。
- 若需要更多滤波器（如卡尔曼、双指数平滑），建议新增独立类并保留统一接口风格。

许可证

- 遵循项目主仓库许可协议。
