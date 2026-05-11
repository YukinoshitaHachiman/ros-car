#ifndef __USART_DEBUG_H
#define __USART_DEBUG_H

#include "stm32f10x.h"
#include <stdio.h>

#define USART_DEBUG           USART1
#define USART_DEBUG_BAUD      115200
#define USART_DEBUG_TX_BUF    256
#define USART_DEBUG_RX_BUF    512

void usart_debug_init(void);
void usart_debug_send_byte(uint8_t data);
void usart_debug_send_bytes(uint8_t *buf, uint16_t len);
uint16_t usart_debug_recv_bytes(uint8_t *buf, uint16_t max_len);
uint8_t usart_debug_rx_available(void);
void usart_debug_flush_rx(void);

#endif
