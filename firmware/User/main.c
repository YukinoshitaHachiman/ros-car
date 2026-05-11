#include "stm32f10x.h"
#include "delay.h"
#include "usart_debug.h"
#include "flash_partition.h"
#include "iap_protocol.h"
#include <stdio.h>

int main(void)
{
    uint8_t buf[32];
    uint16_t i, n;

    delay_init();
    usart_debug_init();
    iap_protocol_init();

    printf("\r\n==================================\r\n");
    printf("  ROSbot IAP App v1.0\r\n");
    printf("  Flash %dK  SYSCLK %dMHz\r\n",
           FLASH_SIZE / 1024, SystemCoreClock / 1000000);
    printf("  BL:  0x%08X %dK\r\n", BOOTLOADER_ADDR, BOOTLOADER_SIZE / 1024);
    printf("  APP: 0x%08X %dK\r\n", APP_ADDR, APP_SIZE / 1024);
    printf("  STO: 0x%08X %dK\r\n", STORAGE_ADDR, STORAGE_SIZE / 1024);
    printf("==================================\r\n");

    for (;;) {
        n = usart_debug_recv_bytes(buf, sizeof(buf));
        for (i = 0; i < n; i++) {
            iap_protocol_feed(buf[i]);
        }
    }
}
