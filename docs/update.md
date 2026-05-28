# 功能更新记录

## 2026-05-28 - STM32 电机驱动 + 杂项外设

### 已实现 — 电机
- [x] 电机驱动模块 (`User/motor.c`) — PWM 调速 + 编码器回采
  - TIM3 全重映射 PWM: PC6/PC7 (M1), PC8/PC9 (M2), 频率 1kHz, 分辨率 1000 步
  - TIM2 编码器模式: PA15/PB3 → M1 编码器, 4x 正交解码
  - TIM4 编码器模式: PB6/PB7 → M2 编码器, 4x 正交解码
  - 占空比限制: 最大 75% (9V), 推荐 50% (6V)
  - M3/M4 预留 (引脚已定义，待后续实现)

### 已实现 — 杂项外设
- [x] tick 模块 (`User/tick.c`) — TIM6 产生 1ms 系统节拍
- [x] ADC 电池检测 (`User/adc_battery.c`) — PC4 分压采样 ADC1_IN14, 反推电源电压
- [x] 按键模块 (`User/key.c`) — PD2 低有效, 状态机检测短按/长按/双击, 10ms 扫描
- [x] OLED 屏幕 (`User/oled.c`) — SSD1306 128×64, PB10/PB11 软件 I2C, 6×8 字体
- [x] 蜂鸣器 (`User/buzzer.c`) — PC5 有源蜂鸣器, 低电压(<7V) 1s 间隔报警, 宏开关默认关闭
- [x] LED (`User/led.c`) — PC13 高电平点亮, 正常电压 1s 周期闪烁, 低电压 0.5s 周期快闪
- [x] 系统监控 (`User/sysmon.c`) — 统一调度: 按键 10ms / ADC 100ms / OLED 200ms / 蜂鸣器 100ms / LED 50ms

## 2026-05-11 - STM32 IAP 基础功能

### 已实现
- [x] delay 模块 (`User/delay.c`) — SysTick 单次计时，微秒/毫秒延时
- [x] USART 调试模块 (`User/usart_debug.c`) — USART1 初始化，环形缓冲 TX/RX，printf 重定向
- [x] Flash 分区管理 (`User/flash_partition.h`) — 定义 Bootloader/App/Storage 分区布局
- [x] Flash 操作模块 (`User/flash_ops.c`) — Flash 擦除/写入底层封装 + Storage 分区操作
- [x] IAP 协议模块 (`User/iap_protocol.c`) — CRC16 校验、帧解析状态机、命令分发
- [x] 主程序 (`User/main.c`) — 初始化 + 轮询接收字节送入协议模块 + 读取 IMU 数据
- [x] ICM20948 驱动 (`User/icm20948.c`) — SPI 模式，中断方式读取 9 轴数据
  - SPI2: PB12(CS)/PB13(SCLK)/PB14(MISO)/PB15(MOSI)，Mode 3, 4.5MHz
  - INT1: PA4 下降沿触发，脉冲模式，RAW_DATA_RDY 中断
  - 传感器配置: Gyro ±500dps, Accel ±4g

### 分区布局 (STM32F103RCT6, 256KB, 2KB/page)
| 区域          | 起始地址    | 大小   | 页范围    | 用途             |
|---------------|-------------|--------|-----------|------------------|
| Bootloader    | 0x08000000  | 16 KB  | 0~7       | 启动 + IAP 更新   |
| App           | 0x08004000  | 110 KB | 8~62      | 用户应用程序      |
| Storage       | 0x0801F800  | 110 KB | 63~117    | 接收固件暂存区    |
| System Config | 0x0803B000  | 20 KB  | 118~127   | 系统配置(最后10页) |

### 通信协议 (USART1, 115200-8-N-1)
帧格式: `AA 55 <cmd> <len_l> <len_h> [payload...] <crc16_l> <crc16_h>`

| 命令 | 值   | 说明                    |
|------|------|-------------------------|
| `E`  | 0x45 | 擦除 Storage 全区域     |
| `D`  | 0x44 | 写入一页 (含 page_idx)  |
| `F`  | 0x46 | 完成写入，置就绪标志    |
| `I`  | 0x49 | 查询分区信息            |

响应: `AA 55 <cmd> <status>` (status: 0=成功, 非0=错误码)

## 2026-05-11 - ROS 端固件升级工具

### 已实现
- [x] `tools/upgrade_firmware.py` — IAP 固件升级脚本
  - 通过串口 (USART1, 115200-8-N-1) 发送固件到 STM32
  - 自动执行: 擦除 → 分页写入 → 完成确认
  - 支持进度条、超时重试、固件列表查看
  - 固件存放目录: `tools/bin/`
  - 依赖: `pyserial` (`pip install pyserial`)
