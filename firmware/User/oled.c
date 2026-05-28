#include "oled.h"
#include "delay.h"

// ── Pin mapping ───────────────────────────────────────────────────────
// PB10 = SCL, PB11 = SDA (I2C2 or software I2C)
#define OLED_SCL_H()  GPIO_SetBits(GPIOB, GPIO_Pin_10)
#define OLED_SCL_L()  GPIO_ResetBits(GPIOB, GPIO_Pin_10)
#define OLED_SDA_H()  GPIO_SetBits(GPIOB, GPIO_Pin_11)
#define OLED_SDA_L()  GPIO_ResetBits(GPIOB, GPIO_Pin_11)
#define OLED_SDA_IN()  GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11)

#define OLED_ADDR  0x78   // SSD1306 8-bit write address

// ── Frame buffer ──────────────────────────────────────────────────────
static uint8_t fb[8][128];  // 8 pages × 128 columns

// ── 6x8 font: digits 0-9, 'V', '.', ' ', '-', 'B', 'A', 'T' ──────
static const uint8_t font_6x8[][6] = {
    [0]  = {0x00,0x3E,0x51,0x49,0x45,0x3E}, // 0
    [1]  = {0x00,0x00,0x42,0x7F,0x40,0x00}, // 1
    [2]  = {0x00,0x62,0x51,0x49,0x49,0x46}, // 2
    [3]  = {0x00,0x22,0x41,0x49,0x49,0x36}, // 3
    [4]  = {0x00,0x18,0x14,0x12,0x7F,0x10}, // 4
    [5]  = {0x00,0x27,0x45,0x45,0x45,0x39}, // 5
    [6]  = {0x00,0x3E,0x49,0x49,0x49,0x32}, // 6
    [7]  = {0x00,0x01,0x71,0x09,0x05,0x03}, // 7
    [8]  = {0x00,0x36,0x49,0x49,0x49,0x36}, // 8
    [9]  = {0x00,0x06,0x49,0x49,0x49,0x3E}, // 9
    [10] = {0x00,0x02,0x0C,0x30,0x0C,0x02}, // V
    [11] = {0x00,0x00,0x80,0x60,0x00,0x00}, // .
    [12] = {0x00,0x00,0x00,0x00,0x00,0x00}, // space
    [13] = {0x00,0x08,0x08,0x08,0x08,0x08}, // -
    [14] = {0x00,0x7F,0x49,0x49,0x49,0x36}, // B
    [15] = {0x00,0x7F,0x09,0x09,0x09,0x06}, // A (approximation)
    [16] = {0x00,0x01,0x01,0x7F,0x01,0x01}, // T
};

#define FONT_IDX(c)  ((c) - '0')           // for '0'-'9'
#define FONT_V       10
#define FONT_PERIOD  20

static uint8_t get_font_idx(char c)
{
    if (c >= '0' && c <= '9') return c - '0';  // 0-9
    if (c == 'V') return 10;
    if (c == '.') return 11;
    if (c == ' ') return 12;
    if (c == '-') return 13;
    if (c == 'B') return 14;
    if (c == 'A') return 15;
    if (c == 'T') return 16;
    return 12; // space fallback
}

// ── Software I2C ─────────────────────────────────────────────────────
static void i2c_delay(void) { delay_us(2); }

static void i2c_start(void)
{
    OLED_SDA_H(); OLED_SCL_H(); i2c_delay();
    OLED_SDA_L(); i2c_delay();
    OLED_SCL_L();
}

static void i2c_stop(void)
{
    OLED_SDA_L(); OLED_SCL_H(); i2c_delay();
    OLED_SDA_H(); i2c_delay();
}

static uint8_t i2c_write_byte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80) OLED_SDA_H(); else OLED_SDA_L();
        i2c_delay();
        OLED_SCL_H(); i2c_delay();
        OLED_SCL_L();
        byte <<= 1;
    }
    // Read ACK
    OLED_SDA_H(); i2c_delay();
    OLED_SCL_H(); i2c_delay();
    uint8_t ack = OLED_SDA_IN();
    OLED_SCL_L(); i2c_delay();
    return ack;
}

