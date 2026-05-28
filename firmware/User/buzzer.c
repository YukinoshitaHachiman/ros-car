#include "buzzer.h"
#include "tick.h"

#define BUZZER_PORT  GPIOC
#define BUZZER_PIN   GPIO_Pin_5

#define LOW_VOLTAGE_THRESHOLD  7000   // 低于 7.0V 触发报警
#define ALARM_INTERVAL_MS      1000   // 1 秒间隔

static uint32_t last_toggle;
static uint8_t  buzzer_state;  // 0=off, 1=on

void buzzer_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    gpio.GPIO_Pin   = BUZZER_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BUZZER_PORT, &gpio);

    buzzer_off();
    last_toggle  = 0;
    buzzer_state = 0;
}

void buzzer_on(void)
{
    GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
}

void buzzer_off(void)
{
    GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
}

void buzzer_alarm_check(uint16_t battery_mv)
{
#ifdef BUZZER_ENABLE
    if (battery_mv < LOW_VOLTAGE_THRESHOLD && battery_mv > 0) {
        // 低电压：每隔 1 秒翻转一次
        if (tick_elapsed(last_toggle) >= ALARM_INTERVAL_MS) {
            last_toggle = tick_get();
            buzzer_state = !buzzer_state;
            if (buzzer_state)
                buzzer_on();
            else
                buzzer_off();
        }
    } else {
        buzzer_off();
        buzzer_state = 0;
    }
#else
    (void)battery_mv;
    buzzer_off();
#endif
}
