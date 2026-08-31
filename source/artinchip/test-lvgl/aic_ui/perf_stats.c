#include "perf_stats.h"

#include <stddef.h>
#include <string.h>

#include "un260/lv_drivers/uart_io.h"
#include "un260/lv_system/app_clock.h"

#include "cpu_mem.h"
#include "lvgl/lvgl.h"
#include "lv_port_disp.h"

static perf_stats_snapshot_t g_perf_stats = {0};
static struct cpu_occupy g_cpu_prev = {0};
static bool g_cpu_prev_valid = false;

typedef struct {
    uint64_t total_us;
    uint32_t max_us;
    uint32_t count;
    uint16_t histogram[128];
} perf_time_accumulator_t;

#define PERF_TIME_HISTOGRAM_BUCKET_US 4000U
#define PERF_TIME_HISTOGRAM_BUCKETS 128U

static perf_time_accumulator_t g_lvgl_time;
static perf_time_accumulator_t g_loop_time;
static perf_time_accumulator_t g_main_refresh_time;

static void perf_time_report(perf_time_accumulator_t *time,
                             uint32_t elapsed_us)
{
    uint32_t bucket;

    time->total_us += elapsed_us;
    time->count++;
    if (elapsed_us > time->max_us) {
        time->max_us = elapsed_us;
    }

    bucket = elapsed_us / PERF_TIME_HISTOGRAM_BUCKET_US;
    if (bucket >= PERF_TIME_HISTOGRAM_BUCKETS) {
        bucket = PERF_TIME_HISTOGRAM_BUCKETS - 1U;
    }
    if (time->histogram[bucket] < UINT16_MAX) {
        time->histogram[bucket]++;
    }
}

static void perf_time_sample(perf_time_accumulator_t *time,
                             float *avg_ms,
                             float *max_ms)
{
    if (time->count > 0) {
        *avg_ms = (float)time->total_us / (float)time->count / 1000.0f;
        *max_ms = (float)time->max_us / 1000.0f;
    } else {
        *avg_ms = 0.0f;
        *max_ms = 0.0f;
    }

    time->total_us = 0;
    time->max_us = 0;
    time->count = 0;
    memset(time->histogram, 0, sizeof(time->histogram));
}

typedef struct {
    uint64_t total_us;
    uint64_t submit_us;
    uint64_t emit_us;
    uint64_t sync_us;
    uint64_t pixels;
    uint32_t max_us;
    uint32_t sync_max_us;
    uint32_t count;
} perf_profile_op_accumulator_t;

#define PERF_PROFILE_EVENT_CAPACITY 8

typedef struct {
    const char *page_name;
    const char *event;
    uint32_t elapsed_us;
} perf_profile_event_sample_t;

typedef struct {
    bool valid;
    bool first_frame_ready;
    uint32_t page_id;
    const char *page_name;
    const char *mode;
    uint64_t started_us;
    uint32_t switch_us;
    uint32_t first_frame_us;
    uint32_t total_us;
} perf_profile_page_open_sample_t;

typedef struct {
    bool enabled;
    uint32_t window_started_ms;
    uint32_t frame_sequence;
    uint32_t page_id;
    const char *page_name;
    uint32_t page_switches;
    uint32_t frames;
    uint64_t invalid_area_total;
    uint64_t invalid_pixels_total;
    uint32_t full_screen_frames;
    uint64_t mirror_pixels_total;
    perf_time_accumulator_t active_handler;
    perf_time_accumulator_t loop;
    perf_time_accumulator_t flush;
    perf_time_accumulator_t pan;
    perf_time_accumulator_t vsync;
    perf_time_accumulator_t mirror;
    perf_profile_op_accumulator_t ge[PERF_PROFILE_GE_COUNT];
    uint32_t label_calls;
    uint64_t label_text_bytes;
    uint64_t label_visible_pixels;
    uint64_t label_total_us;
    uint32_t label_max_us;
    bool pending_switch_valid;
    perf_profile_page_switch_sample_t pending_switch;
    perf_profile_page_open_sample_t pending_open;
    uint8_t pending_event_count;
    perf_profile_event_sample_t pending_events[PERF_PROFILE_EVENT_CAPACITY];
} perf_profile_state_t;

