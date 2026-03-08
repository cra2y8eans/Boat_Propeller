Filters示例文件


// ==================== 引脚定义 ====================
#define POT_PIN     34   // 电位器
#define TEMP_PIN    35   // 温度传感器
#define BATT_PIN    36   // 电池电压检测

// ==================== FreeRTOS 任务 ====================

/**
 * @brief 电位器采样任务（高频，50Hz）
 * 
 * 使用 LowPass 滤波器，alpha=0.3
 * 滤波后的值通过队列发送给其他任务（示例中未实现队列，仅串口打印）
 */
void potTask(void *pvParameters) {
    // 每个任务拥有自己的滤波器实例，互不干扰 但相互之间也不能调用参数！！
    Filters::LowPass potFilter(0.3);
    
    TickType_t xLastWake = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(20); // 20ms -> 50Hz

    for (;;) {
        vTaskDelayUntil(&xLastWake, xPeriod);

        int raw = analogRead(POT_PIN);
        float speed = potFilter.update(raw);

        // 实际应用中可通过队列发送给电机控制任务
        Serial.printf("Pot: raw=%d, filtered=%.2f\n", raw, speed);
    }
}

/**
 * @brief 温度采集任务（低频，非阻塞滑动平均）
 * 
 * 每125ms采样一次，8次后（约1秒）计算一次平均值
 */
void tempTask(void *pvParameters) {
    Filters::MovingAverage tempFilter(8);  // 8点滑动平均
    
    TickType_t xLastWake = xTaskGetTickCount();
    const TickType_t xSamplePeriod = pdMS_TO_TICKS(125); // 125ms

    uint8_t count = 0;

    for (;;) {
        vTaskDelayUntil(&xLastWake, xSamplePeriod);

        int raw = analogRead(TEMP_PIN);
        tempFilter.addSample(raw);
        
        count++;
        if (count >= 8) {  // 每秒输出一次
            count = 0;
            float temp = tempFilter.getAverage();
            // 实际应用中可通过队列共享给风扇控制任务
            Serial.printf("Temp: avg=%.2f\n", temp);
        }
    }
}

/**
 * @brief 电池电量采集任务（极低频，阻塞均值）
 * 
 * 每5秒采集一次，每次采样10次取平均
 */
void battTask(void *pvParameters) {
    TickType_t xLastWake = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(5000); // 5秒

    for (;;) {
        vTaskDelayUntil(&xLastWake, xPeriod);

        // 直接调用静态方法，阻塞采样10次
        float batt = Filters::BlockingAverage::read(BATT_PIN, 10);
        
        Serial.printf("Battery: %.2f\n", batt);
    }
}

// ==================== Arduino 入口 ====================

void setup() {
    Serial.begin(115200);
    delay(1000); // 等待串口稳定
    Serial.println("Filter Example Started");

    // 配置ADC分辨率（ESP32-S3默认12位）
    analogReadResolution(12);

    // 创建FreeRTOS任务
    xTaskCreatePinnedToCore(
        potTask,    // 任务函数
        "PotTask",  // 任务名称
        4096,       // 栈大小（字节）
        NULL,       // 任务参数
        2,          // 优先级（高）
        NULL,       // 任务句柄
        0           // 运行核心（0或1，ESP32双核）
    );

    xTaskCreatePinnedToCore(
        tempTask,
        "TempTask",
        4096,
        NULL,
        1,          // 优先级（中）
        NULL,
        0
    );

    xTaskCreatePinnedToCore(
        battTask,
        "BattTask",
        4096,
        NULL,
        1,          // 优先级（中）
        NULL,
        1           // 放在另一个核心
    );

    // 注意：创建任务后，不需要删除当前任务，vTaskStartScheduler() 会被自动调用
}

void loop() {
    // FreeRTOS 下，loop() 可以留空，或放一些低优先级处理
    vTaskDelay(pdMS_TO_TICKS(1000));
}