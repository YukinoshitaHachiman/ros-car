#include "stm32f10x.h"
#include "delay.h"
#include "usart_debug.h"
#include "flash_partition.h"
#include "iap_protocol.h"
#include "icm20948.h"
#include "motor.h"
#include "sysmon.h"
#include <stdio.h>

int main(void)
{
		SCB->VTOR = 0x08004000;
    uint8_t  buf[32];
    uint16_t i, n;
    uint8_t  whoami;

    delay_init();
    usart_debug_init();
    iap_protocol_init();
    icm20948_init();
    motor_init();
    sysmon_init();

    printf("\r\n==================================\r\n");
    printf("  ROSbot IAP App v1.0\r\n");
    printf("  Flash %dK  SYSCLK %dMHz\r\n",
           FLASH_SIZE / 1024, SystemCoreClock / 1000000);
    printf("  BL:  0x%08X %dK\r\n", BOOTLOADER_ADDR, BOOTLOADER_SIZE / 1024);
    printf("  APP: 0x%08X %dK\r\n", APP_ADDR, APP_SIZE / 1024);
    printf("  STO: 0x%08X %dK\r\n", STORAGE_ADDR, STORAGE_SIZE / 1024);

    whoami = icm20948_read_byte(UB_0, ICM_WHO_AM_I);
    printf("  ICM20948 WHO_AM_I: 0x%02X\r\n", whoami);
    printf("  FS: gyro=+-2000dps accel=+-16g\r\n");
    printf("  Calibration done in init\r\n");
    printf("==================================\r\n");

    motor_test();

    for (;;) {
        sysmon_update();

        n = usart_debug_recv_bytes(buf, sizeof(buf));
        for (i = 0; i < n; i++) {
            iap_protocol_feed(buf[i]);
        }

        if (icm20948_data_ready() || icm20948_data_ready_poll()) {
            icm20948_data_t imu;
            icm20948_read_sensor_data(&imu);
            printf("IMU: a(%d,%d,%d) g(%d,%d,%d) t=%d\r\n",
                   imu.accel_x, imu.accel_y, imu.accel_z,
                   imu.gyro_x, imu.gyro_y, imu.gyro_z,
                   imu.temp);
        }
    }
}