static perf_profile_state_t g_profile;

#define PERF_PROFILE_INV_WATCH_CAPACITY 12U

typedef struct {
    const lv_obj_t *obj;
    const char *name;
    uint32_t calls;
    uint32_t duplicates;
    uint64_t pixels;
    uint32_t last_frame_sequence;
} perf_profile_inv_watch_t;

static perf_profile_inv_watch_t
    g_inv_watch[PERF_PROFILE_INV_WATCH_CAPACITY];
static uint32_t g_inv_requests;
static uint32_t g_inv_duplicates;
static uint64_t g_inv_pixels;

static uint64_t perf_profile_label_clock_us(void)
{
    return app_clock_monotonic_us();
}

static void perf_profile_report_label_draw(uint32_t elapsed_us,
                                           uint32_t text_bytes,
                                           uint32_t visible_pixels)
{
    if (!g_profile.enabled) {
        return;
    }

    g_profile.label_calls++;
    g_profile.label_text_bytes += text_bytes;
    g_profile.label_visible_pixels += visible_pixels;
    g_profile.label_total_us += elapsed_us;
    if (elapsed_us > g_profile.label_max_us) {
        g_profile.label_max_us = elapsed_us;
    }
}

static void perf_profile_invalidation_cb(const lv_obj_t *obj,
                                         const lv_area_t *area)
{
    uint64_t pixels;
    uint32_t i;

    if (!g_profile.enabled || obj == NULL || area == NULL) {
        return;
    }

    pixels = (uint64_t)lv_area_get_width(area) *
             (uint64_t)lv_area_get_height(area);
    g_inv_requests++;
    g_inv_pixels += pixels;

    for (i = 0; i < PERF_PROFILE_INV_WATCH_CAPACITY; i++) {
        perf_profile_inv_watch_t *watch = &g_inv_watch[i];
        const lv_obj_t *ancestor = obj;

        while (ancestor != NULL && ancestor != watch->obj) {
            ancestor = lv_obj_get_parent(ancestor);
        }
        if (ancestor == NULL) {
            continue;
        }
        watch->calls++;
        watch->pixels += pixels;
        if (watch->last_frame_sequence == g_profile.frame_sequence) {
            watch->duplicates++;
            g_inv_duplicates++;
        }
        watch->last_frame_sequence = g_profile.frame_sequence;
        break;
    }
}

static uint32_t perf_time_average_us(const perf_time_accumulator_t *time)
{
    if (time->count == 0) {
        return 0;
    }
    return (uint32_t)(time->total_us / time->count);
}

static uint32_t perf_time_p95_us(const perf_time_accumulator_t *time)
{
    uint32_t bucket;
    uint32_t accumulated = 0;
    uint32_t target;

    if (time->count == 0) {
        return 0;
    }

    target = (time->count * 95U + 99U) / 100U;
    for (bucket = 0; bucket < PERF_TIME_HISTOGRAM_BUCKETS; bucket++) {
        accumulated += time->histogram[bucket];
        if (accumulated >= target) {
            uint32_t upper_bound = (bucket + 1U) * PERF_TIME_HISTOGRAM_BUCKET_US;
            if (bucket == PERF_TIME_HISTOGRAM_BUCKETS - 1U) {
                return time->max_us;
            }
            return upper_bound < time->max_us ? upper_bound : time->max_us;
        }
    }

    return time->max_us;
}

