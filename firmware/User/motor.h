#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

// ── Motor identity ────────────────────────────────────────────────────
typedef enum {
    MOTOR_1 = 0,
    MOTOR_2 = 1,
    // MOTOR_3, MOTOR_4 reserved — use different timers when needed
    MOTOR_COUNT
} motor_id_t;

typedef enum {
    MOTOR_STOP     = 0,
    MOTOR_FORWARD  = 1,
    MOTOR_BACKWARD = 2
} motor_dir_t;

// ── PWM parameters (TIM3, 1kHz, 1000-step resolution) ────────────────
#define MOTOR_PWM_PERIOD     1000
#define MOTOR_DUTY_MAX       750   // 75%, 板子12V → 电机9V (最高安全值)
#define MOTOR_DUTY_RECOMMEND 500   // 50%, 板子12V → 电机6V (推荐值)

// ── Encoder ────────────────────────────────────────────────────────────
#define MOTOR_ENCODER_CPR    2496  // 电机输出轴每圈脉冲数 (13线 × 48减速比 × 4)

// ── Public API ────────────────────────────────────────────────────────
void    motor_init(void);
void    motor_run(motor_id_t motor, motor_dir_t dir, uint16_t duty);
void    motor_stop(motor_id_t motor);
int32_t motor_get_encoder(motor_id_t motor);
void    motor_reset_encoder(motor_id_t motor);
void    motor_test(void);

#endif
