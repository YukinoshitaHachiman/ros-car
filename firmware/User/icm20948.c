#include "icm20948.h"
#include "delay.h"
#include <stdio.h>

// ── SPI & CS macros ──────────────────────────────────────────────────
#define ICM_CS_LOW()   GPIO_ResetBits(GPIOB, GPIO_Pin_12)
#define ICM_CS_HIGH()  GPIO_SetBits(GPIOB, GPIO_Pin_12)
#define ICM_SPI        SPI2
#define ICM_READ       0x80

// ── Private state ───────────────────────────────────────────────────
static volatile uint8_t data_ready = 0;
static int16_t gyro_bias_x = 0;
static int16_t gyro_bias_y = 0;
static int16_t gyro_bias_z = 0;
static int16_t accel_bias_x = 0;
static int16_t accel_bias_y = 0;
static int16_t accel_bias_z = 0;
static int16_t accel_scale_z = 0;

// ── SPI byte transfer ───────────────────────────────────────────────
static uint8_t spi_transfer(uint8_t tx)
{
    while (SPI_I2S_GetFlagStatus(ICM_SPI, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(ICM_SPI, tx);
    while (SPI_I2S_GetFlagStatus(ICM_SPI, SPI_I2S_FLAG_RXNE) == RESET);
    return (uint8_t)SPI_I2S_ReceiveData(ICM_SPI);
}

// ── Fast read (no bank select — for data path) ──────────────────────
static uint8_t icm_read_fast(uint8_t reg)
{
    uint8_t val;
    ICM_CS_LOW();
    spi_transfer(reg | ICM_READ);
    val = spi_transfer(0xFF);
    ICM_CS_HIGH();
    return val;
}

static void icm_read_buf_fast(uint8_t reg, uint8_t *buf, uint8_t len)
{
    ICM_CS_LOW();
    spi_transfer(reg | ICM_READ);
    while (len--) {
        *buf++ = spi_transfer(0xFF);
    }
    ICM_CS_HIGH();
}

// ── Bank select (standalone SPI transaction) ────────────────────────
static void icm_select_bank(icm_bank_t bank)
{
    ICM_CS_LOW();
    spi_transfer(ICM_BANK_SEL);
    spi_transfer((bank << 4) & 0x30);
    ICM_CS_HIGH();
    delay_us(50);
}

// ── Public: register access with bank select ────────────────────────
uint8_t icm20948_read_byte(icm_bank_t bank, uint8_t reg)
{
    icm_select_bank(bank);
    return icm_read_fast(reg);
}

void icm20948_write_byte(icm_bank_t bank, uint8_t reg, uint8_t data)
{
    icm_select_bank(bank);
    ICM_CS_LOW();
    spi_transfer(reg & 0x7F);
    spi_transfer(data);
    ICM_CS_HIGH();
}

// ── GPIO init ───────────────────────────────────────────────────────
static void icm_gpio_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    // PB12(CS) — push-pull output (software NSS)
    gpio.GPIO_Pin   = GPIO_Pin_12;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    // PB13(SCLK), PB15(MOSI) — AF push-pull
    gpio.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_15;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    // PB14(MISO) — input floating
    gpio.GPIO_Pin  = GPIO_Pin_14;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio);

    // PA4(INT1) — input pull-up
    gpio.GPIO_Pin  = GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &gpio);
}

// ── SPI init ────────────────────────────────────────────────────────
static void icm_spi_init(void)
{
    SPI_InitTypeDef spi;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    SPI_StructInit(&spi);
    spi.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode              = SPI_Mode_Master;
    spi.SPI_DataSize          = SPI_DataSize_8b;
    spi.SPI_CPOL              = SPI_CPOL_Low;
    spi.SPI_CPHA              = SPI_CPHA_1Edge;
    spi.SPI_NSS               = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    spi.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_Init(ICM_SPI, &spi);

    SPI_Cmd(ICM_SPI, ENABLE);
    ICM_CS_HIGH();
}

// ── EXTI init ───────────────────────────────────────────────────────
static void icm_exti_init(void)
{
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource4);

    exti.EXTI_Line    = EXTI_Line4;
    exti.EXTI_Mode    = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel                   = EXTI4_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);
}

// ── Sensor configuration ────────────────────────────────────────────
static void icm_config(void)
{
    uint8_t whoami;

    do {
        whoami = icm20948_read_byte(UB_0, ICM_WHO_AM_I);
    } while (whoami != 0xEA);

    icm20948_write_byte(UB_0, ICM_PWR_MGMT_1, ICM_PWR_RESET);
    delay_ms(100);
    icm20948_write_byte(UB_0, ICM_PWR_MGMT_1, ICM_CLKSEL_AUTO);
    delay_ms(100);
    icm20948_write_byte(UB_0, ICM_USER_CTRL, ICM_I2C_IF_DIS);

    icm20948_write_byte(UB_2, ICM_GYRO_CONFIG_1, ICM_GYRO_FS_2000);
    icm20948_write_byte(UB_2, ICM_ACCEL_CONFIG, ICM_ACCEL_FS_16G);

    icm20948_write_byte(UB_0, ICM_INT_PIN_CFG, ICM_INT1_ACTL);
    icm20948_write_byte(UB_0, ICM_INT_ENABLE, ICM_RAW_RDY_EN);

    // Ensure bank 0 is active for data reads
    icm_select_bank(UB_0);
}