static void oled_write_cmd(uint8_t cmd)
{
    i2c_start();
    i2c_write_byte(OLED_ADDR);
    i2c_write_byte(0x00);  // Co=0, D/C#=0 → command
    i2c_write_byte(cmd);
    i2c_stop();
}

static void oled_write_data(uint8_t data)
{
    i2c_start();
    i2c_write_byte(OLED_ADDR);
    i2c_write_byte(0x40);  // Co=0, D/C#=1 → data
    i2c_write_byte(data);
    i2c_stop();
}

// ── Init ──────────────────────────────────────────────────────────────
void oled_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Pin  = GPIO_Pin_10 | GPIO_Pin_11;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);
    GPIO_SetBits(GPIOB, GPIO_Pin_10 | GPIO_Pin_11);

    delay_ms(20);  // power-up wait

    // SSD1306 init sequence
    oled_write_cmd(0xAE);  // display off
    oled_write_cmd(0xD5); oled_write_cmd(0x80);  // clock div
    oled_write_cmd(0xA8); oled_write_cmd(0x3F);  // mux ratio (64-1)
    oled_write_cmd(0xD3); oled_write_cmd(0x00);  // display offset
    oled_write_cmd(0x40);  // start line 0
    oled_write_cmd(0x8D); oled_write_cmd(0x14);  // charge pump
    oled_write_cmd(0x20); oled_write_cmd(0x00);  // horizontal addressing
    oled_write_cmd(0xA1);  // segment remap
    oled_write_cmd(0xC8);  // COM scan direction
    oled_write_cmd(0xDA); oled_write_cmd(0x12);  // COM pins
    oled_write_cmd(0x81); oled_write_cmd(0xCF);  // contrast
    oled_write_cmd(0xD9); oled_write_cmd(0xF1);  // precharge
    oled_write_cmd(0xDB); oled_write_cmd(0x40);  // VCOMH
    oled_write_cmd(0xA4);  // display on resume
    oled_write_cmd(0xA6);  // normal display
    oled_write_cmd(0x2E);  // stop scroll
    oled_write_cmd(0xAF);  // display on

    oled_clear();
    oled_refresh();
}

// ── Clear buffer ─────────────────────────────────────────────────────
void oled_clear(void)
{
    for (uint8_t page = 0; page < 8; page++)
        for (uint8_t col = 0; col < 128; col++)
            fb[page][col] = 0x00;
}

// ── Show string at pixel position ────────────────────────────────────
void oled_show_string(uint8_t x, uint8_t y, const char *str)
{
    while (*str) {
        uint8_t idx = get_font_idx(*str);
        uint8_t page = y / 8;

        if (page < 8 && x <= 121) {
            for (uint8_t i = 0; i < 6 && (x + i) < 128; i++) {
                if (y % 8 == 0) {
                    fb[page][x + i] = font_6x8[idx][i];
                }
            }
        }
        x += 6;
        str++;
    }
}

// ── Show number ───────────────────────────────────────────────────────
void oled_show_num(uint8_t x, uint8_t y, uint32_t num, uint8_t digits)
{
    char buf[11];
    uint8_t i = 0;
    if (num == 0) {
        buf[i++] = '0';
    } else {
        while (num > 0 && i < 10) {
            buf[i++] = '0' + (num % 10);
            num /= 10;
        }
    }
    // Pad to requested digits
    while (i < digits && i < 10) buf[i++] = '0';
    // Reverse and display
    for (int8_t j = i - 1; j >= 0; j--) {
        char s[2] = {buf[j], '\0'};
        oled_show_string(x, y, s);
        x += 6;
    }
}

// ── Refresh: flush frame buffer to display ───────────────────────────
void oled_refresh(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        oled_write_cmd(0xB0 + page);  // set page
        oled_write_cmd(0x00);         // column low
        oled_write_cmd(0x10);         // column high

        i2c_start();
        i2c_write_byte(OLED_ADDR);
        i2c_write_byte(0x40);  // data stream
        for (uint8_t col = 0; col < 128; col++) {
            i2c_write_byte(fb[page][col]);
        }
        i2c_stop();
    }
}
