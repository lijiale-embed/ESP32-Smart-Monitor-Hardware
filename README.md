# 基于 ESP32 与 FreeRTOS 的多任务智能环境监测系统

本项目基于 ESP32 微控制器与 FreeRTOS 实时操作系统，实现了一个高可靠性、低延迟的多任务智能环境监测原型系统。

## 📌 项目特性 (Features)
- **多任务并发调度**：基于 FreeRTOS 划分传感器采集（Producer）、屏幕显示（Consumer）与紧急中断响应三个独立任务。
- **安全的任务间通信 (IPC)**：使用 FreeRTOS 消息队列（Queue）传输自定义传感器数据结构体，避免多任务共享资源产生数据撕裂与总线冲突。
- **中断底半部机制**：利用 GPIO 外部中断（EXTI）结合二值信号量（Binary Semaphore），在 ISR 中快速释放信号并使用 `portYIELD_FROM_ISR` 实现微秒级抢占响应。
- **低 CPU 占用率**：基于阻塞式等待机制（`portMAX_DELAY`），空闲任务自动让出 CPU 算力。

## 🛠️ 硬件与引脚分配 (Hardware & Pinout)
| 外设模块 | 引脚 / 总线 | ESP32 GPIO 引脚 | 功能描述 |
| :--- | :--- | :--- | :--- |
| **DHT22** | 单总线 (1-Wire) | GPIO 4 | 采集环境温度与相对湿度 |
| **LCD1602 (I2C)** | I2C (SDA / SCL) | GPIO 21 (SDA), GPIO 22 (SCL) | 实时数据显示 |
| **Pushbutton** | 外部中断 (EXTI) | GPIO 18 (内部上拉) | 紧急报警触发中断 |

## 🏗️ 软件架构设计 (Architecture)
```text
  [ DHT22 传感器 ]               [ 外部按键 (EXTI) ]
         │ (单总线)                      │ (下降沿中断)
         ▼                              ▼
┌──────────────────┐           ┌──────────────────┐
│   TaskSensor     │           │   Button_ISR     │
│  (Priority: 1)   │           │ (IRAM_ATTR 高速) │
└────────┬─────────┘           └────────┬─────────┘
         │ xQueueSend                   │ xSemaphoreGiveFromISR
         ▼                              ▼
┌──────────────────┐           ┌──────────────────┐
│  FreeRTOS Queue  │           │ Binary Semaphore │
└────────┬─────────┘           └────────┬─────────┘
         │ xQueueReceive                │ xSemaphoreTake
         ▼                              ▼
┌──────────────────┐           ┌──────────────────┐
│   TaskDisplay    │           │  TaskEmergency   │
│  (Priority: 2)   │           │  (Priority: 3)   │
└────────┬─────────┘           └────────┬─────────┘
         ▼                              ▼
   [ LCD1602 屏幕 ]               [ 紧急报警模式 ]
