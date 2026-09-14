#include "app_startup_trace.h"
#include "un260/lv_system/app_clock.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

void app_startup_trace_mark(const char *stage)
{
    static bool initialized;
    static bool enabled;
    static uint64_t first_ms;
    if (!initialized) {
        const char *value = getenv("UN260_BOOT_TRACE");
        enabled = value && strcmp(value, "1") == 0;
        initialized = true;
        if (enabled) first_ms = app_clock_monotonic_ms();
    }
    if (!enabled) return;
    uint64_t now = app_clock_monotonic_ms();
    fprintf(stderr, "BOOT_TRACE app uptime_ms=%" PRIu64 " elapsed_ms=%" PRIu64 " stage=%s\n",
            now, now - first_ms, stage);
}
