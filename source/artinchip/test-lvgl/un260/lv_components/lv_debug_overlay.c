#include "un260/lv_components/lv_debug_overlay.h"

#include "lvgl/lvgl.h"
#include "aic_ui/perf_stats.h"

static lv_obj_t *g_debug_overlay = NULL;
static lv_obj_t *g_debug_fps_label = NULL;
static lv_obj_t *g_debug_cpu_label = NULL;
static lv_obj_t *g_debug_mem_label = NULL;
static lv_obj_t *g_debug_lvgl_label = NULL;
static lv_obj_t *g_debug_loop_label = NULL;
static lv_obj_t *g_debug_main_label = NULL;
static lv_timer_t *g_debug_timer = NULL;
static bool g_debug_overlay_enabled = false;

static void debug_overlay_refresh_cb(lv_timer_t *timer)
{
    perf_stats_snapshot_t stats;
    LV_UNUSED(timer);

    if (!g_debug_overlay || !lv_obj_is_valid(g_debug_overlay)) {
        return;
    }

    perf_stats_sample();
    perf_stats_get_snapshot(&stats);

    lv_label_set_text_fmt(g_debug_fps_label, "FPS: %d", stats.fps);

    if (stats.cpu_valid) {
        lv_label_set_text_fmt(g_debug_cpu_label, "CPU: %.0f%%", stats.cpu_percent);
    } else {
        lv_label_set_text(g_debug_cpu_label, "CPU: N/A");
    }

    if (stats.mem_valid) {
        lv_label_set_text_fmt(g_debug_mem_label, "MEM: %.1fMB", stats.mem_mb);
    } else {
        lv_label_set_text(g_debug_mem_label, "MEM: N/A");
    }

    lv_label_set_text_fmt(g_debug_lvgl_label, "LVGL: %.2f/%.2fms",
                          stats.lvgl_avg_ms, stats.lvgl_max_ms);
    lv_label_set_text_fmt(g_debug_loop_label, "LOOP: %.2f/%.2fms",
                          stats.loop_avg_ms, stats.loop_max_ms);
    lv_label_set_text_fmt(g_debug_main_label, "MAIN: %.2f/%.2fms",
                          stats.main_refresh_avg_ms,
                          stats.main_refresh_max_ms);
}

void lv_debug_overlay_init(void)
{
    if (g_debug_overlay && lv_obj_is_valid(g_debug_overlay)) {
        return;
    }

    g_debug_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_debug_overlay);
    lv_obj_set_size(g_debug_overlay, 176, 118);
    lv_obj_align(g_debug_overlay, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_set_style_bg_color(g_debug_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_debug_overlay, LV_OPA_40, 0);
    lv_obj_set_style_radius(g_debug_overlay, 8, 0);
    lv_obj_set_style_pad_all(g_debug_overlay, 6, 0);
    lv_obj_set_style_pad_row(g_debug_overlay, 2, 0);
    lv_obj_set_style_border_width(g_debug_overlay, 0, 0);
    lv_obj_set_layout(g_debug_overlay, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_debug_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_debug_overlay, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(g_debug_overlay, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    g_debug_fps_label = lv_label_create(g_debug_overlay);
    g_debug_cpu_label = lv_label_create(g_debug_overlay);
    g_debug_mem_label = lv_label_create(g_debug_overlay);
    g_debug_lvgl_label = lv_label_create(g_debug_overlay);
    g_debug_loop_label = lv_label_create(g_debug_overlay);
    g_debug_main_label = lv_label_create(g_debug_overlay);

    lv_obj_set_style_text_color(g_debug_fps_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(g_debug_cpu_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(g_debug_mem_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(g_debug_lvgl_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(g_debug_loop_label, lv_color_white(), 0);
    lv_obj_set_style_text_color(g_debug_main_label, lv_color_white(), 0);

    lv_label_set_text(g_debug_fps_label, "FPS: 0");
    lv_label_set_text(g_debug_cpu_label, "CPU: --%");
    lv_label_set_text(g_debug_mem_label, "MEM: --");
    lv_label_set_text(g_debug_lvgl_label, "LVGL: --/--ms");
    lv_label_set_text(g_debug_loop_label, "LOOP: --/--ms");
    lv_label_set_text(g_debug_main_label, "MAIN: --/--ms");

    if (g_debug_timer == NULL) {
        g_debug_timer = lv_timer_create(debug_overlay_refresh_cb, 1000, NULL);
    }
    if (g_debug_timer && g_debug_overlay_enabled) {
        lv_timer_resume(g_debug_timer);
    } else if (g_debug_timer) {
        lv_timer_pause(g_debug_timer);
    }

    if (!g_debug_overlay_enabled) {
        lv_obj_add_flag(g_debug_overlay, LV_OBJ_FLAG_HIDDEN);
    }

    if (g_debug_overlay_enabled) {
        debug_overlay_refresh_cb(NULL);
    }
}

void lv_debug_overlay_set_enabled(bool enabled)
{
    g_debug_overlay_enabled = enabled;

    if (!g_debug_overlay || !lv_obj_is_valid(g_debug_overlay)) {
        return;
    }

    if (enabled) {
        perf_stats_sample();
        lv_obj_clear_flag(g_debug_overlay, LV_OBJ_FLAG_HIDDEN);
        perf_stats_reset_window();
        if (g_debug_timer) {
            lv_timer_resume(g_debug_timer);
            lv_timer_reset(g_debug_timer);
        }
    } else {
        lv_obj_add_flag(g_debug_overlay, LV_OBJ_FLAG_HIDDEN);
        if (g_debug_timer) {
            lv_timer_pause(g_debug_timer);
        }
    }
}

bool lv_debug_overlay_is_enabled(void)
{
    return g_debug_overlay_enabled;
}
