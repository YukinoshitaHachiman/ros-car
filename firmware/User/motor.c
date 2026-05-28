#include "motor.h"
#include "delay.h"
#include <stdio.h>

// ── Hardware mapping ──────────────────────────────────────────────────
// TIM3 PWM (full remap): PC6/PC7/PC8/PC9 = CH1/CH2/CH3/CH4
// Motor 1: CH1(PC6)=IN1, CH2(PC7)=IN2
// Motor 2: CH3(PC8)=IN1, CH4(PC9)=IN2
// TIM2 encoder: PA15(H1A, CH1), PB3(H1B, CH2) → Motor 1
// TIM4 encoder: PB6(H2B, CH1), PB7(H2A, CH2) → Motor 2

#define PWM_TIM           TIM3
#define ENC1_TIM          TIM2
#define ENC2_TIM          TIM4

// Channel index per motor: motor*2+1 = IN1, motor*2+2 = IN2
#define MOTOR_CH_A(m)     ((m) * 2 + 1)
#define MOTOR_CH_B(m)     ((m) * 2 + 2)

// ── PWM duty storage ──────────────────────────────────────────────────
static motor_dir_t motor_dir[MOTOR_COUNT];
static uint16_t   motor_duty[MOTOR_COUNT];

// ── Set single TIM3 channel compare value ─────────────────────────────
static void pwm_set_ch(uint8_t ch, uint16_t val)
{
    switch (ch) {
    case 1: TIM3->CCR1 = val; break;
    case 2: TIM3->CCR2 = val; break;
    case 3: TIM3->CCR3 = val; break;
    case 4: TIM3->CCR4 = val; break;
    }
}

// ── Apply direction + duty to a motor's two channels ──────────────────
static void motor_apply(motor_id_t motor)
{
    uint8_t ch_a = MOTOR_CH_A(motor);
    uint8_t ch_b = MOTOR_CH_B(motor);
    uint16_t d  = motor_duty[motor];

    switch (motor_dir[motor]) {
    case MOTOR_FORWARD:
        pwm_set_ch(ch_a, d);
        pwm_set_ch(ch_b, 0);
        break;
    case MOTOR_BACKWARD:
        pwm_set_ch(ch_a, 0);
        pwm_set_ch(ch_b, d);
        break;
    default: // MOTOR_STOP
        pwm_set_ch(ch_a, 0);
        pwm_set_ch(ch_b, 0);
        break;
    }
}

