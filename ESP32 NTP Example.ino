#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <DHTesp.h>

// ==================== 1. 硬件引脚与外设定义 ====================
#define DHT_PIN      4   // DHT22 传感器数据引脚
#define BUTTON_PIN   18  // 紧急报警按键引脚

LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C LCD 屏幕对象 (地址 0x27)
DHTesp dhtSensor;                    // DHT22 传感器驱动对象

// ==================== 2. 核心数据结构定义 ====================
// 使用结构体打包传感器数据
typedef struct {
  float temperature; // 温度 (°C)
  float humidity;    // 湿度 (%)
} SensorData_t;

// ==================== 3. FreeRTOS 句柄定义 ====================
QueueHandle_t     sensorQueue;  // 传感器数据传输队列
SemaphoreHandle_t btnSemaphore; // 按键中断二值信号量

// ==================== 4. 外部中断服务函数 (ISR) ====================
// IRAM_ATTR: 将中断函数放入内部高速 SRAM 运行
void IRAM_ATTR Button_ISR() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  // 在中断中释放二值信号量，唤醒紧急报警任务
  xSemaphoreGiveFromISR(btnSemaphore, &xHigherPriorityTaskWoken);

  // 如果唤醒了更高优先级的任务，中断退出后立即强制切换 CPU
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// ==================== 5. 任务实现 ====================

// 任务1：传感器周期采集任务 (生产者, 优先级 1)
void TaskSensor(void *pvParameters) {
  SensorData_t sensorPacket;

  for (;;) {
    // 1. 读取温湿度硬件数据
    TempAndHumidity newValues = dhtSensor.getTempAndHumidity();
    sensorPacket.temperature = newValues.temperature;
    sensorPacket.humidity = newValues.humidity;

    // 2. 将结构体数据通过指针地址(&)推入队列
    xQueueSend(sensorQueue, &sensorPacket, portMAX_DELAY);

    // 3. 串口打印调试信息
    Serial.printf("[Task-Sensor] 采集完成 -> 温度: %.1f °C, 湿度: %.1f %%\n",
                  sensorPacket.temperature, sensorPacket.humidity);

    // 4. 非阻塞延时 2 秒
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

// 任务2：屏幕数据显示任务 (消费者, 优先级 2)
void TaskDisplay(void *pvParameters) {
  SensorData_t displayData;

  for (;;) {
    // 等待队列数据（队列为空时任务自动休眠挂起，0% CPU 占用）
    if (xQueueReceive(sensorQueue, &displayData, portMAX_DELAY) == pdPASS) {
      Serial.printf("[Task-Display] 收到数据，刷新LCD屏幕...\n");

      // 刷新第一行：温度
      lcd.setCursor(0, 0);
      lcd.printf("Temp: %.1f C    ", displayData.temperature);

      // 刷新第二行：湿度
      lcd.setCursor(0, 1);
      lcd.printf("Humi: %.1f %%    ", displayData.humidity);
    }
  }
}

// 任务3：紧急事件处理任务 (最高优先级 3)
void TaskEmergency(void *pvParameters) {
  for (;;) {
    // 等待中断发来的信号量（平时一直休眠）
    if (xSemaphoreTake(btnSemaphore, portMAX_DELAY) == pdTRUE) {
      Serial.println("\n>>> [Task-Emergency] 响应硬件中断！执行紧急报警！<<<");

      // 强行抢占屏幕显示报警内容
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("! SYSTEM ALARM !");
      lcd.setCursor(0, 1);
      lcd.print("EMERGENCY STOP");

      // 报警保持 2.5 秒
      vTaskDelay(2500 / portTICK_PERIOD_MS);
      lcd.clear();
    }
  }
}

// ==================== 6. 系统初始化 ====================
void setup() {
  Serial.begin(115200);
  Serial.println("\n========== 系统初始化启动 ==========");

  // 1. 初始化 LCD 屏幕
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Booting...");

  // 2. 初始化 DHT22 传感器
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  // 3. 创建 FreeRTOS 通信组件
  sensorQueue  = xQueueCreate(5, sizeof(SensorData_t)); // 容量 5 的结构体队列
  btnSemaphore = xSemaphoreCreateBinary();              // 二值信号量

  // 4. 配置按键引脚与外部中断 (下降沿触发)
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), Button_ISR, FALLING);

  // 5. 创建 3 个并发任务
  xTaskCreate(TaskSensor,    "Sensor_Task",    3072, NULL, 1, NULL);
  xTaskCreate(TaskDisplay,   "Display_Task",   2048, NULL, 2, NULL);
  xTaskCreate(TaskEmergency, "Emergency_Task", 2048, NULL, 3, NULL);

  Serial.println("========== FreeRTOS 调度器运行中 ==========\n");
}

void loop() {
  // FreeRTOS 多任务环境下，loop 保持休眠
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
