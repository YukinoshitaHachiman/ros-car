#include "sysmon.h"
#include "tick.h"
#include "adc_battery.h"
#include "key.h"
#include "oled.h"
#include "buzzer.h"
#include "led.h"
#include <stdio.h>

static uint16_t  bat_mv;
static uint32_t  last_key, last_adc, last_oled, last_buzzer, last_led;

void sysmon_init(void)
{
    tick_init();
    adc_battery_init();
    key_init();
    oled_init();
    buzzer_init();
    led_init();

    bat_mv      = 0;
    last_key    = tick_get();
    last_adc    = last_key;
    last_oled   = last_key;
    last_buzzer = last_key;
    last_led    = last_key;
}

void sysmon_update(void)
{
    uint32_t now = tick_get();

    // ── Key scan (10ms) ────────────────────────────────────────────────
    if (tick_elapsed(last_key) >= 10) {
        last_key = now;
        key_event_t evt = key_scan();
        if (evt == KEY_EVT_SHORT) {
            printf("[KEY] short press\r\n");
        } else if (evt == KEY_EVT_LONG) {
            printf("[KEY] long press\r\n");
        } else if (evt == KEY_EVT_DOUBLE) {
            printf("[KEY] double click\r\n");
        }
    }

    // ── ADC battery (100ms) ────────────────────────────────────────────
    if (tick_elapsed(last_adc) >= 100) {
        last_adc = now;
        bat_mv = adc_battery_read_mv();
    }

    // ── OLED display (200ms) ───────────────────────────────────────────
    if (tick_elapsed(last_oled) >= 200) {
        last_oled = now;
        oled_clear();
        oled_show_string(0, 0, "BAT:");
        if (bat_mv > 0) {
            oled_show_num(30, 0, bat_mv / 1000, 1);
            oled_show_string(36, 0, ".");
            oled_show_num(42, 0, (bat_mv % 1000) / 100, 1);
            oled_show_string(48, 0, "V");
        } else {
            oled_show_string(30, 0, "----");
        }
        oled_refresh();
    }

    // ── Buzzer alarm (100ms) ───────────────────────────────────────────
    if (tick_elapsed(last_buzzer) >= 100) {
        last_buzzer = now;
        buzzer_alarm_check(bat_mv);
    }

    // ── LED blink (50ms) ───────────────────────────────────────────────
    if (tick_elapsed(last_led) >= 50) {
        last_led = now;
        led_voltage_check(bat_mv);
    }
}