static void perf_profile_reset_window(uint32_t now_ms)
{
    uint32_t i;

    g_profile.window_started_ms = now_ms;
    g_profile.frames = 0;
    g_profile.page_switches = 0;
    g_profile.invalid_area_total = 0;
    g_profile.invalid_pixels_total = 0;
    g_profile.full_screen_frames = 0;
    g_profile.mirror_pixels_total = 0;
    g_profile.active_handler = (perf_time_accumulator_t){0};
    g_profile.loop = (perf_time_accumulator_t){0};
    g_profile.flush = (perf_time_accumulator_t){0};
    g_profile.pan = (perf_time_accumulator_t){0};
    g_profile.vsync = (perf_time_accumulator_t){0};
    g_profile.mirror = (perf_time_accumulator_t){0};
    memset(g_profile.ge, 0, sizeof(g_profile.ge));
    g_profile.label_calls = 0;
    g_profile.label_text_bytes = 0;
    g_profile.label_visible_pixels = 0;
    g_profile.label_total_us = 0;
    g_profile.label_max_us = 0;
    g_inv_requests = 0;
    g_inv_duplicates = 0;
    g_inv_pixels = 0;
    for (i = 0; i < PERF_PROFILE_INV_WATCH_CAPACITY; i++) {
        g_inv_watch[i].calls = 0;
        g_inv_watch[i].duplicates = 0;
        g_inv_watch[i].pixels = 0;
        g_inv_watch[i].last_frame_sequence = UINT32_MAX;
    }
}

void perf_profile_set_enabled(bool enabled)
{
    if (g_profile.enabled == enabled) {
        return;
    }

    g_profile.enabled = enabled;
    lv_obj_set_invalidation_monitor_cb(enabled ?
        perf_profile_invalidation_cb : NULL);
    lv_draw_label_set_profile_cb(enabled ? perf_profile_label_clock_us : NULL,
                                 enabled ? perf_profile_report_label_draw : NULL);
    if (enabled) {
        g_profile.page_id = UINT32_MAX;
        g_profile.page_name = "INVALID";
    }
    g_profile.pending_switch_valid = false;
    g_profile.pending_open = (perf_profile_page_open_sample_t){0};
    g_profile.pending_event_count = 0;
    perf_profile_reset_window(0);
    uart_debug_printf(
        "PERF_PROFILE state=%s channel=UART inv_source=%s period_ms=1000\n",
        enabled ? "ON" : "OFF", enabled ? "ON" : "OFF");
}

void perf_profile_watch_invalidation(const void *obj, const char *name)
{
    uint32_t i;
    uint32_t free_index = PERF_PROFILE_INV_WATCH_CAPACITY;

    if (obj == NULL || name == NULL) {
        return;
    }

    for (i = 0; i < PERF_PROFILE_INV_WATCH_CAPACITY; i++) {
        if (g_inv_watch[i].obj == obj) {
            g_inv_watch[i].name = name;
            return;
        }
        if (free_index == PERF_PROFILE_INV_WATCH_CAPACITY &&
            g_inv_watch[i].obj == NULL) {
            free_index = i;
        }
    }
    if (free_index == PERF_PROFILE_INV_WATCH_CAPACITY) {
        return;
    }

    g_inv_watch[free_index] = (perf_profile_inv_watch_t){
        .obj = (const lv_obj_t *)obj,
        .name = name,
        .last_frame_sequence = UINT32_MAX,
    };
}

void perf_profile_unwatch_invalidation(const void *obj)
{
    uint32_t i;

    if (obj == NULL) {
        return;
    }
    for (i = 0; i < PERF_PROFILE_INV_WATCH_CAPACITY; i++) {
        if (g_inv_watch[i].obj == obj) {
            g_inv_watch[i] = (perf_profile_inv_watch_t){0};
            return;
        }
    }
}

bool perf_profile_is_enabled(void)
{
    return g_profile.enabled;
}

uint32_t perf_profile_frame_sequence(void)
{
    return g_profile.frame_sequence;
}

void perf_profile_set_page_context(uint32_t page_id, const char *page_name)
{
    if (!g_profile.enabled) {
        return;
    }

    if (g_profile.page_id != UINT32_MAX && g_profile.page_id != page_id) {
        g_profile.page_switches++;
    }
    g_profile.page_id = page_id;
    g_profile.page_name = page_name != NULL ? page_name : "INVALID";
}