// ── Calibration ─────────────────────────────────────────────────────
static void icm_calibrate_accel(uint16_t samples)
{
    int32_t sum_x = 0, sum_y = 0, sum_z = 0;
    uint16_t n;

    for (n = 0; n < samples; n++) {
        while (!(icm_read_fast(ICM_INT_STATUS_1) & 0x01));
        uint8_t buf[6];
        icm_read_buf_fast(ICM_ACCEL_XOUT_H, buf, 6);
        sum_x += (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
        sum_y += (int16_t)(((uint16_t)buf[2] << 8) | buf[3]);
        sum_z += (int16_t)(((uint16_t)buf[4] << 8) | buf[5]);
    }

    accel_bias_x = (int16_t)(sum_x / samples);
    accel_bias_y = (int16_t)(sum_y / samples);
    accel_scale_z = 2048;
    accel_bias_z = (int16_t)(sum_z / samples) - accel_scale_z;

    printf("[CALIB] accel raw_avg: X=%ld Y=%ld Z=%ld\r\n",
           sum_x / samples, sum_y / samples, sum_z / samples);
    printf("[CALIB] accel bias: X=%d Y=%d Z=%d\r\n",
           accel_bias_x, accel_bias_y, accel_bias_z);
}

static void icm_calibrate_gyro_fast(uint16_t samples)
{
    int32_t sum_x = 0, sum_y = 0, sum_z = 0;
    uint16_t n;

    for (n = 0; n < samples; n++) {
        while (!(icm_read_fast(ICM_INT_STATUS_1) & 0x01));
        uint8_t buf[6];
        icm_read_buf_fast(ICM_GYRO_XOUT_H, buf, 6);
        sum_x += (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
        sum_y += (int16_t)(((uint16_t)buf[2] << 8) | buf[3]);
        sum_z += (int16_t)(((uint16_t)buf[4] << 8) | buf[5]);
    }

    gyro_bias_x = (int16_t)(sum_x / samples);
    gyro_bias_y = (int16_t)(sum_y / samples);
    gyro_bias_z = (int16_t)(sum_z / samples);
}

// ── Public: init ────────────────────────────────────────────────────
void icm20948_init(void)
{
    icm_gpio_init();
    icm_spi_init();
    icm_exti_init();
    icm_config();

    delay_ms(200);  // let sensor stabilize after config

    icm_calibrate_accel(500);
    icm_calibrate_gyro_fast(500);
}

// ── Public: calibration (for re-calibration) ────────────────────────
void icm20948_calibrate_gyro(uint16_t samples)
{
    icm_calibrate_gyro_fast(samples);
}

// ── Public: data ready ──────────────────────────────────────────────
uint8_t icm20948_data_ready(void)
{
    return data_ready;
}

uint8_t icm20948_data_ready_poll(void)
{
    return icm_read_fast(ICM_INT_STATUS_1) & 0x01;
}

// ── Public: read sensor data ────────────────────────────────────────
void icm20948_read_sensor_data(icm20948_data_t *data)
{
    uint8_t buf[14];

    icm_read_buf_fast(ICM_ACCEL_XOUT_H, buf, 14);

    data->accel_x = (int16_t)(((uint16_t)buf[0]  << 8) | buf[1]);
    data->accel_y = (int16_t)(((uint16_t)buf[2]  << 8) | buf[3]);
    data->accel_z = (int16_t)(((uint16_t)buf[4]  << 8) | buf[5]);
    data->gyro_x  = (int16_t)(((uint16_t)buf[6]  << 8) | buf[7]);
    data->gyro_y  = (int16_t)(((uint16_t)buf[8]  << 8) | buf[9]);
    data->gyro_z  = (int16_t)(((uint16_t)buf[10] << 8) | buf[11]);
    data->temp    = (int16_t)(((uint16_t)buf[12] << 8) | buf[13]);

    data->accel_x -= accel_bias_x;
    data->accel_y -= accel_bias_y;
    data->accel_z -= accel_bias_z;
    data->gyro_x  -= gyro_bias_x;
    data->gyro_y  -= gyro_bias_y;
    data->gyro_z  -= gyro_bias_z;

    data_ready = 0;
}

// ── ISR handler ─────────────────────────────────────────────────────
void icm20948_irq_handler(void)
{
    if (EXTI_GetITStatus(EXTI_Line4) != RESET) {
        data_ready = 1;
        EXTI_ClearITPendingBit(EXTI_Line4);
    }
}
