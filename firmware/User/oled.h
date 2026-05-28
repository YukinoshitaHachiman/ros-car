#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

void oled_init(void);
void oled_clear(void);
void oled_show_string(uint8_t x, uint8_t y, const char *str);
void oled_show_num(uint8_t x, uint8_t y, uint32_t num, uint8_t digits);
void oled_refresh(void);

#endif
