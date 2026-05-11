#ifndef __FLASH_PARTITION_H
#define __FLASH_PARTITION_H

#include "stm32f10x.h"

// STM32F103C8 (medium density): 64KB Flash, 1KB/page
#define FLASH_BASE          0x08000000
#define FLASH_SIZE          (64 * 1024)
#define FLASH_PAGE_SIZE     1024
#define FLASH_PAGE_COUNT    64

// ---- IAP Partition Layout ----
// Bootloader:  16KB, pages 0  ~ 15
// App:         16KB, pages 16 ~ 31
// Storage:     32KB, pages 32 ~ 63  (firmware download buffer)

#define BOOTLOADER_ADDR     0x08000000
#define BOOTLOADER_SIZE     (16 * 1024)
#define BOOTLOADER_START    0
#define BOOTLOADER_END      15

#define APP_ADDR            0x08004000
#define APP_SIZE            (16 * 1024)
#define APP_START           16
#define APP_END             31

#define STORAGE_ADDR        0x08008000
#define STORAGE_SIZE        (32 * 1024)
#define STORAGE_START       32
#define STORAGE_END         63

// firmware status flag stored at the last page of storage area
#define FW_FLAG_ADDR        (STORAGE_ADDR + STORAGE_SIZE - FLASH_PAGE_SIZE)
#define FW_FLAG_NONE        0xFFFF
#define FW_FLAG_READY       0x5A5A

#define PAGE_ADDR(n)        (FLASH_BASE + (n) * FLASH_PAGE_SIZE)
#define ADDR_TO_PAGE(addr)  (((uint32_t)(addr) - FLASH_BASE) / FLASH_PAGE_SIZE)

#endif
