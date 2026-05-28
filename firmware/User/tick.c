#include "tick.h"

static volatile uint32_t ms_tick = 0;

void tick_init(void)
{
    TIM_TimeBaseInitTypeDef tbase;

    // TIM5 on APB1, available on STM32F103 high-density
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

    // 72MHz / 72 = 1MHz → period 1000 → 1kHz = 1ms
    tbase.TIM_Prescaler         = 72 - 1;
    tbase.TIM_CounterMode       = TIM_CounterMode_Up;
    tbase.TIM_Period            = 1000 - 1;
    tbase.TIM_ClockDivision     = TIM_CKD_DIV1;
    tbase.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM5, &tbase);

    TIM_ClearFlag(TIM5, TIM_FLAG_Update);
    TIM_ITConfig(TIM5, TIM_IT_Update, ENABLE);

    NVIC_InitTypeDef nvic;
    nvic.NVIC_IRQChannel                   = TIM5_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    TIM_Cmd(TIM5, ENABLE);
}

uint32_t tick_get(void)
{
    return ms_tick;
}

uint32_t tick_elapsed(uint32_t since)
{
    return ms_tick - since;
}

void TIM5_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM5, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM5, TIM_IT_Update);
        ms_tick++;
    }
}
