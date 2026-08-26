#ifndef UN260_APP_SERVICE_APP_CLOCK_H
#define UN260_APP_SERVICE_APP_CLOCK_H

#include <stdint.h>

uint64_t app_clock_monotonic_us(void);
uint64_t app_clock_monotonic_ms(void);
uint32_t app_clock_uptime_ms(void);
uint32_t app_clock_elapsed_us32(uint64_t started_us, uint64_t finished_us);

/* Compatibility entry used by LV_TICK_CUSTOM_SYS_TIME_EXPR. */
uint32_t custom_tick_get(void);

#endif
