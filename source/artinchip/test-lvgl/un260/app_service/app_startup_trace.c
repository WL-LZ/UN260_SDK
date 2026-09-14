#include "app_startup_trace.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_drivers/boot_frame_stats.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static boot_frame_stats_t g_frames;
static bool g_frames_enabled;

bool app_startup_frames_begin(void)
{
    const char *value = getenv("UN260_BOOT_FRAME_TRACE");
    memset(&g_frames, 0, sizeof(g_frames));
    g_frames_enabled = value && strcmp(value, "1") == 0;
    return g_frames_enabled;
}

void app_startup_frames_present(uint64_t present_us)
{
    if (g_frames_enabled) boot_frame_stats_record(&g_frames, present_us, 0);
}

void app_startup_frames_finish(void)
{
    if (!g_frames_enabled) return;
    g_frames_enabled = false;
    /* LVGL work is not sampled here: zero is unspecified, not free rendering.
     * Its raw intervals include intentional still holds, just like native. */
    boot_frame_stats_print(stderr, &g_frames, "lvgl-intro", "detached");
}

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
