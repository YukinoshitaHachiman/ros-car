#ifndef __ICM20948_H
#define __ICM20948_H

#include "stm32f10x.h"

// ── Pin mapping ─────────────────────────────────────────────────────
// SPI2: PB12(NSS), PB13(SCLK), PB14(MISO/SDO), PB15(MOSI/SDI)
// INT1: PA4

// ── Register bank select ────────────────────────────────────────────
#define ICM_BANK_SEL      0x7F

// ── Bank 0 registers ────────────────────────────────────────────────
#define ICM_WHO_AM_I      0x00
#define ICM_USER_CTRL     0x03
#define ICM_PWR_MGMT_1    0x06
#define ICM_INT_PIN_CFG   0x0F
#define ICM_INT_ENABLE    0x10
#define ICM_ACCEL_XOUT_H  0x2D   // 14 bytes: accel(6) + gyro(6) + temp(2)
#define ICM_GYRO_XOUT_H   0x33
#define ICM_TEMP_OUT_H    0x39

// ── Bank 2 registers ────────────────────────────────────────────────
#define ICM_GYRO_CONFIG_1 0x01
#define ICM_ACCEL_CONFIG  0x14

// ── PWR_MGMT_1 bits ─────────────────────────────────────────────────
#define ICM_PWR_RESET     0x80
#define ICM_PWR_SLEEP     0x40
#define ICM_PWR_CYCLE     0x20
#define ICM_CLKSEL_AUTO   0x01

// ── INT_PIN_CFG bits ────────────────────────────────────────────────
#define ICM_INT1_ACTL     0x80   // 1=active low
#define ICM_INT1_OPEN     0x40   // 1=open drain
#define ICM_INT1_LATCH    0x20   // 1=latch, 0=50us pulse
#define ICM_ANYRD_CLR     0x10   // 1=any read clears

// ── INT_ENABLE bits ─────────────────────────────────────────────────
#define ICM_RAW_RDY_EN    0x01

// ── Gyro full-scale (GYRO_CONFIG_1[2:0]) ────────────────────────────
#define ICM_GYRO_FS_250   0x00
#define ICM_GYRO_FS_500   0x01
#define ICM_GYRO_FS_1000  0x02
#define ICM_GYRO_FS_2000  0x03

// ── Accel full-scale (ACCEL_CONFIG[1:0]) ────────────────────────────
#define ICM_ACCEL_FS_2G   0x00
#define ICM_ACCEL_FS_4G   0x01
#define ICM_ACCEL_FS_8G   0x02
#define ICM_ACCEL_FS_16G  0x03

// ── Sensor data structure ───────────────────────────────────────────
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temp;
} icm20948_data_t;

// ── Public API ──────────────────────────────────────────────────────
void icm20948_init(void);
uint8_t icm20948_read_byte(uint8_t reg);
void icm20948_write_byte(uint8_t reg, uint8_t data);
void icm20948_read_buf(uint8_t reg, uint8_t *buf, uint8_t len);
void icm20948_read_sensor_data(icm20948_data_t *data);
uint8_t icm20948_data_ready(void);

// Called from ISR
void icm20948_irq_handler(void);

#endif
