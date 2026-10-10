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
static bool g_frames_selfcheck;
#define STARTUP_INTERVAL_SAMPLES 512U
static uint64_t g_intervals[STARTUP_INTERVAL_SAMPLES];
static unsigned g_interval_count;
static uint64_t g_long_intervals;

static int compare_intervals(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y;
}

bool app_startup_frames_begin(void)
{
    const char *value = getenv("UN260_BOOT_FRAME_TRACE");
    memset(&g_frames, 0, sizeof(g_frames));
    g_frames_selfcheck = false;
    g_interval_count = 0;
    g_long_intervals = 0;
    g_frames_enabled = value && strcmp(value, "1") == 0;
    return g_frames_enabled;
}

bool app_startup_frames_begin_selfcheck(void)
{
    bool enabled = app_startup_frames_begin();
    g_frames_selfcheck = true;
    return enabled;
}

void app_startup_frames_present(uint64_t present_us)
{
    if (!g_frames_enabled) return;
    if (g_frames_selfcheck && g_frames.frames && present_us >= g_frames.last_us) {
        uint64_t interval = present_us - g_frames.last_us;
        if (g_interval_count < STARTUP_INTERVAL_SAMPLES)
            g_intervals[g_interval_count++] = interval;
        if (interval > 50000U) ++g_long_intervals;
    }
    boot_frame_stats_record(&g_frames, present_us, 0);
}

void app_startup_frames_finish(void)
{
    if (!g_frames_enabled) return;
    g_frames_enabled = false;
    /* LVGL work is not sampled here: zero is unspecified, not free rendering.
     * Its raw intervals include intentional still holds, just like native. */
    boot_frame_stats_print(stderr, &g_frames,
        g_frames_selfcheck ? "lvgl-selfcheck" : "lvgl-intro", "detached");
    if (g_frames_selfcheck) {
        qsort(g_intervals, g_interval_count, sizeof(g_intervals[0]), compare_intervals);
        unsigned p95 = g_interval_count ? (g_interval_count * 95U + 99U) / 100U - 1U : 0U;
        fprintf(stderr, "BOOT_FRAME_PERCENTILES renderer=lvgl-selfcheck"
            " samples=%u omitted=%" PRIu64 " p50_us=%" PRIu64 " p95_us=%" PRIu64
            " worst_us=%" PRIu64 " long_intervals_gt50000=%" PRIu64 "\n",
            g_interval_count, g_frames.intervals - g_interval_count,
            g_interval_count ? g_intervals[(g_interval_count - 1U) / 2U] : 0U,
            g_interval_count ? g_intervals[p95] : 0U,
            g_frames.interval_max_us, g_long_intervals);
    }
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
