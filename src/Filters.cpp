#include <Filters.h> // 通过组件 include 路径引用
#include <cstring>

// 构造函数：使用默认配置
Filter::Filter() {
  setMovingAverageWindow(8);
  setMedianWindow(5);
  setLowPassCutoff(5.0f, 0.01f);
  _movingAverageIndex         = 0;
  _movingAverageCount         = 0;
  _movingAverageSum           = 0;
  _movingAverageMilliVoltsSum = 0.0f;
  _medianIndex                = 0;
  _medianCount                = 0;
  _lowPassPrevious            = 0.0f;
  _lowPassFirstCall           = true;
}

// 配置实现
void Filter::setMovingAverageWindow(uint8_t movingAverageWindow) {
  if (movingAverageWindow < 1) movingAverageWindow = 1;
  if (movingAverageWindow > MAX_WINDOW) movingAverageWindow = MAX_WINDOW;
  _movingAverageWindow        = movingAverageWindow;
  _movingAverageIndex         = 0;
  _movingAverageCount         = 0;
  _movingAverageSum           = 0;
  _movingAverageMilliVoltsSum = 0.0f;
  std::memset(_movingAverageRawBuffer, 0, sizeof(_movingAverageRawBuffer));
  std::memset(_movingAverageMvBuffer, 0, sizeof(_movingAverageMvBuffer));
}

void Filter::setMedianWindow(uint8_t medianWindow) {
  if (medianWindow < 1) medianWindow = 1;
  if (medianWindow > MAX_WINDOW) medianWindow = MAX_WINDOW;
  _medianWindow = medianWindow;
  _medianIndex  = 0;
  _medianCount  = 0;
  std::memset(_medianBuffer, 0, sizeof(_medianBuffer));
}

void Filter::setLowPassCutoff(float lowPassCutoffHz, float deltaTimeSeconds) {
  if (lowPassCutoffHz <= 0.0f) {
    _lowPassAlpha = 1.0f;
    return;
  }
  float RC      = 1.0f / (2.0f * PI * lowPassCutoffHz);
  _lowPassAlpha = deltaTimeSeconds / (RC + deltaTimeSeconds);
  if (_lowPassAlpha < 0.0f) _lowPassAlpha = 0.0f;
  if (_lowPassAlpha > 1.0f) _lowPassAlpha = 1.0f;
}

// 滤波实现（保持原有逻辑）
float Filter::updateMovingAverageRaw(int raw) {
  _movingAverageSum -= _movingAverageRawBuffer[_movingAverageIndex];
  _movingAverageRawBuffer[_movingAverageIndex] = raw;
  _movingAverageSum += raw;
  _movingAverageIndex = (_movingAverageIndex + 1) % _movingAverageWindow;
  if (_movingAverageCount < _movingAverageWindow) _movingAverageCount++;
  _lastMovingAverageRaw = (float)_movingAverageSum / (float)_movingAverageCount;
  return _lastMovingAverageRaw;
}

float Filter::updateMovingAverageVolts(float volts) {
  float milliVolts = volts * 1000.0f;
  _movingAverageMilliVoltsSum -= _movingAverageMvBuffer[_movingAverageIndex];
  _movingAverageMvBuffer[_movingAverageIndex] = milliVolts;
  _movingAverageMilliVoltsSum += milliVolts;
  _movingAverageIndex = (_movingAverageIndex + 1) % _movingAverageWindow;
  if (_movingAverageCount < _movingAverageWindow) _movingAverageCount++;
  _lastMovingAverageVolts = (_movingAverageMilliVoltsSum / (float)_movingAverageCount) / 1000.0f;
  return _lastMovingAverageVolts;
}

float Filter::updateMedianRaw(int raw) {
  _medianBuffer[_medianIndex] = raw;
  _medianIndex                = (_medianIndex + 1) % _medianWindow;
  if (_medianCount < _medianWindow) _medianCount++;
  int tmp[MAX_WINDOW];
  for (uint8_t i = 0; i < _medianCount; i++)
    tmp[i] = _medianBuffer[i];
  for (uint8_t i = 1; i < _medianCount; i++) {
    int key = tmp[i];
    int j   = i - 1;
    while (j >= 0 && tmp[j] > key) {
      tmp[j + 1] = tmp[j];
      j--;
    }
    tmp[j + 1] = key;
  }
  _lastMedianRaw = (float)tmp[_medianCount / 2];
  return _lastMedianRaw;
}

float Filter::updateLowPassRaw(int raw) {
  float value = (float)raw;
  if (_lowPassFirstCall) {
    _lowPassPrevious  = value;
    _lowPassFirstCall = false;
  }
  _lowPassPrevious = _lowPassPrevious + _lowPassAlpha * (value - _lowPassPrevious);
  _lastLowPassRaw  = _lowPassPrevious;
  return _lastLowPassRaw;
}

float Filter::updateLowPassVolts(float volts) {
  float value = volts;
  if (_lowPassFirstCall) {
    _lowPassPrevious  = value;
    _lowPassFirstCall = false;
  }
  _lowPassPrevious  = _lowPassPrevious + _lowPassAlpha * (value - _lowPassPrevious);
  _lastLowPassVolts = _lowPassPrevious;
  return _lastLowPassVolts;
}

float Filter::updateMovingAverageThenLowPassRaw(int raw) {
  float movingAverage = updateMovingAverageRaw(raw);
  if (_lowPassFirstCall) {
    _lowPassPrevious  = movingAverage;
    _lowPassFirstCall = false;
  }
  _lowPassPrevious                 = _lowPassPrevious + _lowPassAlpha * (movingAverage - _lowPassPrevious);
  _lastMovingAverageThenLowPassRaw = _lowPassPrevious;
  return _lastMovingAverageThenLowPassRaw;
}

// 别名与 getter
float Filter::movingAverageRaw(int raw) { return updateMovingAverageRaw(raw); }
float Filter::lowPassRaw(int raw) { return updateLowPassRaw(raw); }
float Filter::movingAverageVolts(float volts) { return updateMovingAverageVolts(volts); }
float Filter::lowPassVolts(float volts) { return updateLowPassVolts(volts); }

float Filter::getLastMovingAverageRaw() const { return _lastMovingAverageRaw; }
float Filter::getLastMovingAverageVolts() const { return _lastMovingAverageVolts; }
float Filter::getLastMedianRaw() const { return _lastMedianRaw; }
float Filter::getLastLowPassRaw() const { return _lastLowPassRaw; }
float Filter::getLastLowPassVolts() const { return _lastLowPassVolts; }
float Filter::getLastMovingAverageThenLowPassRaw() const { return _lastMovingAverageThenLowPassRaw; }
