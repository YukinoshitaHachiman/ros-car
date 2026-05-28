#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);
void led_voltage_check(uint16_t battery_mv);  // 每 50ms 调用, 自动调速

#endif
