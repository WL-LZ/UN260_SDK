#ifndef UN260_BOOT_FRAME_STATS_H
#define UN260_BOOT_FRAME_STATS_H

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

/* Optional, bounded presentation statistics. Owners supply monotonic times
 * and opt in before calling; no clocks, allocation, logging or UI policy here.
 * Raw presentation intervals deliberately include intentional static holds. */
typedef struct {
    uint64_t frames;
    uint64_t intervals;
    uint64_t first_us;
    uint64_t last_us;
    uint64_t interval_total_us;
    uint64_t interval_max_us;
    uint64_t work_total_us;
    uint64_t work_max_us;
    uint64_t interval_bins[6];
    uint64_t clock_resets;
} boot_frame_stats_t;

static inline void boot_frame_stats_record(boot_frame_stats_t *stats,
                                           uint64_t present_us,
                                           uint64_t work_us)
{
    if (stats->frames) {
        if (present_us >= stats->last_us) {
            uint64_t interval = present_us - stats->last_us;
            unsigned bucket = interval <= 16000U ? 0U : interval <= 20000U ? 1U :
                interval <= 33000U ? 2U : interval <= 50000U ? 3U :
                interval <= 100000U ? 4U : 5U;
            ++stats->intervals;
            ++stats->interval_bins[bucket];
            stats->interval_total_us += interval;
            if (interval > stats->interval_max_us) stats->interval_max_us = interval;
        } else {
            ++stats->clock_resets;
        }
    } else {
        stats->first_us = present_us;
    }
    ++stats->frames;
    stats->last_us = present_us;
    stats->work_total_us += work_us;
    if (work_us > stats->work_max_us) stats->work_max_us = work_us;
}

static inline void boot_frame_stats_print(FILE *output,
                                          const boot_frame_stats_t *stats,
                                          const char *renderer,
                                          const char *reason)
{
    fprintf(output,
        "BOOT_FRAMES renderer=%s reason=%s frames=%" PRIu64
        " first_uptime_us=%" PRIu64 " last_uptime_us=%" PRIu64
        " raw_intervals=%" PRIu64 " raw_interval_total_us=%" PRIu64
        " raw_interval_max_us=%" PRIu64 " work_total_us=%" PRIu64
        " work_max_us=%" PRIu64 " raw_bins_us=le16000:%" PRIu64
        ",le20000:%" PRIu64 ",le33000:%" PRIu64 ",le50000:%" PRIu64
        ",le100000:%" PRIu64 ",gt100000:%" PRIu64
        " clock_resets=%" PRIu64 " includes_static_holds=1\n",
        renderer, reason, stats->frames, stats->first_us, stats->last_us,
        stats->intervals, stats->interval_total_us, stats->interval_max_us,
        stats->work_total_us, stats->work_max_us, stats->interval_bins[0],
        stats->interval_bins[1], stats->interval_bins[2], stats->interval_bins[3],
        stats->interval_bins[4], stats->interval_bins[5], stats->clock_resets);
}

#endif
