# 功能更新记录

## 2026-05-11 - STM32 IAP 基础功能

### 已实现
- [x] delay 模块 (`User/delay.c`) — SysTick 单次计时，微秒/毫秒延时
- [x] USART 调试模块 (`User/usart_debug.c`) — USART1 初始化，环形缓冲 TX/RX，printf 重定向
- [x] Flash 分区管理 (`User/flash_partition.h`) — 定义 Bootloader/App/Storage 分区布局
- [x] 主程序 (`User/main.c`) — 帧解析、固件接收、Flash 写入存储分区

### 分区布局 (STM32F103C8, 64KB)
| 区域       | 起始地址    | 大小   | 用途             |
|------------|-------------|--------|------------------|
| Bootloader | 0x08000000  | 16 KB  | 启动 + IAP 更新   |
| App        | 0x08004000  | 16 KB  | 用户应用程序      |
| Storage    | 0x08008000  | 32 KB  | 接收固件暂存区    |

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