void perf_profile_report_active_handler_us(uint32_t elapsed_us)
{
    if (g_profile.enabled) {
        perf_time_report(&g_profile.active_handler, elapsed_us);
    }
}

void perf_profile_report_flush(const perf_profile_flush_sample_t *sample)
{
    if (!g_profile.enabled || sample == NULL) {
        return;
    }

    g_profile.frame_sequence++;
    g_profile.frames++;
    g_profile.invalid_area_total += sample->invalid_area_count;
    g_profile.invalid_pixels_total += sample->invalid_pixels;
    g_profile.mirror_pixels_total += sample->mirror_pixels;
    if (sample->full_screen) {
        g_profile.full_screen_frames++;
    }
    perf_time_report(&g_profile.flush, sample->total_us);
    perf_time_report(&g_profile.pan, sample->pan_us);
    perf_time_report(&g_profile.vsync, sample->vsync_us);
    perf_time_report(&g_profile.mirror, sample->mirror_us);

    if (g_profile.pending_open.valid &&
        !g_profile.pending_open.first_frame_ready) {
        uint32_t total_us = app_clock_elapsed_us32(
            g_profile.pending_open.started_us, app_clock_monotonic_us());

        g_profile.pending_open.total_us = total_us;
        g_profile.pending_open.first_frame_us =
            total_us > g_profile.pending_open.switch_us ?
            total_us - g_profile.pending_open.switch_us : 0;
        g_profile.pending_open.first_frame_ready = true;
    }
}

void perf_profile_report_ge(perf_profile_ge_op_t op, uint64_t pixels,
                            uint32_t elapsed_us, uint32_t submit_us,
                            uint32_t emit_us, uint32_t sync_us)
{
    perf_profile_op_accumulator_t *acc;

    if (!g_profile.enabled || op >= PERF_PROFILE_GE_COUNT) {
        return;
    }

    acc = &g_profile.ge[op];
    acc->count++;
    acc->pixels += pixels;
    acc->total_us += elapsed_us;
    acc->submit_us += submit_us;
    acc->emit_us += emit_us;
    acc->sync_us += sync_us;
    if (elapsed_us > acc->max_us) {
        acc->max_us = elapsed_us;
    }
    if (sync_us > acc->sync_max_us) {
        acc->sync_max_us = sync_us;
    }
}

void perf_profile_report_page_switch(
    const perf_profile_page_switch_sample_t *sample)
{
    if (!g_profile.enabled || sample == NULL) {
        return;
    }

    g_profile.pending_switch = *sample;
    g_profile.pending_switch_valid = true;
    g_profile.pending_open = (perf_profile_page_open_sample_t){
        .valid = sample->started_us != 0,
        .page_id = sample->to_id,
        .page_name = sample->to_name,
        .mode = sample->enter_action,
        .started_us = sample->started_us,
        .switch_us = sample->total_us,
    };
}

void perf_profile_report_event_us(const char *page_name, const char *event,
                                  uint32_t elapsed_us)
{
    perf_profile_event_sample_t *sample;

    if (!g_profile.enabled || event == NULL ||
        g_profile.pending_event_count >= PERF_PROFILE_EVENT_CAPACITY) {
        return;
    }

    sample = &g_profile.pending_events[g_profile.pending_event_count++];
    sample->page_name = page_name != NULL ? page_name : "INVALID";
    sample->event = event;
    sample->elapsed_us = elapsed_us;
}

