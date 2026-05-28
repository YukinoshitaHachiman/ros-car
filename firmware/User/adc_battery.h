#ifndef __ADC_BATTERY_H
#define __ADC_BATTERY_H

#include "stm32f10x.h"

void     adc_battery_init(void);
uint16_t adc_battery_read_raw(void);
uint16_t adc_battery_read_mv(void);      // 电池电压, mV

#endif
