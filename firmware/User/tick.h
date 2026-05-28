#ifndef __TICK_H
#define __TICK_H

#include "stm32f10x.h"

void tick_init(void);
uint32_t tick_get(void);
uint32_t tick_elapsed(uint32_t since);

#endif