static void perf_profile_emit_pending_events(void)
{
    uint8_t i;

    if (g_profile.pending_switch_valid) {
        const perf_profile_page_switch_sample_t *sample =
            &g_profile.pending_switch;

        uart_debug_printf(
            "PERF_SWITCH route=%s from=%s(%u) to=%s(%u) "
            "prep=%u notify=%u leave=%s:%u enter=%s:%u commit=%u total=%u\n",
            sample->route != NULL ? sample->route : "UNKNOWN",
            sample->from_name != NULL ? sample->from_name : "INVALID",
            sample->from_id,
            sample->to_name != NULL ? sample->to_name : "INVALID",
            sample->to_id,
            sample->prepare_us, sample->notify_us,
            sample->leave_action != NULL ? sample->leave_action : "NONE",
            sample->leave_us,
            sample->enter_action != NULL ? sample->enter_action : "NONE",
            sample->enter_us, sample->commit_us, sample->total_us);
        g_profile.pending_switch_valid = false;
    }

    if (g_profile.pending_open.valid &&
        g_profile.pending_open.first_frame_ready) {
        const perf_profile_page_open_sample_t *sample =
            &g_profile.pending_open;
        const char *temperature =
            sample->mode != NULL && strcmp(sample->mode, "CREATE") == 0 ?
            "COLD" : "WARM";

        uart_debug_printf(
            "PERF_OPEN page=%s(%u) type=%s mode=%s "
            "switch_us=%u first_frame_us=%u total_us=%u total_ms=%u.%03u\n",
            sample->page_name != NULL ? sample->page_name : "INVALID",
            sample->page_id, temperature,
            sample->mode != NULL ? sample->mode : "UNKNOWN",
            sample->switch_us, sample->first_frame_us, sample->total_us,
            sample->total_us / 1000U, sample->total_us % 1000U);
        g_profile.pending_open = (perf_profile_page_open_sample_t){0};
    }

    for (i = 0; i < g_profile.pending_event_count; i++) {
        const perf_profile_event_sample_t *sample =
            &g_profile.pending_events[i];

        uart_debug_printf("PERF_EVENT page=%s event=%s us=%u\n",
                          sample->page_name, sample->event,
                          sample->elapsed_us);
    }
    g_profile.pending_event_count = 0;
}

