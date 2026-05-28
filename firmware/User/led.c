#include "led.h"
#include "tick.h"

#define LED_PORT  GPIOC
#define LED_PIN   GPIO_Pin_13

#define LOW_VOLTAGE_THRESHOLD  7000   // 7.0V, 与蜂鸣器一致

// 闪烁间隔 (周期的一半 = 亮/灭各一半)
#define BLINK_NORMAL_MS  500   // 正常电压: 1s 周期
#define BLINK_LOW_MS     250   // 低电压:   0.5s 周期

static uint32_t last_toggle;
static uint8_t  led_state;

void led_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    gpio.GPIO_Pin   = LED_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_PORT, &gpio);

    led_off();
    last_toggle = 0;
    led_state   = 0;
}

void led_on(void)
{
    GPIO_SetBits(LED_PORT, LED_PIN);
    led_state = 1;
}

void led_off(void)
{
    GPIO_ResetBits(LED_PORT, LED_PIN);
    led_state = 0;
}

void led_toggle(void)
{
    if (led_state) led_off(); else led_on();
}

void led_voltage_check(uint16_t battery_mv)
{
    uint16_t interval;

    if (battery_mv < LOW_VOLTAGE_THRESHOLD && battery_mv > 0) {
        interval = BLINK_LOW_MS;   // 低电压: 快速闪烁
    } else {
        interval = BLINK_NORMAL_MS; // 正常: 慢速闪烁
    }

    if (tick_elapsed(last_toggle) >= interval) {
        last_toggle = tick_get();
        led_toggle();
    }
}
