#ifndef PERF_STATS_H
#define PERF_STATS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PERF_PROFILE_GE_FILL = 0,
    PERF_PROFILE_GE_BLIT,
    PERF_PROFILE_GE_ROTATE,
    PERF_PROFILE_GE_COUNT
} perf_profile_ge_op_t;

typedef struct {
    uint32_t invalid_area_count;
    uint64_t invalid_pixels;
    uint64_t mirror_pixels;
    uint32_t total_us;
    uint32_t pan_us;
    uint32_t vsync_us;
    uint32_t mirror_us;
    bool full_screen;
} perf_profile_flush_sample_t;

typedef struct {
    uint64_t started_us;
    uint32_t from_id;
    const char *from_name;
    uint32_t to_id;
    const char *to_name;
    const char *route;
    const char *leave_action;
    const char *enter_action;
    uint32_t prepare_us;
    uint32_t notify_us;
    uint32_t leave_us;
    uint32_t enter_us;
    uint32_t commit_us;
    uint32_t total_us;
} perf_profile_page_switch_sample_t;

typedef struct {
    int fps;
    float cpu_percent;
    float mem_mb;
    float lvgl_avg_ms;
    float lvgl_max_ms;
    float loop_avg_ms;
    float loop_max_ms;
    float main_refresh_avg_ms;
    float main_refresh_max_ms;
    bool cpu_valid;
    bool mem_valid;
} perf_stats_snapshot_t;

void perf_stats_init(void);
void perf_stats_report_lvgl_time_us(uint32_t elapsed_us);
void perf_stats_report_loop_time_us(uint32_t elapsed_us);
void perf_stats_report_main_refresh_time_us(uint32_t elapsed_us);
void perf_stats_reset_window(void);
void perf_stats_sample(void);
void perf_stats_get_snapshot(perf_stats_snapshot_t *out);

void perf_profile_set_enabled(bool enabled);
bool perf_profile_is_enabled(void);
uint32_t perf_profile_frame_sequence(void);
void perf_profile_set_page_context(uint32_t page_id, const char *page_name);
void perf_profile_report_active_handler_us(uint32_t elapsed_us);
void perf_profile_report_flush(const perf_profile_flush_sample_t *sample);
void perf_profile_report_ge(perf_profile_ge_op_t op, uint64_t pixels,
                            uint32_t elapsed_us, uint32_t submit_us,
                            uint32_t emit_us, uint32_t sync_us);
void perf_profile_report_page_switch(
    const perf_profile_page_switch_sample_t *sample);
void perf_profile_report_event_us(const char *page_name, const char *event,
                                  uint32_t elapsed_us);
void perf_profile_report_image_decode(const char *src, uint32_t elapsed_us,
                                      uint64_t decoded_bytes);
void perf_profile_watch_invalidation(const void *obj, const char *name);
void perf_profile_unwatch_invalidation(const void *obj);
void perf_profile_poll(uint32_t now_ms);

#endif
