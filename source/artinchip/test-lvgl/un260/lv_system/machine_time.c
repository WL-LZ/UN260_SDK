#include "machine_time.h"
#include <stdio.h>
#include "lvgl/lvgl.h"

static machine_time_value_t g_machine_time = { 2024, 10, 26, 11, 28, 30 };
static bool g_machine_time_paused = false;
static lv_timer_t* g_machine_time_timer = NULL;

static bool is_leap(uint16_t year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (month < 1 || month > 12) return 31;
    if (month == 2) return (uint8_t)(mdays[1] + (is_leap(year) ? 1 : 0));
    return mdays[month - 1];
}

void machine_time_normalize(machine_time_value_t* value)
{
    uint8_t max_day;

    if (!value) return;

    if (value->year < 2000) value->year = 2000;
    if (value->month < 1) value->month = 1;
    if (value->month > 12) value->month = 12;

    max_day = days_in_month(value->year, value->month);
    if (value->day < 1) value->day = 1;
    if (value->day > max_day) value->day = max_day;

    if (value->hour > 23) value->hour = 23;
    if (value->minute > 59) value->minute = 59;
    if (value->second > 59) value->second = 59;
}

bool machine_time_is_valid(const machine_time_value_t* value)
{
    if (!value || value->year < 2000 || value->month < 1 || value->month > 12) return false;
    if (value->day < 1 || value->day > days_in_month(value->year, value->month)) return false;
    return value->hour <= 23 && value->minute <= 59 && value->second <= 59;
}

void machine_time_confirm(const machine_time_value_t* value)
{
    machine_time_value_t confirmed;

    if (!value) return;

    confirmed = *value;
    machine_time_normalize(&confirmed);
    g_machine_time = confirmed;
}

void machine_time_get(machine_time_value_t* value)
{
    if (!value) return;
    *value = g_machine_time;
}

void machine_time_tick(void)
{
    machine_time_value_t value;
    uint8_t max_day;

    if (g_machine_time_paused) return;

    value = g_machine_time;
    machine_time_normalize(&value);

    value.second++;
    if (value.second >= 60) {
        value.second = 0;
        value.minute++;
        if (value.minute >= 60) {
            value.minute = 0;
            value.hour++;
            if (value.hour >= 24) {
                value.hour = 0;
                value.day++;
                max_day = days_in_month(value.year, value.month);
                if (value.day > max_day) {
                    value.day = 1;
                    value.month++;
                    if (value.month > 12) {
                        value.month = 1;
                        value.year++;
                        if (value.year < 2000) value.year = 2000;
                    }
                }
            }
        }
    }

    g_machine_time = value;
}

static void machine_time_timer_cb(lv_timer_t* timer)
{
    (void)timer;
    machine_time_tick();
}

void machine_time_init(void)
{
    if (g_machine_time_timer) return;
    g_machine_time_timer = lv_timer_create(machine_time_timer_cb, 1000, NULL);
}

void machine_time_pause(bool pause)
{
    g_machine_time_paused = pause;
}

void machine_time_format(char* out, uint32_t out_len)
{
    machine_time_value_t value;

    if (!out || out_len == 0) return;

    machine_time_get(&value);
    snprintf(out, out_len, "%04u/%02u/%02u/%02u/%02u/%02u",
             (unsigned)value.year, (unsigned)value.month, (unsigned)value.day,
             (unsigned)value.hour, (unsigned)value.minute, (unsigned)value.second);
}
