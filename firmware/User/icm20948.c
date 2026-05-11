#include "icm20948.h"
#include "delay.h"

// ── CS macro ────────────────────────────────────────────────────────
#define ICM_CS_LOW()   GPIO_ResetBits(GPIOB, GPIO_Pin_12)
#define ICM_CS_HIGH()  GPIO_SetBits(GPIOB, GPIO_Pin_12)

#define ICM_SPI        SPI2

#define ICM_READ       0x80

// ── Private state ───────────────────────────────────────────────────
static volatile uint8_t data_ready = 0;

// ── SPI byte transfer ───────────────────────────────────────────────
static uint8_t spi_transfer(uint8_t tx)
{
    while (SPI_I2S_GetFlagStatus(ICM_SPI, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(ICM_SPI, tx);
    while (SPI_I2S_GetFlagStatus(ICM_SPI, SPI_I2S_FLAG_RXNE) == RESET);
    return (uint8_t)SPI_I2S_ReceiveData(ICM_SPI);
}

// ── Register bank select ────────────────────────────────────────────
static void icm_select_bank(uint8_t bank)
{
    icm20948_write_byte(ICM_BANK_SEL, bank & 0x03);
}

// ── Public: single register read/write ──────────────────────────────

uint8_t icm20948_read_byte(uint8_t reg)
{
    uint8_t val;
    ICM_CS_LOW();
    spi_transfer(reg | ICM_READ);
    val = spi_transfer(0xFF);
    ICM_CS_HIGH();
    return val;
}

void icm20948_write_byte(uint8_t reg, uint8_t data)
{
    ICM_CS_LOW();
    spi_transfer(reg & 0x7F);
    spi_transfer(data);
    ICM_CS_HIGH();
}

// ── Public: multi-byte read ─────────────────────────────────────────
void icm20948_read_buf(uint8_t reg, uint8_t *buf, uint8_t len)
{
    ICM_CS_LOW();
    spi_transfer(reg | ICM_READ);
    while (len--) {
        *buf++ = spi_transfer(0xFF);
    }
    ICM_CS_HIGH();
}

// ── GPIO init ───────────────────────────────────────────────────────
static void icm_gpio_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    // PB12(CS), PB13(SCLK), PB15(MOSI) — AF push-pull
    gpio.GPIO_Pin   = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_15;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    // PB14(MISO) — input floating
    gpio.GPIO_Pin  = GPIO_Pin_14;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio);

    // PA4(INT1) — input pull-up (INT1 active low, idle high)
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
    spi.SPI_CPOL              = SPI_CPOL_High;                // Mode 3
    spi.SPI_CPHA              = SPI_CPHA_2Edge;
    spi.SPI_NSS               = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;     // 36/8=4.5MHz < 7MHz
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
    exti.EXTI_Trigger = EXTI_Trigger_Falling;  // INT1 active low
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
    // Reset then wake up
    icm20948_write_byte(ICM_PWR_MGMT_1, ICM_PWR_RESET);
    delay_ms(100);
    icm20948_write_byte(ICM_PWR_MGMT_1, ICM_CLKSEL_AUTO);
    delay_ms(100);

    // Gyro ±500dps, Accel ±4g (Bank 2)
    icm_select_bank(2);
    icm20948_write_byte(ICM_GYRO_CONFIG_1, ICM_GYRO_FS_500);
    icm20948_write_byte(ICM_ACCEL_CONFIG, ICM_ACCEL_FS_4G);
    icm_select_bank(0);

    // INT1: active low, push-pull, 50us pulse
    icm20948_write_byte(ICM_INT_PIN_CFG, ICM_INT1_ACTL);

    // Enable raw data ready interrupt
    icm20948_write_byte(ICM_INT_ENABLE, ICM_RAW_RDY_EN);
}

// ── Public: init ────────────────────────────────────────────────────
void icm20948_init(void)
{
    icm_gpio_init();
    icm_spi_init();
    icm_exti_init();
    icm_config();
}

// ── Public: data ready flag ─────────────────────────────────────────
uint8_t icm20948_data_ready(void)
{
    return data_ready;
}

// ── Public: read all sensor data ────────────────────────────────────
void icm20948_read_sensor_data(icm20948_data_t *data)
{
    uint8_t buf[14];

    // Burst read: accel(6) + gyro(6) + temp(2)
    icm20948_read_buf(ICM_ACCEL_XOUT_H, buf, 14);

    data->accel_x = ((int16_t)buf[0]  << 8) | buf[1];
    data->accel_y = ((int16_t)buf[2]  << 8) | buf[3];
    data->accel_z = ((int16_t)buf[4]  << 8) | buf[5];
    data->gyro_x  = ((int16_t)buf[6]  << 8) | buf[7];
    data->gyro_y  = ((int16_t)buf[8]  << 8) | buf[9];
    data->gyro_z  = ((int16_t)buf[10] << 8) | buf[11];
    data->temp    = ((int16_t)buf[12] << 8) | buf[13];

    data_ready = 0;
}

// ── Called from EXTI4_IRQHandler ────────────────────────────────────
void icm20948_irq_handler(void)
{
    if (EXTI_GetITStatus(EXTI_Line4) != RESET) {
        data_ready = 1;
        EXTI_ClearITPendingBit(EXTI_Line4);
    }
}