void perf_profile_poll(uint32_t now_ms)
{
    uint32_t elapsed_ms;
    uint32_t fps;
    uint32_t inv_avg;
    uint64_t pixels_avg;
    const char *activity;
    uint32_t ge_commands;
    uint64_t ge_submit_us;
    uint64_t ge_emit_us;
    uint64_t ge_sync_us;
    uint32_t ge_sync_max_us;

    if (!g_profile.enabled) {
        return;
    }
    perf_profile_emit_pending_events();
    if (g_profile.window_started_ms == 0) {
        perf_profile_reset_window(now_ms);
        return;
    }

    elapsed_ms = now_ms - g_profile.window_started_ms;
    if (elapsed_ms < 1000) {
        return;
    }

    fps = elapsed_ms > 0 ?
        (uint32_t)(((uint64_t)g_profile.frames * 1000ULL) / elapsed_ms) : 0;
    inv_avg = g_profile.frames > 0 ?
        (uint32_t)(g_profile.invalid_area_total / g_profile.frames) : 0;
    pixels_avg = g_profile.frames > 0 ?
        g_profile.invalid_pixels_total / g_profile.frames : 0;
    activity = g_profile.page_switches > 0 ? "SWITCH" :
               (g_profile.frames > 0 ? "RENDER" : "IDLE");
    ge_commands = g_profile.ge[PERF_PROFILE_GE_FILL].count +
                  g_profile.ge[PERF_PROFILE_GE_BLIT].count +
                  g_profile.ge[PERF_PROFILE_GE_ROTATE].count;
    ge_submit_us = g_profile.ge[PERF_PROFILE_GE_FILL].submit_us +
                   g_profile.ge[PERF_PROFILE_GE_BLIT].submit_us +
                   g_profile.ge[PERF_PROFILE_GE_ROTATE].submit_us;
    ge_emit_us = g_profile.ge[PERF_PROFILE_GE_FILL].emit_us +
                 g_profile.ge[PERF_PROFILE_GE_BLIT].emit_us +
                 g_profile.ge[PERF_PROFILE_GE_ROTATE].emit_us;
    ge_sync_us = g_profile.ge[PERF_PROFILE_GE_FILL].sync_us +
                 g_profile.ge[PERF_PROFILE_GE_BLIT].sync_us +
                 g_profile.ge[PERF_PROFILE_GE_ROTATE].sync_us;
    ge_sync_max_us = g_profile.ge[PERF_PROFILE_GE_FILL].sync_max_us;
    if (g_profile.ge[PERF_PROFILE_GE_BLIT].sync_max_us > ge_sync_max_us) {
        ge_sync_max_us = g_profile.ge[PERF_PROFILE_GE_BLIT].sync_max_us;
    }
    if (g_profile.ge[PERF_PROFILE_GE_ROTATE].sync_max_us > ge_sync_max_us) {
        ge_sync_max_us = g_profile.ge[PERF_PROFILE_GE_ROTATE].sync_max_us;
    }

    uart_debug_printf(
        "PERF page=%s(%u) act=%s fps=%u n=%u inv=%u/%llu/%u h=%u/%u/%u loop=%u/%u/%u "
        "out=%u/%u/%u/%u/%u/%u ge=%u:%llu,%u:%llu,%u:%llu "
        "gepipe=%u/%llu/%llu/%llu/%u\n",
        g_profile.page_name, g_profile.page_id, activity,
        fps, g_profile.frames, inv_avg, (unsigned long long)pixels_avg,
        g_profile.full_screen_frames,
        perf_time_average_us(&g_profile.active_handler),
        perf_time_p95_us(&g_profile.active_handler),
        g_profile.active_handler.max_us,
        perf_time_average_us(&g_profile.loop),
        perf_time_p95_us(&g_profile.loop), g_profile.loop.max_us,
        perf_time_average_us(&g_profile.flush),
        perf_time_p95_us(&g_profile.flush), g_profile.flush.max_us,
        perf_time_average_us(&g_profile.pan),
        perf_time_average_us(&g_profile.vsync),
        perf_time_average_us(&g_profile.mirror),
        g_profile.ge[PERF_PROFILE_GE_FILL].count,
        (unsigned long long)g_profile.ge[PERF_PROFILE_GE_FILL].total_us,
        g_profile.ge[PERF_PROFILE_GE_BLIT].count,
        (unsigned long long)g_profile.ge[PERF_PROFILE_GE_BLIT].total_us,
        g_profile.ge[PERF_PROFILE_GE_ROTATE].count,
        (unsigned long long)g_profile.ge[PERF_PROFILE_GE_ROTATE].total_us,
        ge_commands, (unsigned long long)ge_submit_us,
        (unsigned long long)ge_emit_us, (unsigned long long)ge_sync_us,
        ge_sync_max_us);

    uart_debug_printf(
        "PERF_INV page=%s(%u) req=%u px=%llu watched_dup=%u\n",
        g_profile.page_name, g_profile.page_id,
        g_inv_requests, (unsigned long long)g_inv_pixels,
        g_inv_duplicates);
    uart_debug_printf(
        "PERF_DRAW page=%s(%u) label=%u/%llu/%llu/%llu/%u avg_us=%llu\n",
        g_profile.page_name, g_profile.page_id,
        g_profile.label_calls,
        (unsigned long long)g_profile.label_text_bytes,
        (unsigned long long)g_profile.label_visible_pixels,
        (unsigned long long)g_profile.label_total_us,
        g_profile.label_max_us,
        (unsigned long long)(g_profile.label_calls > 0 ?
            g_profile.label_total_us / g_profile.label_calls : 0));
    for (uint32_t i = 0; i < PERF_PROFILE_INV_WATCH_CAPACITY; i++) {
        const perf_profile_inv_watch_t *watch = &g_inv_watch[i];

        if (watch->obj != NULL && watch->calls > 0) {
            uart_debug_printf(
                "PERF_INV_OBJ page=%s name=%s calls=%u px=%llu dup=%u\n",
                g_profile.page_name,
                watch->name != NULL ? watch->name : "UNKNOWN",
                watch->calls, (unsigned long long)watch->pixels,
                watch->duplicates);
        }
    }

    perf_profile_reset_window(now_ms);
}


