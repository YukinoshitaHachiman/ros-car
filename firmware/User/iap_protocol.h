#ifndef __IAP_PROTOCOL_H
#define __IAP_PROTOCOL_H

#include "stm32f10x.h"

void iap_protocol_init(void);
void iap_protocol_feed(uint8_t byte);

#endif