// ── motor_init ────────────────────────────────────────────────────────
void motor_init(void)
{
    GPIO_InitTypeDef  gpio;
    TIM_TimeBaseInitTypeDef   tbase;
    TIM_OCInitTypeDef oc;

    // ── Clocks ─────────────────────────────────────────────────────────
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3 |
                           RCC_APB1Periph_TIM4, ENABLE);

    // Free JTAG pins PA15/PB3 for encoder use, keep SWD
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    // ── PWM pins: PC6~PC9 as AF_PP (TIM3 full remap) ──────────────────
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, ENABLE);

    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9;
    GPIO_Init(GPIOC, &gpio);

    // ── Encoder pins: input floating ───────────────────────────────────
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    // Motor 1 encoder: PA15 + PB3
    gpio.GPIO_Pin  = GPIO_Pin_15;
    GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin  = GPIO_Pin_3;
    GPIO_Init(GPIOB, &gpio);
    // Motor 2 encoder: PB6 + PB7
    gpio.GPIO_Pin  = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_Init(GPIOB, &gpio);

    // ── TIM3 PWM config ────────────────────────────────────────────────
    tbase.TIM_Prescaler         = 72 - 1;   // 72MHz / 72 = 1MHz
    tbase.TIM_CounterMode       = TIM_CounterMode_Up;
    tbase.TIM_Period            = MOTOR_PWM_PERIOD - 1;  // 1kHz
    tbase.TIM_ClockDivision     = TIM_CKD_DIV1;
    tbase.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(PWM_TIM, &tbase);

    oc.TIM_OCMode     = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_OCPolarity  = TIM_OCPolarity_High;
    oc.TIM_Pulse       = 0;  // all motors off initially

    oc.TIM_OCIdleState = TIM_OCIdleState_Reset;  // safe state when paused
    TIM_OC1Init(PWM_TIM, &oc);
    TIM_OC2Init(PWM_TIM, &oc);
    TIM_OC3Init(PWM_TIM, &oc);
    TIM_OC4Init(PWM_TIM, &oc);

    // Enable preload on all channels so duty updates take effect at
    // the next update event, avoiding glitches mid-cycle.
    TIM_OC1PreloadConfig(PWM_TIM, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(PWM_TIM, TIM_OCPreload_Enable);
    TIM_OC3PreloadConfig(PWM_TIM, TIM_OCPreload_Enable);
    TIM_OC4PreloadConfig(PWM_TIM, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(PWM_TIM, ENABLE);
    TIM_Cmd(PWM_TIM, ENABLE);

    // ── TIM2 encoder config (Motor 1) ──────────────────────────────────
    TIM_EncoderInterfaceConfig(ENC1_TIM,
        TIM_EncoderMode_TI12,           // 4x quadrature
        TIM_ICPolarity_Rising,
        TIM_ICPolarity_Rising);
    tbase.TIM_Period = 0xFFFF;  // max 16-bit
    tbase.TIM_Prescaler = 0;
    TIM_TimeBaseInit(ENC1_TIM, &tbase);
    TIM_SetCounter(ENC1_TIM, 0);
    TIM_Cmd(ENC1_TIM, ENABLE);

    // ── TIM4 encoder config (Motor 2) ──────────────────────────────────
    // TIM4 defaults: CH1=PB6, CH2=PB7; no remap needed
    TIM_EncoderInterfaceConfig(ENC2_TIM,
        TIM_EncoderMode_TI12,
        TIM_ICPolarity_Rising,
        TIM_ICPolarity_Rising);
    tbase.TIM_Period = 0xFFFF;
    tbase.TIM_Prescaler = 0;
    TIM_TimeBaseInit(ENC2_TIM, &tbase);
    TIM_SetCounter(ENC2_TIM, 0);
    TIM_Cmd(ENC2_TIM, ENABLE);

    // ── Init state ─────────────────────────────────────────────────────
    motor_dir[0]  = MOTOR_STOP;
    motor_dir[1]  = MOTOR_STOP;
    motor_duty[0] = 0;
    motor_duty[1] = 0;
}

// ── motor_run ─────────────────────────────────────────────────────────
void motor_run(motor_id_t motor, motor_dir_t dir, uint16_t duty)
{
    if (motor >= MOTOR_COUNT) return;
    if (duty > MOTOR_DUTY_MAX) duty = MOTOR_DUTY_MAX;

    motor_dir[motor]  = dir;
    motor_duty[motor] = duty;
    motor_apply(motor);
}

// ── motor_stop ────────────────────────────────────────────────────────
void motor_stop(motor_id_t motor)
{
    motor_run(motor, MOTOR_STOP, 0);
}

// ── motor_get_encoder ─────────────────────────────────────────────────
int32_t motor_get_encoder(motor_id_t motor)
{
    uint16_t raw = 0;
    switch (motor) {
    case MOTOR_1: raw = TIM_GetCounter(ENC1_TIM); break;
    case MOTOR_2: raw = TIM_GetCounter(ENC2_TIM); break;
    default: return 0;
    }
    return (int32_t)(int16_t)raw;  // signed 16-bit → signed 32-bit
}

// ── motor_reset_encoder ───────────────────────────────────────────────
void motor_reset_encoder(motor_id_t motor)
{
    switch (motor) {
    case MOTOR_1: TIM_SetCounter(ENC1_TIM, 0); break;
    case MOTOR_2: TIM_SetCounter(ENC2_TIM, 0); break;
    default: break;
    }
}

// ── motor_test ────────────────────────────────────────────────────────
void motor_test(void)
{
    int32_t enc;

    printf("\r\n========== Motor Driver Test ==========\r\n");

    // ── M1 forward 30% ─────────────────────────────────────────────────
    printf("M1 forward 30%% duty...\r\n");
    motor_reset_encoder(MOTOR_1);
    motor_run(MOTOR_1, MOTOR_FORWARD, 300);
    delay_ms(2000);
    enc = motor_get_encoder(MOTOR_1);
    printf("  M1 encoder after 2s: %d\r\n", (int)enc);
    motor_stop(MOTOR_1);
    delay_ms(500);

    // ── M1 reverse 30% ─────────────────────────────────────────────────
    printf("M1 reverse 30%% duty...\r\n");
    motor_reset_encoder(MOTOR_1);
    motor_run(MOTOR_1, MOTOR_BACKWARD, 300);
    delay_ms(2000);
    enc = motor_get_encoder(MOTOR_1);
    printf("  M1 encoder after 2s: %d\r\n", (int)enc);
    motor_stop(MOTOR_1);
    delay_ms(500);

    // ── M2 forward 30% ─────────────────────────────────────────────────
    printf("M2 forward 30%% duty...\r\n");
    motor_reset_encoder(MOTOR_2);
    motor_run(MOTOR_2, MOTOR_FORWARD, 300);
    delay_ms(2000);
    enc = motor_get_encoder(MOTOR_2);
    printf("  M2 encoder after 2s: %d\r\n", (int)enc);
    motor_stop(MOTOR_2);
    delay_ms(500);

    // ── M2 reverse 30% ─────────────────────────────────────────────────
    printf("M2 reverse 30%% duty...\r\n");
    motor_reset_encoder(MOTOR_2);
    motor_run(MOTOR_2, MOTOR_BACKWARD, 300);
    delay_ms(2000);
    enc = motor_get_encoder(MOTOR_2);
    printf("  M2 encoder after 2s: %d\r\n", (int)enc);
    motor_stop(MOTOR_2);

    printf("Motor test done.\r\n");
    printf("=========================================\r\n");
}
