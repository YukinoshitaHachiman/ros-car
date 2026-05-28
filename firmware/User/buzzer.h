#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"

// 功能开关：默认关闭，取消注释以启用
// #define BUZZER_ENABLE

void buzzer_init(void);
void buzzer_on(void);
void buzzer_off(void);
void buzzer_alarm_check(uint16_t battery_mv);  // 低电压检测，每 100ms 调用

#endif
