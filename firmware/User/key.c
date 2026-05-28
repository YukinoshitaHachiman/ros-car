#include "key.h"
#include "tick.h"

#define KEY_PORT           GPIOD
#define KEY_PIN            GPIO_Pin_2
#define KEY_IS_PRESSED()   (!GPIO_ReadInputDataBit(KEY_PORT, KEY_PIN))

// ── Timing constants (ms) ─────────────────────────────────────────────
#define DEBOUNCE_MS        30
#define LONG_PRESS_MS      1000
#define DOUBLE_CLICK_MS    300

// ── State machine ─────────────────────────────────────────────────────
typedef enum {
    KS_IDLE = 0,
    KS_DEBOUNCE,
    KS_PRESS,
    KS_RELEASE_WAIT
} key_state_t;

static key_state_t  state = KS_IDLE;
static uint32_t     press_time;     // tick when press confirmed
static uint32_t     release_time;   // tick when released
static key_event_t  pending_evt;    // event to return on next non-NONE

void key_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);

    gpio.GPIO_Pin  = KEY_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(KEY_PORT, &gpio);

    state       = KS_IDLE;
    pending_evt = KEY_EVT_NONE;
}

key_event_t key_scan(void)
{
    key_event_t evt;

    // Return pending event first (from previous scan)
    if (pending_evt != KEY_EVT_NONE) {
        evt          = pending_evt;
        pending_evt  = KEY_EVT_NONE;
        return evt;
    }

    uint8_t pressed = KEY_IS_PRESSED();

    switch (state) {

    case KS_IDLE:
        if (pressed) {
            state       = KS_DEBOUNCE;
            press_time  = tick_get();
        }
        break;

    case KS_DEBOUNCE:
        if (pressed) {
            if (tick_elapsed(press_time) >= DEBOUNCE_MS) {
                state       = KS_PRESS;
                press_time  = tick_get();  // record confirmed press time
            }
        } else {
            state = KS_IDLE;  // glitch, back to idle
        }
        break;

    case KS_PRESS:
        if (!pressed) {
            // Key released
            uint32_t held = tick_elapsed(press_time);
            if (held >= LONG_PRESS_MS) {
                pending_evt = KEY_EVT_LONG;
                state       = KS_IDLE;
                break;
            }
            // Wait for possible double-click
            state         = KS_RELEASE_WAIT;
            release_time  = tick_get();
        } else if (tick_elapsed(press_time) >= LONG_PRESS_MS) {
            // Long press while still held
            pending_evt = KEY_EVT_LONG;
            state       = KS_IDLE;
        }
        break;

    case KS_RELEASE_WAIT:
        if (pressed) {
            // Second press → double click (debounce it briefly)
            if (tick_elapsed(release_time) < DOUBLE_CLICK_MS) {
                pending_evt = KEY_EVT_DOUBLE;
                state       = KS_IDLE;
            } else {
                // Too late for double click, treat as new press
                state       = KS_DEBOUNCE;
                press_time  = tick_get();
            }
        } else if (tick_elapsed(release_time) >= DOUBLE_CLICK_MS) {
            // Timeout → single short press
            pending_evt = KEY_EVT_SHORT;
            state       = KS_IDLE;
        }
        break;
    }

    // Return event immediately if one was just produced
    if (pending_evt != KEY_EVT_NONE) {
        evt          = pending_evt;
        pending_evt  = KEY_EVT_NONE;
        return evt;
    }
    return KEY_EVT_NONE;
}
