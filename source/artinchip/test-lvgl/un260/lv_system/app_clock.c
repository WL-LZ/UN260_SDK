#include "app_clock.h"

#include <limits.h>
#include <pthread.h>
#include <time.h>

static pthread_once_t g_app_clock_once = PTHREAD_ONCE_INIT;
static uint64_t g_app_clock_started_us;

uint64_t app_clock_monotonic_us(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }
    return (uint64_t)now.tv_sec * 1000000ULL +
           (uint64_t)(now.tv_nsec / 1000ULL);
}

uint64_t app_clock_monotonic_ms(void)
{
    return app_clock_monotonic_us() / 1000ULL;
}

static void app_clock_capture_start(void)
{
    g_app_clock_started_us = app_clock_monotonic_us();
}

uint32_t app_clock_uptime_ms(void)
{
    uint64_t now_us;

    pthread_once(&g_app_clock_once, app_clock_capture_start);
    now_us = app_clock_monotonic_us();
    if (now_us < g_app_clock_started_us) {
        return 0;
    }
    return (uint32_t)((now_us - g_app_clock_started_us) / 1000ULL);
}

uint32_t app_clock_elapsed_us32(uint64_t started_us, uint64_t finished_us)
{
    uint64_t elapsed_us;

    if (finished_us < started_us) {
        return 0;
    }
    elapsed_us = finished_us - started_us;
    if (elapsed_us > UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)elapsed_us;
}

uint32_t custom_tick_get(void)
{
    return app_clock_uptime_ms();
}
