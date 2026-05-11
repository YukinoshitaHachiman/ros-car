#ifndef __FLASH_OPS_H
#define __FLASH_OPS_H

#include "stm32f10x.h"

// Low-level flash operations
uint8_t flash_erase_page(uint32_t addr);
uint8_t flash_write_buf(uint32_t addr, uint8_t *data, uint16_t len);

// Storage partition operations
uint8_t storage_erase_all(void);
uint8_t storage_write_page(uint8_t idx, uint8_t *data);
uint8_t storage_set_ready_flag(void);

#endif
