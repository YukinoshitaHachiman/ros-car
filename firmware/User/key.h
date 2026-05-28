#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

typedef enum {
    KEY_EVT_NONE   = 0,
    KEY_EVT_SHORT  = 1,   // 短按 (<1s)
    KEY_EVT_LONG   = 2,   // 长按 (>1s)
    KEY_EVT_DOUBLE = 3    // 双击 (300ms 内再次按下)
} key_event_t;

void        key_init(void);
key_event_t key_scan(void);   // 每 10ms 调用一次

#endif
