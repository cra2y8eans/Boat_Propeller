#include "step.h"
#include "ESPNOW.h"
#include "FastAccelStepper.h"
#include "button.h"
#include "esp_log.h"
#include "fault.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "motor.h"

static portMUX_TYPE  step_Mux = portMUX_INITIALIZER_UNLOCKED;
static const char*   TAG      = "stepper";
static const uint8_t enPin    = 35,
                     dirPin   = 14,
                     stepPin  = 47,
                     rxPin    = 39,
                     txPin    = 42,
                     indexPin = 21,
                     uartAddr = 0;

static uint8_t     steps           = 2;    // 微步细分
static uint16_t    stepCurrent     = 2000; // 默认电流 mA
static bool        stealthChopMode = true; // true = StealthChop(静音), false = SpreadCycle(高速)
static const float Rsense          = 0.1;

TMC2209Stepper         myStepper(&Serial1, Rsense, uartAddr);
FastAccelStepperEngine TMC2209engine = FastAccelStepperEngine();
FastAccelStepper*      stepper       = NULL;

// 获取当前步进电机的负载值
uint16_t getSG_RESULT() {
  return myStepper.SG_RESULT();
}

// 获取当前步进电机的实际电流值 (mA)
uint16_t getStepCurrent() {
  return myStepper.cs2rms(myStepper.cs_actual());
}

// 设置静音模式 (true) 或高速模式 (false)
void setStealthChopMode(bool enable) {
  myStepper.en_spreadCycle(!enable); // enable=false 时使用 SpreadCycle
  stealthChopMode = enable;
  ESP_LOGI(TAG, "StealthChop mode: %s", enable ? "ON" : "OFF");
}

bool getStealthChopMode() {
  return stealthChopMode;
}

// 设置最大电流 (mA)
void setStepCurrent(uint16_t current_mA) {
  stepCurrent = current_mA;
  myStepper.rms_current(stepCurrent);
  ESP_LOGI(TAG, "Step current set to %d mA", stepCurrent);
}

uint16_t getStepCurrentSetting() {
  return stepCurrent;
}

// 速度档位映射为频率 (Hz)
static uint32_t speedLevelToHz(uint8_t level) {
  // 1档最慢，5档最快，范围可调
  return level * 60; // 60,120,180,240,300 Hz
}
void stepperEmergencyStop() {
  stepper->stopMove();
}

void stepper_init() {
  Serial1.begin(115200, SERIAL_8N1, rxPin, txPin);
  delay(500);
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(enPin, OUTPUT);
  myStepper.begin();
  myStepper.toff(4);
  myStepper.rms_current(stepCurrent);
  myStepper.microsteps(steps);                // 1/2微步
  myStepper.en_spreadCycle(!stealthChopMode); // 默认静音模式
  myStepper.SGTHRS(0);                        // 关闭 StallGuard 阈值
  myStepper.TCOOLTHRS(0);                     // 关闭 StallGuard 阈值
  // myStepper.TCOOLTHRS(0xFFFFF);               // 启用所有速度下的 StallGuard
  ESP_LOGI(TAG, "TMC2209 初始化完成");

  TMC2209engine.init();
  stepper = TMC2209engine.stepperConnectToPin(stepPin);
  if (stepper) {
    stepper->setDirectionPin(dirPin);
    stepper->setEnablePin(enPin);
    stepper->setAutoEnable(true);             // 是否自动使能
    stepper->setSpeedInHz(speedLevelToHz(3)); // 设置转速，默认3档。单位HZ，计算的是一个完整周期的时间
    stepper->setAcceleration(800);            // 缓启缓停，设置加减速度（步/秒²），用于 moveTo / move 模式下的加减速
    stepper->setDelayToEnable(50);            // 延迟使能，单位 ms
    stepper->setDelayToDisable(30 * 1000);    // 延迟禁用使能，单位 ms
    ESP_LOGI(TAG, "FastAccelStepper 初始化完成");
  }
}

void stepper_control_task(void* pvParameter) {
  TickType_t       xLastWakeTime  = xTaskGetTickCount();
  const TickType_t xPeriod        = pdMS_TO_TICKS(1);
  uint8_t          lastSpeedLevel = 0;

  while (1) {
    taskENTER_CRITICAL(&step_Mux);
    ControlMode mode       = getCurrentCtrlMode();
    bool        turnLeft   = FootPadData.data[0];
    bool        turnRight  = FootPadData.data[1];
    bool        dirReverse = isAccelButtonLongPressed;
    taskEXIT_CRITICAL(&step_Mux);

    uint8_t speedLevel = getStepSpeed(); // 1~5
    if (speedLevel != lastSpeedLevel && stepper) {
      uint32_t freq = speedLevelToHz(speedLevel);
      stepper->setSpeedInHz(freq);
      lastSpeedLevel = speedLevel;
      ESP_LOGI(TAG, "Speed level changed to %d, freq=%d Hz", speedLevel, freq);
    }

    if (mode != HAND_MODE && !isStepperFault) {
      if (turnLeft && !turnRight) {
        stepper->runForward();
      } else if (!turnLeft && turnRight) {
        stepper->runBackward();
      } else {
        stepper->stopMove();
      }
    } else {
      stepper->stopMove();
    }
    // // 如果步进电机发生故障，通过长按减速按钮尝试重新使能来复位驱动器
    // if (isStepperFault) {
    //   if (isDecelButtonLongPressed) {
    //     digitalWrite(enPin, HIGH);
    //     vTaskDelay(pdMS_TO_TICKS(100));
    //     digitalWrite(enPin, LOW);
    //   }
    // }
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}