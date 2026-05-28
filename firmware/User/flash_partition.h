#ifndef __FLASH_PARTITION_H
#define __FLASH_PARTITION_H

#include "stm32f10x.h"

// STM32F103RCT6 (high density): 256KB Flash, 2KB/page
//#define FLASH_BASE          0x08000000
#define FLASH_SIZE          (256 * 1024)
#define FLASH_PAGE_SIZE     2048
#define FLASH_PAGE_COUNT    128

// ── IAP Partition Layout ──────────────────────────────────────────
// Bootloader:    16KB,  pages 0   ~ 7   (8 pages)
// App:          110KB,  pages 8   ~ 62  (55 pages)
// Storage:      110KB,  pages 63  ~ 117 (55 pages)
// System Config: 20KB,  pages 118 ~ 127 (10 pages, reserved)

#define BOOTLOADER_ADDR     0x08000000
#define BOOTLOADER_SIZE     (16 * 1024)

#define APP_ADDR            0x08004000
#define APP_SIZE            (110 * 1024)

#define STORAGE_ADDR        0x0801F800
#define STORAGE_SIZE        (110 * 1024)

#define SYSCFG_ADDR         0x0803B000
#define SYSCFG_SIZE         (10 * 2048)

// OTA flag stored in system config area page 0
#define FW_FLAG_ADDR        SYSCFG_ADDR
#define FW_FLAG_NONE        0xFFFF
#define FW_FLAG_READY       0x5A5A

#define PAGE_ADDR(n)        (FLASH_BASE + (n) * FLASH_PAGE_SIZE)
#define ADDR_TO_PAGE(addr)  (((uint32_t)(addr) - FLASH_BASE) / FLASH_PAGE_SIZE)

#endif