void perf_stats_init(void)
{
    g_perf_stats.fps = 0;
    g_perf_stats.cpu_percent = 0.0f;
    g_perf_stats.mem_mb = 0.0f;
    g_perf_stats.lvgl_avg_ms = 0.0f;
    g_perf_stats.lvgl_max_ms = 0.0f;
    g_perf_stats.loop_avg_ms = 0.0f;
    g_perf_stats.loop_max_ms = 0.0f;
    g_perf_stats.main_refresh_avg_ms = 0.0f;
    g_perf_stats.main_refresh_max_ms = 0.0f;
    g_perf_stats.cpu_valid = false;
    g_perf_stats.mem_valid = false;
    g_lvgl_time = (perf_time_accumulator_t){0};
    g_loop_time = (perf_time_accumulator_t){0};
    g_main_refresh_time = (perf_time_accumulator_t){0};
    memset(&g_profile, 0, sizeof(g_profile));
    memset(g_inv_watch, 0, sizeof(g_inv_watch));
    lv_obj_set_invalidation_monitor_cb(NULL);
    lv_draw_label_set_profile_cb(NULL, NULL);
    g_cpu_prev_valid = (cpu_occupy_get(&g_cpu_prev) == 0);
}

void perf_stats_report_lvgl_time_us(uint32_t elapsed_us)
{
    perf_time_report(&g_lvgl_time, elapsed_us);
}

void perf_stats_report_loop_time_us(uint32_t elapsed_us)
{
    perf_time_report(&g_loop_time, elapsed_us);
    if (g_profile.enabled) {
        perf_time_report(&g_profile.loop, elapsed_us);
    }
}

void perf_stats_report_main_refresh_time_us(uint32_t elapsed_us)
{
    perf_time_report(&g_main_refresh_time, elapsed_us);
}

void perf_stats_reset_window(void)
{
    g_lvgl_time = (perf_time_accumulator_t){0};
    g_loop_time = (perf_time_accumulator_t){0};
    g_main_refresh_time = (perf_time_accumulator_t){0};
    g_cpu_prev_valid = (cpu_occupy_get(&g_cpu_prev) == 0);
}

void perf_stats_sample(void)
{
    struct cpu_occupy cpu_now;
    struct memory_occupy mem_now;

    g_perf_stats.fps = fbdev_draw_fps();

    if (cpu_occupy_get(&cpu_now) == 0) {
        if (g_cpu_prev_valid) {
            float cpu = cpu_occupy_cal(&g_cpu_prev, &cpu_now);
            if (cpu < 0.0f) cpu = 0.0f;
            if (cpu > 100.0f) cpu = 100.0f;
            g_perf_stats.cpu_percent = cpu;
            g_perf_stats.cpu_valid = true;
        } else {
            g_perf_stats.cpu_valid = false;
        }

        g_cpu_prev = cpu_now;
        g_cpu_prev_valid = true;
    } else {
        g_perf_stats.cpu_valid = false;
    }

    if (mem_occupy_get(&mem_now) == 0) {
        g_perf_stats.mem_mb = mem_occupy_cal_size(&mem_now);
        g_perf_stats.mem_valid = true;
    } else {
        g_perf_stats.mem_valid = false;
    }

    perf_time_sample(&g_lvgl_time,
                     &g_perf_stats.lvgl_avg_ms,
                     &g_perf_stats.lvgl_max_ms);
    perf_time_sample(&g_loop_time,
                     &g_perf_stats.loop_avg_ms,
                     &g_perf_stats.loop_max_ms);
    if (g_main_refresh_time.count > 0) {
        perf_time_sample(&g_main_refresh_time,
                         &g_perf_stats.main_refresh_avg_ms,
                         &g_perf_stats.main_refresh_max_ms);
    }
}

void perf_stats_get_snapshot(perf_stats_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }

    *out = g_perf_stats;
}
