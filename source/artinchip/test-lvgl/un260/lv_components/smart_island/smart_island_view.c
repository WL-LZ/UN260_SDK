#include "un260/lv_components/smart_island/smart_island_internal.h"
#include "un260/lv_components/lv_capsule_pagination.h"
#include "un260/currency/currency_state.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_system/machine_time.h"
#include "un260/counting/counting_data_store.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include <string.h>

#define SMART_ISLAND_X                    492
#define SMART_ISLAND_Y                    348
#define SMART_ISLAND_W                    SMART_ISLAND_WIDTH
#define SMART_ISLAND_COMPACT_H            44
#define SMART_ISLAND_RADIUS               22
#define SMART_ISLAND_MINI_W               180

#define SMART_ISLAND_BG_COUNTING          0x111111
#define SMART_ISLAND_BG_WARNING           0xF59E0B
#define SMART_ISLAND_BG_ERROR             0xFF5A5F
#define SMART_ISLAND_BG_SUCCESS           0x17A673
#define SMART_ISLAND_BG_UPDATE            0x111111
#define SMART_ISLAND_TEXT_LIGHT           0xFFFFFF
#define SMART_ISLAND_TEXT_SUB             0x777777
#define SMART_ISLAND_LAST_TEXT_GRAY       0x737373
#define SMART_ISLAND_RESULT_OK_COLOR      0x22C55E
#define SMART_ISLAND_RESULT_ISSUE_COLOR   0xFF5A5F
#define SMART_ISLAND_RESULT_ISSUE_TITLE_COLOR 0xFFD400
#define SMART_ISLAND_RESULT_DETAIL_GRAY   0xA3A3A3
#define SMART_ISLAND_RESULT_NEUTRAL_GRAY  0x737373
#define SMART_ISLAND_READY_DOT            0x00E676
#define SMART_ISLAND_DOT_NON_IDLE         0xFFFFFF

#define SMART_ISLAND_EXPAND_TIME          300U
#define SMART_ISLAND_COLLAPSE_TIME        250U
#define SMART_ISLAND_COLOR_ANIM_TIME      300U
#define SMART_ISLAND_MESSAGE_PULSE_TIME   180U
#define SMART_ISLAND_COUNT_ROLL_TIME      180U
#define SMART_ISLAND_COUNT_ROLL_MIN_MS    150U

static void smart_island_apply_scene_style(void);
static void smart_island_apply_texts(void);
static void smart_island_rebuild_scene_texts(void);
static void smart_island_get_currency_code(char *buf, size_t size);
static const char *smart_island_get_work_mode_text(void);
static void smart_island_apply_progress(void);
static void smart_island_clear_object_refs(void);
static void smart_island_pulse_stop(void);
static void smart_island_visual_apply_now(smart_island_visual_t visual);
static void smart_island_visual_apply_anim(smart_island_visual_t visual);
static void smart_island_prepare_reparent(void);
static void smart_island_modal_update(void);
static void smart_island_bg_color_apply_anim(uint32_t dst_hex);
static void smart_island_bg_color_anim_ready_cb(lv_anim_t *animation);
static void smart_island_apply_idle_line_text(char *dst,
                                              size_t dst_size,
                                              const char *text);
static void smart_island_apply_quality_indicator(void);
static void smart_island_counting_apply_visibility(void);
static void smart_island_counting_anim_start(void);
static void smart_island_counting_anim_stop(void);
static void smart_island_counting_value_apply(bool anim_en);

static const char *smart_island_get_work_mode_text(void)
{
    return machine_state_work_mode() ? ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_MODE_MANUAL)
                                  : ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_MODE_AUTO);
}

static void smart_island_anim_w_cb(void *var, int32_t v) { lv_obj_set_width((lv_obj_t *)var, (lv_coord_t)v); }

static void smart_island_anim_h_cb(void *var, int32_t v) { lv_obj_set_height((lv_obj_t *)var, (lv_coord_t)v); }

void smart_island_anim_x_cb(void *var, int32_t v) { lv_obj_set_x((lv_obj_t *)var, (lv_coord_t)v); }

static void smart_island_anim_y_cb(void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, (lv_coord_t)v); }

static void smart_island_anim_zoom_cb(void *var, int32_t v) { lv_obj_set_style_transform_zoom((lv_obj_t *)var, (lv_coord_t)v, 0); }

static void smart_island_anim_stretch_cb(void *var, int32_t v)
{
    lv_obj_set_style_transform_width((lv_obj_t *)var, (lv_coord_t)v, 0);
}

static void smart_island_counting_bill_anim_cb(void *var, int32_t v)
{
    lv_obj_set_x((lv_obj_t *)var, (lv_coord_t)v);
}

static void smart_island_serial_scan_anim_cb(void *var, int32_t v)
{
    lv_obj_set_x((lv_obj_t *)var, (lv_coord_t)v);
}

static void smart_island_sensor_opa_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void smart_island_dot_opa_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void smart_island_text_opa_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_text_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void smart_island_counting_value_y_cb(void *var, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)var, (lv_coord_t)v);
}

void smart_island_anim_translate_x_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, (lv_coord_t)v, 0);
}

static void smart_island_anim_bg_color_cb(void *var, int32_t v)
{
    lv_color_t from_c;
    lv_color_t to_c;
    lv_color_t mix_c;

    if (var == NULL) {
        return;
    }

    from_c = lv_color_hex(g_si_ctx.view.bg_from);
    to_c = lv_color_hex(g_si_ctx.view.bg_to);
    mix_c = lv_color_mix(to_c, from_c, (lv_opa_t)v);
    lv_obj_set_style_bg_color((lv_obj_t *)var, mix_c, 0);
}

static void smart_island_anim_finish_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    g_si_ctx.view.anim_running = false;
}

static void smart_island_message_pulse_finish_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    g_si_ctx.view.message_pulse_running = false;
    if (g_si_ctx.objects.root && lv_obj_is_valid(g_si_ctx.objects.root)) {
        lv_obj_set_style_transform_width(g_si_ctx.objects.root, 0, 0);
    }
    if (g_si_ctx.objects.dot && lv_obj_is_valid(g_si_ctx.objects.dot)) {
        lv_obj_set_style_translate_x(g_si_ctx.objects.dot, 0, 0);
    }
    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title)) {
        lv_obj_set_style_translate_x(g_si_ctx.objects.title, 0, 0);
    }
}

static void smart_island_result_collapse_finish_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    g_si_ctx.view.anim_running = false;
    /* Keep COMPLETE visible for the whole damping motion.  READY is applied
     * only after geometry has settled, so the two scenes never overlap. */
    smart_island_restore_idle();
}

static void smart_island_counting_value_finish_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    if (g_si_ctx.objects.counting_value == NULL ||
        !lv_obj_is_valid(g_si_ctx.objects.counting_value) ||
        g_si_ctx.objects.counting_value_next == NULL ||
        !lv_obj_is_valid(g_si_ctx.objects.counting_value_next)) {
        return;
    }

    lv_label_set_text(g_si_ctx.objects.counting_value,
                      g_si_ctx.counting.display_value);
    lv_obj_set_y(g_si_ctx.objects.counting_value, 10);
    lv_obj_set_y(g_si_ctx.objects.counting_value_next, SMART_ISLAND_COUNT_H);
    lv_obj_add_flag(g_si_ctx.objects.counting_value_next, LV_OBJ_FLAG_HIDDEN);
}

static void smart_island_bg_color_anim_ready_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    g_si_ctx.view.bg_anim_running = false;
    g_si_ctx.view.bg_current = g_si_ctx.view.bg_to;
}

static void smart_island_bg_color_apply_anim(uint32_t dst_hex)
{
    lv_anim_t a;
    lv_color_t style_c;

    if (g_si_ctx.objects.root == NULL || !lv_obj_is_valid(g_si_ctx.objects.root)) {
        return;
    }

    if (g_si_ctx.view.bg_anim_running && g_si_ctx.view.bg_to == dst_hex) {
        return;
    }

    if (!g_si_ctx.view.bg_anim_running && g_si_ctx.view.bg_current == dst_hex) {
        lv_obj_set_style_bg_color(g_si_ctx.objects.root, lv_color_hex(dst_hex), 0);
        return;
    }

    style_c = lv_obj_get_style_bg_color(g_si_ctx.objects.root, LV_PART_MAIN);
    g_si_ctx.view.bg_from = lv_color_to32(style_c);
    g_si_ctx.view.bg_to = dst_hex;

    lv_anim_del(g_si_ctx.objects.root, smart_island_anim_bg_color_cb);
    lv_anim_init(&a);
    lv_anim_set_var(&a, g_si_ctx.objects.root);
    lv_anim_set_exec_cb(&a, smart_island_anim_bg_color_cb);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, SMART_ISLAND_COLOR_ANIM_TIME);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_ready_cb(&a, smart_island_bg_color_anim_ready_cb);
    lv_anim_start(&a);
    g_si_ctx.view.bg_anim_running = true;
}

void smart_island_reset_page_positions(void)
{
    if (g_si_ctx.objects.page_root && lv_obj_is_valid(g_si_ctx.objects.page_root)) lv_obj_set_x(g_si_ctx.objects.page_root, 0);
    if (g_si_ctx.objects.page_info && lv_obj_is_valid(g_si_ctx.objects.page_info)) lv_obj_set_x(g_si_ctx.objects.page_info, 0);
    if (g_si_ctx.objects.page_action && lv_obj_is_valid(g_si_ctx.objects.page_action)) lv_obj_set_x(g_si_ctx.objects.page_action, 0);
    if (g_si_ctx.objects.action_track && lv_obj_is_valid(g_si_ctx.objects.action_track)) {
        lv_obj_set_x(g_si_ctx.objects.action_track, -(lv_coord_t)g_si_ctx.action.page_index * SMART_ISLAND_W);
    }
}

void smart_island_reset_compact_header_position(void)
{
    if (g_si_ctx.objects.dot && lv_obj_is_valid(g_si_ctx.objects.dot)) {
        lv_obj_set_x(g_si_ctx.objects.dot, 16);
        lv_obj_set_y(g_si_ctx.objects.dot, 17);
        lv_obj_set_style_translate_x(g_si_ctx.objects.dot, 0, 0);
    }
    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title)) {
        lv_label_set_long_mode(g_si_ctx.objects.title, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(g_si_ctx.objects.title, 150);
        lv_obj_set_x(g_si_ctx.objects.title, 36);
        lv_obj_set_y(g_si_ctx.objects.title, 13);
        lv_obj_set_style_translate_x(g_si_ctx.objects.title, 0, 0);
    }
    if (g_si_ctx.objects.expand_title && lv_obj_is_valid(g_si_ctx.objects.expand_title)) {
        lv_label_set_long_mode(g_si_ctx.objects.expand_title, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(g_si_ctx.objects.expand_title, 213);
        lv_obj_set_x(g_si_ctx.objects.expand_title, 24);
        lv_obj_set_y(g_si_ctx.objects.expand_title, 30);
    }
}

void smart_island_reset_time_position(void)
{
    if (g_si_ctx.objects.time == NULL || !lv_obj_is_valid(g_si_ctx.objects.time)) {
        return;
    }

    if (g_si_ctx.view.visual == SMART_ISLAND_VISUAL_EXPANDED) {
        lv_obj_align(g_si_ctx.objects.time, LV_ALIGN_TOP_RIGHT, -14, 12);
    } else {
        lv_obj_align(g_si_ctx.objects.time, LV_ALIGN_RIGHT_MID, -14, 0);
    }
}

const char *smart_island_text_or_default(const char *text, ui_text_id_t text_id)
{
    if (text && text[0] != '\0') {
        return text;
    }

    return ui_text_get(text_id);
}

static void smart_island_clear_object_refs(void)
{
    uint8_t i;

    g_si_ctx.objects.root = NULL;
    g_si_ctx.objects.modal = NULL;
    g_si_ctx.objects.dot = NULL;
    g_si_ctx.objects.title = NULL;
    g_si_ctx.objects.subtitle = NULL;
    g_si_ctx.objects.time = NULL;
    g_si_ctx.objects.badge = NULL;
    g_si_ctx.objects.progress = NULL;
    g_si_ctx.objects.counting_root = NULL;
    g_si_ctx.objects.counting_gate = NULL;
    g_si_ctx.objects.counting_gate_left = NULL;
    g_si_ctx.objects.counting_gate_right = NULL;
    g_si_ctx.objects.counting_sensor = NULL;
    g_si_ctx.objects.counting_bill = NULL;
    g_si_ctx.objects.counting_bill_mid = NULL;
    g_si_ctx.objects.counting_bill_low = NULL;
    g_si_ctx.objects.counting_value = NULL;
    g_si_ctx.objects.counting_value_next = NULL;
    g_si_ctx.objects.counting_value_track = NULL;
    g_si_ctx.objects.counting_value_scan = NULL;
    g_si_ctx.objects.counting_unit = NULL;
    g_si_ctx.objects.counting_divider = NULL;
    g_si_ctx.objects.counting_serial = NULL;
    g_si_ctx.objects.counting_serial_track = NULL;
    g_si_ctx.objects.counting_serial_scan = NULL;
    g_si_ctx.objects.page_root = NULL;
    g_si_ctx.objects.page_info = NULL;
    g_si_ctx.objects.page_action = NULL;
    g_si_ctx.objects.expand_title = NULL;
    g_si_ctx.objects.expand_subtitle = NULL;
    g_si_ctx.objects.expand_last = NULL;
    g_si_ctx.objects.expand_divider = NULL;
    g_si_ctx.objects.expand_footer = NULL;
    g_si_ctx.objects.expand_extra = NULL;
    g_si_ctx.objects.action_track = NULL;
    g_si_ctx.objects.page_indicator = NULL;
    g_si_ctx.objects.quality_bar_bg = NULL;
    g_si_ctx.objects.quality_bar_fg = NULL;
    g_si_ctx.objects.quality_percent = NULL;

    for (i = 0; i < SMART_ISLAND_ACTION_PAGE_COUNT; i++) {
        g_si_ctx.objects.action_buttons[i] = NULL;
        g_si_ctx.objects.action_labels[i] = NULL;
        g_si_ctx.objects.action_arrows[i] = NULL;
    }
}

void smart_island_update_idle_time(void)
{
    machine_time_value_t now;
    char buf[16];
    if (g_si_ctx.objects.time == NULL || !lv_obj_is_valid(g_si_ctx.objects.time)) return;
    machine_time_get(&now);
    lv_snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
        (unsigned)now.hour,
        (unsigned)now.minute,
        (unsigned)now.second);
    lv_label_set_text(g_si_ctx.objects.time, buf);
}

static void smart_island_get_currency_code(char *buf, size_t size)
{
    char curr_code[4];

    if (buf == NULL || size == 0U) return;

    if (!counting_data_monetary_result_supported(counting_data_current())) {
        lv_snprintf(buf, size, "%s", currency_state_display_code(CURRENCY_MULTI_CODE));
        return;
    }

    /* Keep the island consistent with the main page: AUTO is shown until
     * the controller explicitly reports a detected currency via 0x50. */
    currency_state_get_effective_code(curr_code);
    if (curr_code[0] != '\0') {
        lv_snprintf(buf, size, "%s", currency_state_display_code(curr_code));
    } else {
        lv_snprintf(buf, size, "%s", "CUR");
    }
}

static void smart_island_apply_progress(void)
{
    lv_obj_t *progress = g_si_ctx.objects.progress;

    if (progress == NULL || !lv_obj_is_valid(progress)) {
        return;
    }

    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_UPDATE) {
        lv_obj_clear_flag(progress, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(progress, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(progress, 0, LV_ANIM_OFF);
}

static void smart_island_counting_anim_stop(void)
{
    lv_obj_t *bills[] = {
        g_si_ctx.objects.counting_bill,
        g_si_ctx.objects.counting_bill_mid,
        g_si_ctx.objects.counting_bill_low,
    };
    lv_obj_t *sensor = g_si_ctx.objects.counting_sensor;
    lv_obj_t *scan = g_si_ctx.objects.counting_serial_scan;
    lv_obj_t *value_scan = g_si_ctx.objects.counting_value_scan;

    for (size_t i = 0; i < sizeof(bills) / sizeof(bills[0]); i++) {
        if (bills[i] && lv_obj_is_valid(bills[i])) {
            lv_anim_del(bills[i], smart_island_counting_bill_anim_cb);
            lv_obj_set_x(bills[i], -16);
        }
    }
    if (sensor && lv_obj_is_valid(sensor)) {
        lv_anim_del(sensor, smart_island_counting_bill_anim_cb);
        lv_anim_del(sensor, smart_island_sensor_opa_anim_cb);
        lv_obj_set_x(sensor, 8);
        lv_obj_set_style_bg_opa(sensor, LV_OPA_30, 0);
    }
    if (scan && lv_obj_is_valid(scan)) {
        lv_anim_del(scan, smart_island_serial_scan_anim_cb);
        lv_obj_set_x(scan, 0);
    }
    if (value_scan && lv_obj_is_valid(value_scan)) {
        lv_anim_del(value_scan, smart_island_serial_scan_anim_cb);
        lv_obj_set_x(value_scan, 0);
    }
    g_si_ctx.counting.gate_anim_running = false;
    g_si_ctx.counting.serial_anim_running = false;
}

static void smart_island_counting_anim_start(void)
{
    lv_anim_t animation;
    lv_obj_t *bills[] = {
        g_si_ctx.objects.counting_bill,
        g_si_ctx.objects.counting_bill_mid,
        g_si_ctx.objects.counting_bill_low,
    };
    const uint16_t durations[] = { 680, 730, 780 };
    const uint16_t delays[] = { 0, 210, 420 };
    lv_obj_t *sensor = g_si_ctx.objects.counting_sensor;

    if (g_si_ctx.counting.gate_anim_running ||
        bills[0] == NULL || !lv_obj_is_valid(bills[0])) {
        return;
    }

    /* Three thin note traces continuously cross a fixed sensor gate.  Their
     * slightly different periods avoid a mechanical equalizer rhythm while
     * clipping outside the gate makes each trace enter and leave naturally. */
    for (size_t i = 0; i < sizeof(bills) / sizeof(bills[0]); i++) {
        if (bills[i] == NULL || !lv_obj_is_valid(bills[i])) {
            continue;
        }
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, bills[i]);
        lv_anim_set_exec_cb(&animation, smart_island_counting_bill_anim_cb);
        lv_anim_set_values(&animation, -16, 41);
        lv_anim_set_time(&animation, durations[i]);
        lv_anim_set_delay(&animation, delays[i]);
        lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&animation, lv_anim_path_linear);
        lv_anim_start(&animation);
    }

    if (sensor && lv_obj_is_valid(sensor)) {
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, sensor);
        lv_anim_set_exec_cb(&animation, smart_island_counting_bill_anim_cb);
        lv_anim_set_values(&animation, 8, 32);
        lv_anim_set_time(&animation, 720);
        lv_anim_set_repeat_delay(&animation, 80);
        lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);

        lv_anim_init(&animation);
        lv_anim_set_var(&animation, sensor);
        lv_anim_set_exec_cb(&animation, smart_island_sensor_opa_anim_cb);
        lv_anim_set_values(&animation, LV_OPA_30, LV_OPA_COVER);
        lv_anim_set_time(&animation, 720);
        lv_anim_set_repeat_delay(&animation, 80);
        lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
    g_si_ctx.counting.gate_anim_running = true;

    if (g_si_ctx.objects.counting_serial_scan &&
        lv_obj_is_valid(g_si_ctx.objects.counting_serial_scan)) {
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.counting_serial_scan);
        lv_anim_set_exec_cb(&animation, smart_island_serial_scan_anim_cb);
        lv_anim_set_values(&animation, 0, 54);
        lv_anim_set_time(&animation, 680);
        lv_anim_set_playback_time(&animation, 220);
        lv_anim_set_repeat_delay(&animation, 120);
        lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
         lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
        g_si_ctx.counting.serial_anim_running = true;
    }
    if (g_si_ctx.objects.counting_value_scan &&
        lv_obj_is_valid(g_si_ctx.objects.counting_value_scan)) {
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.counting_value_scan);
        lv_anim_set_exec_cb(&animation, smart_island_serial_scan_anim_cb);
        lv_anim_set_values(&animation, 0, 24);
        lv_anim_set_time(&animation, 610);
        lv_anim_set_playback_time(&animation, 190);
        lv_anim_set_repeat_delay(&animation, 100);
        lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_in);
        lv_anim_start(&animation);
    }
}

static void smart_island_counting_value_apply(bool anim_en)
{
    char next_value[16];
    uint32_t now;
    lv_anim_t animation;

    if (g_si_ctx.objects.counting_value == NULL ||
        !lv_obj_is_valid(g_si_ctx.objects.counting_value) ||
        g_si_ctx.objects.counting_value_next == NULL ||
        !lv_obj_is_valid(g_si_ctx.objects.counting_value_next)) {
        return;
    }

    if (g_si_ctx.counting.mode == MODE_CNT) {
        lv_snprintf(next_value, sizeof(next_value), "%d", g_si_ctx.counting.pcs);
    } else if (g_si_ctx.counting.denomination > 0) {
        lv_snprintf(next_value, sizeof(next_value), "%d",
                    g_si_ctx.counting.denomination);
    } else {
        lv_snprintf(next_value, sizeof(next_value), "%s", "--");
    }

    if (g_si_ctx.counting.value_initialized &&
        strcmp(g_si_ctx.counting.display_value, next_value) == 0) {
        return;
    }

    now = lv_tick_get();
    lv_anim_del(g_si_ctx.objects.counting_value, smart_island_counting_value_y_cb);
    lv_anim_del(g_si_ctx.objects.counting_value_next, smart_island_counting_value_y_cb);

    if (!g_si_ctx.counting.value_initialized || !anim_en ||
        lv_tick_elaps(g_si_ctx.counting.last_roll_tick) < SMART_ISLAND_COUNT_ROLL_MIN_MS) {
        lv_snprintf(g_si_ctx.counting.display_value,
                    sizeof(g_si_ctx.counting.display_value), "%s", next_value);
        lv_label_set_text(g_si_ctx.objects.counting_value,
                          g_si_ctx.counting.display_value);
        lv_obj_set_y(g_si_ctx.objects.counting_value, 10);
        lv_obj_set_y(g_si_ctx.objects.counting_value_next, SMART_ISLAND_COUNT_H);
        lv_obj_add_flag(g_si_ctx.objects.counting_value_next, LV_OBJ_FLAG_HIDDEN);
        g_si_ctx.counting.value_initialized = true;
        g_si_ctx.counting.last_roll_tick = now;
        return;
    }

    lv_label_set_text(g_si_ctx.objects.counting_value,
                      g_si_ctx.counting.display_value);
    lv_obj_set_y(g_si_ctx.objects.counting_value, 10);
    lv_label_set_text(g_si_ctx.objects.counting_value_next, next_value);
    lv_obj_set_y(g_si_ctx.objects.counting_value_next, SMART_ISLAND_COUNT_H);
    lv_obj_clear_flag(g_si_ctx.objects.counting_value_next, LV_OBJ_FLAG_HIDDEN);
    lv_snprintf(g_si_ctx.counting.display_value,
                sizeof(g_si_ctx.counting.display_value), "%s", next_value);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, g_si_ctx.objects.counting_value);
    lv_anim_set_exec_cb(&animation, smart_island_counting_value_y_cb);
    lv_anim_set_values(&animation, 10, -28);
    lv_anim_set_time(&animation, SMART_ISLAND_COUNT_ROLL_TIME);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);

    lv_anim_set_var(&animation, g_si_ctx.objects.counting_value_next);
    lv_anim_set_values(&animation, SMART_ISLAND_COUNT_H, 10);
    lv_anim_set_ready_cb(&animation, smart_island_counting_value_finish_cb);
    lv_anim_start(&animation);
    g_si_ctx.counting.last_roll_tick = now;
}

static void smart_island_counting_apply_visibility(void)
{
    bool visible =
        g_si_ctx.view.scene == SMART_ISLAND_SCENE_COUNTING &&
        g_si_ctx.view.visual != SMART_ISLAND_VISUAL_EXPANDED;
    bool cnt_mode = g_si_ctx.counting.mode == MODE_CNT;
    bool has_denomination = g_si_ctx.counting.denomination > 0;
    bool has_serial = g_si_ctx.counting.serial[0] != '\0';
    lv_coord_t root_w = SMART_ISLAND_COUNT_W;

    if (g_si_ctx.objects.counting_root == NULL ||
        !lv_obj_is_valid(g_si_ctx.objects.counting_root)) {
        return;
    }

    if (!visible) {
        lv_obj_add_flag(g_si_ctx.objects.counting_root, LV_OBJ_FLAG_HIDDEN);
        smart_island_counting_anim_stop();
        return;
    }

    lv_obj_set_size(g_si_ctx.objects.counting_root, root_w, SMART_ISLAND_COUNT_H);
    lv_obj_clear_flag(g_si_ctx.objects.counting_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_si_ctx.objects.counting_root);

    if (g_si_ctx.objects.counting_value &&
        lv_obj_is_valid(g_si_ctx.objects.counting_value)) {
        lv_obj_set_width(g_si_ctx.objects.counting_value, cnt_mode ? 92 : 58);
        lv_obj_set_x(g_si_ctx.objects.counting_value, cnt_mode ? 92 : 69);
        lv_obj_set_style_text_align(g_si_ctx.objects.counting_value,
                                    LV_TEXT_ALIGN_RIGHT, 0);
    }
    if (g_si_ctx.objects.counting_value_next &&
        lv_obj_is_valid(g_si_ctx.objects.counting_value_next)) {
        lv_obj_set_width(g_si_ctx.objects.counting_value_next, cnt_mode ? 92 : 58);
        lv_obj_set_x(g_si_ctx.objects.counting_value_next, cnt_mode ? 92 : 69);
        lv_obj_set_style_text_align(g_si_ctx.objects.counting_value_next,
                                    LV_TEXT_ALIGN_RIGHT, 0);
    }

    if (cnt_mode) {
        lv_obj_clear_flag(g_si_ctx.objects.counting_value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_si_ctx.objects.counting_value_track, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_si_ctx.objects.counting_unit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(g_si_ctx.objects.counting_unit, 192, 16);
        lv_obj_add_flag(g_si_ctx.objects.counting_divider, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_si_ctx.objects.counting_serial, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_si_ctx.objects.counting_serial_track, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_si_ctx.objects.counting_unit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_si_ctx.objects.counting_divider, LV_OBJ_FLAG_HIDDEN);
        if (has_denomination) {
            lv_obj_clear_flag(g_si_ctx.objects.counting_value, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(g_si_ctx.objects.counting_value_track,
                            LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_si_ctx.objects.counting_value, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(g_si_ctx.objects.counting_value_next,
                            LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_si_ctx.objects.counting_value_track,
                              LV_OBJ_FLAG_HIDDEN);
        }
        if (has_serial) {
            lv_label_set_text(g_si_ctx.objects.counting_serial,
                              g_si_ctx.counting.serial);
            lv_obj_clear_flag(g_si_ctx.objects.counting_serial,
                              LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(g_si_ctx.objects.counting_serial_track,
                            LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_si_ctx.objects.counting_serial,
                            LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_si_ctx.objects.counting_serial_track,
                              LV_OBJ_FLAG_HIDDEN);
        }
    }

    smart_island_counting_value_apply(true);
    smart_island_counting_anim_start();
}

static void smart_island_apply_idle_line_text(char *dst, size_t dst_size, const char *text)
{
    if (dst == NULL || dst_size == 0U) {
        return;
    }

    if (text && text[0] != '\0') {
        lv_snprintf(dst, dst_size, "%s", text);
    } else {
        dst[0] = '\0';
    }
}

static void smart_island_apply_quality_indicator(void)
{
    lv_coord_t fg_w;
    char percent_buf[8];

    if (g_si_ctx.objects.quality_bar_bg == NULL || !lv_obj_is_valid(g_si_ctx.objects.quality_bar_bg) ||
        g_si_ctx.objects.quality_bar_fg == NULL || !lv_obj_is_valid(g_si_ctx.objects.quality_bar_fg) ||
        g_si_ctx.objects.quality_percent == NULL || !lv_obj_is_valid(g_si_ctx.objects.quality_percent)) {
        return;
    }

    /* 不要跟随 page 在切页动画中反复 hidden/unhidden，避免回到信息页时闪一下 */
    if (!(g_si_ctx.view.scene == SMART_ISLAND_SCENE_IDLE &&
          g_si_ctx.view.visual == SMART_ISLAND_VISUAL_EXPANDED)) {
        lv_obj_add_flag(g_si_ctx.objects.quality_bar_bg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_si_ctx.objects.quality_percent, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    fg_w = (lv_coord_t)((56 * g_si_ctx.text.idle_quality_percent) / 100);
    if (fg_w < 0) {
        fg_w = 0;
    }
    if (fg_w > 56) {
        fg_w = 56;
    }

    lv_obj_set_width(g_si_ctx.objects.quality_bar_fg, fg_w);

    if (!g_si_ctx.text.idle_has_data) {
        lv_obj_set_style_bg_color(g_si_ctx.objects.quality_bar_bg, lv_color_hex(SMART_ISLAND_RESULT_NEUTRAL_GRAY), 0);
        lv_obj_set_style_bg_color(g_si_ctx.objects.quality_bar_fg, lv_color_hex(SMART_ISLAND_RESULT_NEUTRAL_GRAY), 0);
    } else {
        lv_obj_set_style_bg_color(g_si_ctx.objects.quality_bar_bg, lv_color_hex(SMART_ISLAND_RESULT_ISSUE_COLOR), 0);
        lv_obj_set_style_bg_color(g_si_ctx.objects.quality_bar_fg, lv_color_hex(SMART_ISLAND_RESULT_OK_COLOR), 0);
    }

    lv_snprintf(percent_buf, sizeof(percent_buf), "%u%%", (unsigned)g_si_ctx.text.idle_quality_percent);
    lv_label_set_text(g_si_ctx.objects.quality_percent, percent_buf);
    if (!g_si_ctx.text.idle_has_data) {
        lv_obj_set_style_text_color(g_si_ctx.objects.quality_percent, lv_color_hex(SMART_ISLAND_RESULT_NEUTRAL_GRAY), 0);
    } else if (g_si_ctx.text.idle_has_issue) {
        lv_obj_set_style_text_color(g_si_ctx.objects.quality_percent, lv_color_hex(SMART_ISLAND_TEXT_LIGHT), 0);
    } else {
        lv_obj_set_style_text_color(g_si_ctx.objects.quality_percent, lv_color_hex(SMART_ISLAND_RESULT_OK_COLOR), 0);
    }

    lv_obj_clear_flag(g_si_ctx.objects.quality_bar_bg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_si_ctx.objects.quality_percent, LV_OBJ_FLAG_HIDDEN);
}

static void smart_island_apply_texts(void)
{
    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title)) {
        lv_label_set_text(g_si_ctx.objects.title, g_si_ctx.text.compact);
    }
    if (g_si_ctx.objects.expand_title && lv_obj_is_valid(g_si_ctx.objects.expand_title)) {
        lv_label_set_text(g_si_ctx.objects.expand_title, g_si_ctx.text.info_title);
    }
    if (g_si_ctx.objects.expand_subtitle && lv_obj_is_valid(g_si_ctx.objects.expand_subtitle)) {
        lv_label_set_text(g_si_ctx.objects.expand_subtitle, g_si_ctx.text.info_summary);
    }
    if (g_si_ctx.objects.expand_last && lv_obj_is_valid(g_si_ctx.objects.expand_last)) {
        lv_label_set_text(g_si_ctx.objects.expand_last, ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_LAST_TAG));
    }
    if (g_si_ctx.objects.expand_footer && lv_obj_is_valid(g_si_ctx.objects.expand_footer)) {
        lv_label_set_text(g_si_ctx.objects.expand_footer, g_si_ctx.text.info_footer);
        if (g_si_ctx.text.info_footer[0] != '\0') lv_obj_clear_flag(g_si_ctx.objects.expand_footer, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(g_si_ctx.objects.expand_footer, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_si_ctx.objects.expand_extra && lv_obj_is_valid(g_si_ctx.objects.expand_extra)) {
        lv_label_set_text(g_si_ctx.objects.expand_extra, g_si_ctx.text.info_extra);
        if (g_si_ctx.text.info_extra[0] != '\0') lv_obj_clear_flag(g_si_ctx.objects.expand_extra, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(g_si_ctx.objects.expand_extra, LV_OBJ_FLAG_HIDDEN);
    }
}

static void smart_island_rebuild_scene_texts(void)
{
    char curr[8] = {0};
    char detail_line[96] = {0};
    const char *work_text = smart_island_get_work_mode_text();
    bool monetary_supported = !currency_state_multi_selected() &&
        counting_data_monetary_result_supported(counting_data_current());
    memset(g_si_ctx.text.compact, 0, sizeof(g_si_ctx.text.compact));
    memset(g_si_ctx.text.info_title, 0, sizeof(g_si_ctx.text.info_title));
    memset(g_si_ctx.text.info_summary, 0, sizeof(g_si_ctx.text.info_summary));
    memset(g_si_ctx.text.info_footer, 0, sizeof(g_si_ctx.text.info_footer));
    memset(g_si_ctx.text.info_extra, 0, sizeof(g_si_ctx.text.info_extra));

    smart_island_get_currency_code(curr, sizeof(curr));

    switch (g_si_ctx.view.scene) {
    case SMART_ISLAND_SCENE_COUNTING:
        if (machine_state_mode() == MODE_MDC || machine_state_mode() == MODE_SDC) {
            if (g_si_ctx.text.serial_ticker[0] != '\0') {
                lv_snprintf(g_si_ctx.text.compact, sizeof(g_si_ctx.text.compact), "%s",
                    g_si_ctx.text.serial_ticker);
            }
        } else {
            lv_snprintf(g_si_ctx.text.compact, sizeof(g_si_ctx.text.compact), "%s",
                ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_COUNTING_TITLE));
        }
        lv_snprintf(g_si_ctx.text.info_title, sizeof(g_si_ctx.text.info_title), "%s",
            ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_COUNTING_INFO_TITLE));
        if (monetary_supported && counting_data_current()->total_pcs > 0 && counting_data_current()->total_amount > 0.0f) {
            lv_snprintf(g_si_ctx.text.info_summary, sizeof(g_si_ctx.text.info_summary),
                ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_CUR_PCS_AMOUNT_FMT), curr, counting_data_current()->total_pcs, counting_data_current()->total_amount);
        } else if (counting_data_current()->total_pcs > 0) {
            lv_snprintf(g_si_ctx.text.info_summary, sizeof(g_si_ctx.text.info_summary),
                ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_CUR_PCS_FMT), curr, counting_data_current()->total_pcs);
        } else {
            lv_snprintf(g_si_ctx.text.info_summary, sizeof(g_si_ctx.text.info_summary),
                ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_CUR_MODE_FMT), curr, work_text);
        }
        lv_snprintf(g_si_ctx.text.info_footer, sizeof(g_si_ctx.text.info_footer), "%s", work_text);
        break;

    case SMART_ISLAND_SCENE_RESULT:
        lv_snprintf(g_si_ctx.text.compact, sizeof(g_si_ctx.text.compact),
                    "%d PCS  %s",
                    g_si_ctx.counting.pcs,
                    ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_COUNT_FINISHED));
        g_si_ctx.text.info_title[0] = '\0';
        g_si_ctx.text.info_summary[0] = '\0';
        g_si_ctx.text.info_footer[0] = '\0';
        g_si_ctx.text.info_extra[0] = '\0';
        break;

    case SMART_ISLAND_SCENE_WARNING:
        lv_snprintf(g_si_ctx.text.compact, sizeof(g_si_ctx.text.compact), "%s",
            smart_island_text_or_default(g_si_ctx.warning.text, UI_TEXT_WIDGET_SMART_ISLAND_COUNT_ERROR));
        lv_snprintf(g_si_ctx.text.info_title, sizeof(g_si_ctx.text.info_title), "%s",
            smart_island_text_or_default(g_si_ctx.warning.text, UI_TEXT_WIDGET_SMART_ISLAND_COUNT_ERROR));
        lv_snprintf(g_si_ctx.text.info_summary, sizeof(g_si_ctx.text.info_summary), "%s",
            smart_island_text_or_default(g_si_ctx.warning.text, UI_TEXT_WIDGET_SMART_ISLAND_COUNT_ERROR));
        lv_snprintf(g_si_ctx.text.info_footer, sizeof(g_si_ctx.text.info_footer), "%s",
            "");
        break;

    case SMART_ISLAND_SCENE_UPDATE:
        lv_snprintf(g_si_ctx.text.compact, sizeof(g_si_ctx.text.compact), "%s",
            smart_island_text_or_default(g_si_ctx.view.content.title, UI_TEXT_WIDGET_SMART_ISLAND_UPDATE_TITLE));
        lv_snprintf(g_si_ctx.text.info_title, sizeof(g_si_ctx.text.info_title), "%s",
            smart_island_text_or_default(g_si_ctx.view.content.title, UI_TEXT_WIDGET_SMART_ISLAND_UPDATE_TITLE));
        lv_snprintf(g_si_ctx.text.info_summary, sizeof(g_si_ctx.text.info_summary), "%s",
            smart_island_text_or_default(g_si_ctx.view.content.subtitle, UI_TEXT_WIDGET_SMART_ISLAND_UPDATE_SUBTITLE));
        lv_snprintf(g_si_ctx.text.info_footer, sizeof(g_si_ctx.text.info_footer), "%s",
            ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_UPDATE_INFO_FOOTER));
        break;

    case SMART_ISLAND_SCENE_QR:
        lv_snprintf(g_si_ctx.text.compact, sizeof(g_si_ctx.text.compact), "%s",
            ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_QR_READY));
        lv_snprintf(g_si_ctx.text.info_title, sizeof(g_si_ctx.text.info_title), "%s",
            smart_island_text_or_default(g_si_ctx.view.content.title, UI_TEXT_WIDGET_SMART_ISLAND_QR_READY));
        lv_snprintf(g_si_ctx.text.info_summary, sizeof(g_si_ctx.text.info_summary), "%s",
            ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_QR_INFO_SUBTITLE));
        if (!monetary_supported) {
            lv_snprintf(g_si_ctx.text.info_footer, sizeof(g_si_ctx.text.info_footer),
                ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_CUR_PCS_FMT), curr, counting_data_current()->total_pcs);
        } else if (counting_data_current()->total_pcs > 0 && counting_data_current()->total_amount > 0.0f) {
            lv_snprintf(g_si_ctx.text.info_footer, sizeof(g_si_ctx.text.info_footer),
                ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_CUR_PCS_AMOUNT_FMT), curr, counting_data_current()->total_pcs, counting_data_current()->total_amount);
        } else {
            lv_snprintf(g_si_ctx.text.info_footer, sizeof(g_si_ctx.text.info_footer), "%s",
                ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_QR_INFO_FOOTER));
        }
        break;

    case SMART_ISLAND_SCENE_IDLE:
    default:
    {
        int current_valid = g_si_ctx.text.analysis_valid ? g_si_ctx.text.analysis_valid_pcs : 0;
        int current_total = 0;
        int current_suspect = g_si_ctx.text.analysis_valid ? g_si_ctx.text.analysis_suspect_pcs : 0;
        int current_damaged = g_si_ctx.text.analysis_valid ? g_si_ctx.text.analysis_damaged_pcs : 0;
        int current_issue = current_suspect + current_damaged;

        lv_snprintf(g_si_ctx.text.compact, sizeof(g_si_ctx.text.compact),
            ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_READY_CUR_FMT), curr);

        g_si_ctx.text.info_title[0] = '\0';

        current_total = current_valid + current_issue;
        g_si_ctx.text.idle_has_issue = (current_issue > 0);
        g_si_ctx.text.idle_has_data = g_si_ctx.text.analysis_valid && current_total > 0;
        g_si_ctx.text.idle_no_count = !g_si_ctx.text.analysis_valid;

        if (!monetary_supported) {
            lv_snprintf(g_si_ctx.text.info_summary, sizeof(g_si_ctx.text.info_summary),
                ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_CUR_PCS_FMT), curr,
                counting_data_current()->last_total_pcs);
        } else if (g_si_ctx.text.idle_line1[0] != '\0') {
            lv_snprintf(g_si_ctx.text.info_summary, sizeof(g_si_ctx.text.info_summary), "%s",
                g_si_ctx.text.idle_line1);
        } else if (counting_data_current()->last_total_pcs > 0 || counting_data_current()->last_total_amount > 0.0f) {
            lv_snprintf(g_si_ctx.text.info_summary, sizeof(g_si_ctx.text.info_summary),
                ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_PCS_AMOUNT_FMT), counting_data_current()->last_total_pcs, counting_data_current()->last_total_amount);
        } else {
            lv_snprintf(g_si_ctx.text.info_summary, sizeof(g_si_ctx.text.info_summary),
                ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_PCS_AMOUNT_FMT), 0, 0.0f);
        }

        if (g_si_ctx.text.idle_line2[0] != '\0') {
            lv_snprintf(g_si_ctx.text.info_footer, sizeof(g_si_ctx.text.info_footer), "%s",
                g_si_ctx.text.idle_line2);
        } else if (g_si_ctx.text.idle_no_count) {
            lv_snprintf(g_si_ctx.text.info_footer, sizeof(g_si_ctx.text.info_footer), "%s",
                ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_IDLE_NO_COUNT));
        } else {
            lv_snprintf(g_si_ctx.text.info_footer, sizeof(g_si_ctx.text.info_footer), "%s",
                g_si_ctx.text.idle_has_issue
                    ? ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_RESULT_ISSUE_TITLE)
                    : ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_RESULT_OK_TITLE));
        }

        if (g_si_ctx.text.idle_line3[0] != '\0') {
            lv_snprintf(g_si_ctx.text.info_extra, sizeof(g_si_ctx.text.info_extra), "%s",
                g_si_ctx.text.idle_line3);
        } else if (g_si_ctx.text.idle_no_count) {
            lv_snprintf(g_si_ctx.text.info_extra, sizeof(g_si_ctx.text.info_extra), "%s",
                ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_PLACE_BANKNOTES));
        } else {
            if (g_si_ctx.text.idle_has_issue) {
                lv_snprintf(detail_line, sizeof(detail_line),
                    ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_RESULT_ISSUE_DETAIL_FMT),
                    current_suspect,
                    current_damaged);
                lv_snprintf(g_si_ctx.text.info_extra, sizeof(g_si_ctx.text.info_extra), "%s", detail_line);
            } else {
                lv_snprintf(g_si_ctx.text.info_extra, sizeof(g_si_ctx.text.info_extra), "%s",
                    ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_RESULT_OK_DETAIL));
            }
        }

        if (current_total > 0) {
            g_si_ctx.text.idle_quality_percent = (uint8_t)((current_valid * 100) / current_total);
        } else {
            g_si_ctx.text.idle_quality_percent = 0;
        }
        break;
    }
    }

    smart_island_apply_texts();
}

static void smart_island_modal_update(void)
{
    if (g_si_ctx.objects.modal == NULL || !lv_obj_is_valid(g_si_ctx.objects.modal)) return;
    if (g_si_ctx.view.visual == SMART_ISLAND_VISUAL_EXPANDED) {
        lv_obj_clear_flag(g_si_ctx.objects.modal, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_si_ctx.objects.modal);
        lv_obj_move_foreground(g_si_ctx.objects.root);
    } else {
        lv_obj_add_flag(g_si_ctx.objects.modal, LV_OBJ_FLAG_HIDDEN);
    }
}

void smart_island_update_pages_visible(void)
{
    if (g_si_ctx.objects.page_root && lv_obj_is_valid(g_si_ctx.objects.page_root)) {
        if (g_si_ctx.view.visual == SMART_ISLAND_VISUAL_EXPANDED) {
            lv_obj_clear_flag(g_si_ctx.objects.page_root, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_si_ctx.objects.page_root, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (g_si_ctx.objects.page_info && lv_obj_is_valid(g_si_ctx.objects.page_info)) {
        if (g_si_ctx.view.page == SMART_ISLAND_PAGE_INFO) {
            lv_obj_clear_flag(g_si_ctx.objects.page_info, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_si_ctx.objects.page_info, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (g_si_ctx.objects.page_action && lv_obj_is_valid(g_si_ctx.objects.page_action)) {
        if (g_si_ctx.view.page == SMART_ISLAND_PAGE_ACTION) {
            lv_obj_clear_flag(g_si_ctx.objects.page_action, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_si_ctx.objects.page_action, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (g_si_ctx.objects.page_indicator && lv_obj_is_valid(g_si_ctx.objects.page_indicator)) {
        if (g_si_ctx.view.visual == SMART_ISLAND_VISUAL_EXPANDED) {
            lv_obj_clear_flag(g_si_ctx.objects.page_indicator, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_si_ctx.objects.page_indicator, LV_OBJ_FLAG_HIDDEN);
        }
    }

    smart_island_apply_quality_indicator();
}

static void smart_island_apply_scene_style(void)
{
    uint32_t bg_hex = SMART_ISLAND_BG_IDLE;
    lv_color_t title_color = lv_color_hex(SMART_ISLAND_TEXT_LIGHT);
    lv_color_t dot_color = lv_color_hex(SMART_ISLAND_READY_DOT);
    bool show_time = true;
    bool show_dot = true;
    bool show_counting = false;

    if (g_si_ctx.objects.root == NULL || !lv_obj_is_valid(g_si_ctx.objects.root)) return;

    switch (g_si_ctx.view.scene) {
    case SMART_ISLAND_SCENE_IDLE:
    case SMART_ISLAND_SCENE_QR:
        bg_hex = SMART_ISLAND_BG_IDLE;
        title_color = lv_color_hex(SMART_ISLAND_TEXT_LIGHT);
        dot_color = lv_color_hex(SMART_ISLAND_READY_DOT);
        show_time = (g_si_ctx.view.scene == SMART_ISLAND_SCENE_IDLE);
        show_dot = true;
        break;

    case SMART_ISLAND_SCENE_RESULT:
        bg_hex = SMART_ISLAND_BG_IDLE;
        title_color = lv_color_hex(SMART_ISLAND_TEXT_LIGHT);
        dot_color = lv_color_hex(SMART_ISLAND_RESULT_OK_COLOR);
        show_time = false;
        show_dot = true;
        break;

    case SMART_ISLAND_SCENE_COUNTING:
        bg_hex = SMART_ISLAND_BG_COUNTING;
        title_color = lv_color_hex(SMART_ISLAND_TEXT_LIGHT);
        dot_color = lv_color_hex(SMART_ISLAND_DOT_NON_IDLE);
        show_time = false;
        show_dot = false;
        show_counting =
            g_si_ctx.view.visual != SMART_ISLAND_VISUAL_EXPANDED;
        show_dot = !show_counting;
        break;
    case SMART_ISLAND_SCENE_WARNING:
        bg_hex =
            (g_si_ctx.warning.level == SMART_ISLAND_WARNING_LEVEL_ERROR)
            ? SMART_ISLAND_BG_ERROR
            : SMART_ISLAND_BG_WARNING;
        title_color = lv_color_hex(SMART_ISLAND_TEXT_LIGHT);
        dot_color = lv_color_hex(SMART_ISLAND_DOT_NON_IDLE);
        show_time = false;
        show_dot = true;
        break;

    case SMART_ISLAND_SCENE_UPDATE:
        bg_hex = SMART_ISLAND_BG_UPDATE;
        title_color = lv_color_hex(SMART_ISLAND_TEXT_LIGHT);
        dot_color = lv_color_hex(SMART_ISLAND_DOT_NON_IDLE);
        show_time = false;
        show_dot = true;
        break;
    default: break;
    }

    /* 功能页仍不显示时间，头部通过滑动动画进出 */
    if (g_si_ctx.view.visual == SMART_ISLAND_VISUAL_EXPANDED &&
        g_si_ctx.view.page == SMART_ISLAND_PAGE_ACTION) {
        show_time = false;
    }

    smart_island_bg_color_apply_anim(bg_hex);
    lv_obj_set_style_border_width(g_si_ctx.objects.root, 0, 0);
    lv_obj_set_style_outline_width(g_si_ctx.objects.root, 0, 0);
    lv_obj_set_style_shadow_width(g_si_ctx.objects.root, 0, 0);

    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title)) {
        lv_obj_set_style_text_color(g_si_ctx.objects.title, title_color, 0);
        lv_obj_set_style_text_opa(g_si_ctx.objects.title, LV_OPA_COVER, 0);
        if (show_counting) {
            lv_obj_add_flag(g_si_ctx.objects.title, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(g_si_ctx.objects.title, LV_OBJ_FLAG_HIDDEN);
        }
        if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_COUNTING &&
            (machine_state_mode() == MODE_MDC || machine_state_mode() == MODE_SDC) &&
            g_si_ctx.text.serial_ticker[0] != '\0') {
            lv_label_set_long_mode(g_si_ctx.objects.title, LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_width(g_si_ctx.objects.title, SMART_ISLAND_W - 52);
        } else if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_RESULT) {
            /* RESULT uses the wider counting shell.  Keeping the idle 150 px
             * title width clips the trailing 'D' in FINISHED. */
            lv_label_set_long_mode(g_si_ctx.objects.title, LV_LABEL_LONG_CLIP);
            lv_obj_set_width(g_si_ctx.objects.title,
                             SMART_ISLAND_COUNT_W - 52);
        } else {
            lv_label_set_long_mode(g_si_ctx.objects.title, LV_LABEL_LONG_CLIP);
            lv_obj_set_width(g_si_ctx.objects.title, 150);
        }
    }

    if (g_si_ctx.objects.subtitle && lv_obj_is_valid(g_si_ctx.objects.subtitle)) {
        lv_obj_add_flag(g_si_ctx.objects.subtitle, LV_OBJ_FLAG_HIDDEN);
    }

    if (g_si_ctx.objects.expand_title && lv_obj_is_valid(g_si_ctx.objects.expand_title)) {
        lv_obj_set_style_text_opa(g_si_ctx.objects.expand_title, LV_OPA_COVER, 0);
        lv_obj_clear_flag(g_si_ctx.objects.expand_title, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_si_ctx.objects.expand_subtitle && lv_obj_is_valid(g_si_ctx.objects.expand_subtitle)) {
        lv_obj_set_style_text_opa(g_si_ctx.objects.expand_subtitle, LV_OPA_COVER, 0);
        lv_obj_clear_flag(g_si_ctx.objects.expand_subtitle, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_si_ctx.objects.expand_last && lv_obj_is_valid(g_si_ctx.objects.expand_last)) {
        if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_IDLE) {
            lv_obj_clear_flag(g_si_ctx.objects.expand_last, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_si_ctx.objects.expand_last, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (g_si_ctx.objects.expand_divider && lv_obj_is_valid(g_si_ctx.objects.expand_divider)) {
        if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_IDLE) {
            lv_obj_clear_flag(g_si_ctx.objects.expand_divider, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_si_ctx.objects.expand_divider, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (g_si_ctx.objects.expand_footer && lv_obj_is_valid(g_si_ctx.objects.expand_footer)) {
        if (g_si_ctx.text.info_footer[0] != '\0') lv_obj_clear_flag(g_si_ctx.objects.expand_footer, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(g_si_ctx.objects.expand_footer, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_si_ctx.objects.expand_extra && lv_obj_is_valid(g_si_ctx.objects.expand_extra)) {
        if (g_si_ctx.text.info_extra[0] != '\0') lv_obj_clear_flag(g_si_ctx.objects.expand_extra, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(g_si_ctx.objects.expand_extra, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_recolor(g_si_ctx.objects.expand_extra, true);
    }
    if (g_si_ctx.objects.expand_subtitle && lv_obj_is_valid(g_si_ctx.objects.expand_subtitle)) {
        lv_label_set_recolor(g_si_ctx.objects.expand_subtitle, true);
    }

    if (g_si_ctx.objects.expand_subtitle && lv_obj_is_valid(g_si_ctx.objects.expand_subtitle)) {
        if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_IDLE) {
            lv_obj_set_style_text_color(g_si_ctx.objects.expand_subtitle, lv_color_hex(SMART_ISLAND_TEXT_LIGHT), 0);
            lv_obj_set_style_text_align(g_si_ctx.objects.expand_subtitle, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_set_width(g_si_ctx.objects.expand_subtitle, 173);
            lv_obj_set_pos(g_si_ctx.objects.expand_subtitle, 68, 36);
        } else {
            lv_obj_set_style_text_color(g_si_ctx.objects.expand_subtitle, title_color, 0);
            lv_obj_set_style_text_align(g_si_ctx.objects.expand_subtitle, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(g_si_ctx.objects.expand_subtitle, SMART_ISLAND_W - 20);
            lv_obj_set_pos(g_si_ctx.objects.expand_subtitle, 20, 42);
        }
    }

    if (g_si_ctx.objects.expand_last && lv_obj_is_valid(g_si_ctx.objects.expand_last)) {
        lv_obj_set_style_text_color(g_si_ctx.objects.expand_last, lv_color_hex(SMART_ISLAND_RESULT_DETAIL_GRAY), 0);
    }

    if (g_si_ctx.objects.expand_divider && lv_obj_is_valid(g_si_ctx.objects.expand_divider)) {
        lv_obj_set_style_bg_color(g_si_ctx.objects.expand_divider, lv_color_hex(0x515151), 0);
        lv_obj_set_style_bg_opa(g_si_ctx.objects.expand_divider, LV_OPA_COVER, 0);
    }

    if (g_si_ctx.objects.expand_title && lv_obj_is_valid(g_si_ctx.objects.expand_title)) {
        lv_obj_set_pos(g_si_ctx.objects.expand_title, 20, 18);
    }

    if (g_si_ctx.objects.expand_footer && lv_obj_is_valid(g_si_ctx.objects.expand_footer)) {
        lv_obj_set_pos(g_si_ctx.objects.expand_footer, 20, 62);
    }

    if (g_si_ctx.objects.expand_extra && lv_obj_is_valid(g_si_ctx.objects.expand_extra)) {
        lv_obj_set_pos(g_si_ctx.objects.expand_extra, 20, 80);
    }

    if (g_si_ctx.objects.quality_bar_bg && lv_obj_is_valid(g_si_ctx.objects.quality_bar_bg)) {
        lv_obj_set_pos(g_si_ctx.objects.quality_bar_bg, 166, 64);
    }
    if (g_si_ctx.objects.quality_percent && lv_obj_is_valid(g_si_ctx.objects.quality_percent)) {
        lv_obj_set_pos(g_si_ctx.objects.quality_percent, 228, 62);
    }
    if (g_si_ctx.objects.expand_last && lv_obj_is_valid(g_si_ctx.objects.expand_last)) {
        lv_obj_set_pos(g_si_ctx.objects.expand_last, 20, 36);
    }
    if (g_si_ctx.objects.expand_divider && lv_obj_is_valid(g_si_ctx.objects.expand_divider)) {
        lv_obj_set_pos(g_si_ctx.objects.expand_divider, 20, 56);
    }

    if (g_si_ctx.objects.expand_title && lv_obj_is_valid(g_si_ctx.objects.expand_title)) {
        if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_IDLE) {
            lv_obj_add_flag(g_si_ctx.objects.expand_title, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(g_si_ctx.objects.expand_title, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (g_si_ctx.objects.expand_footer && lv_obj_is_valid(g_si_ctx.objects.expand_footer)) {
        if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_IDLE) {
            lv_color_t footer_color;
            if (g_si_ctx.text.idle_no_count) {
                footer_color = lv_color_hex(SMART_ISLAND_TEXT_SUB);
            } else if (g_si_ctx.text.idle_has_issue) {
                footer_color = lv_color_hex(SMART_ISLAND_RESULT_ISSUE_TITLE_COLOR);
            } else {
                footer_color = lv_color_hex(SMART_ISLAND_RESULT_OK_COLOR);
            }
            lv_obj_set_style_text_color(g_si_ctx.objects.expand_footer, footer_color, 0);
        } else {
            lv_obj_set_style_text_color(g_si_ctx.objects.expand_footer, lv_color_hex(SMART_ISLAND_TEXT_SUB), 0);
        }
    }

    if (g_si_ctx.objects.expand_extra && lv_obj_is_valid(g_si_ctx.objects.expand_extra)) {
        if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_IDLE) {
            lv_color_t extra_color = lv_color_hex(g_si_ctx.text.idle_has_issue
                ? SMART_ISLAND_RESULT_ISSUE_COLOR
                : (g_si_ctx.text.idle_no_count ? SMART_ISLAND_LAST_TEXT_GRAY : SMART_ISLAND_RESULT_DETAIL_GRAY));
            lv_obj_set_style_text_color(g_si_ctx.objects.expand_extra, extra_color, 0);
        } else {
            lv_obj_set_style_text_color(g_si_ctx.objects.expand_extra, lv_color_hex(SMART_ISLAND_TEXT_SUB), 0);
        }
    }

    if (g_si_ctx.objects.dot && lv_obj_is_valid(g_si_ctx.objects.dot)) {
        lv_obj_set_style_bg_color(g_si_ctx.objects.dot, dot_color, 0);
        if (show_dot) lv_obj_clear_flag(g_si_ctx.objects.dot, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(g_si_ctx.objects.dot, LV_OBJ_FLAG_HIDDEN);
    }

    smart_island_counting_apply_visibility();
    if (show_time) {
        smart_island_update_idle_time();
        lv_obj_clear_flag(g_si_ctx.objects.time, LV_OBJ_FLAG_HIDDEN);
        smart_island_reset_time_position();
    } else {
        lv_obj_add_flag(g_si_ctx.objects.time, LV_OBJ_FLAG_HIDDEN);
    }

    smart_island_apply_progress();

    smart_island_apply_quality_indicator();
    smart_island_update_pages_visible();
    smart_island_modal_update();
}

static void smart_island_visual_apply_now(smart_island_visual_t visual)
{
    lv_coord_t x = SMART_ISLAND_X;
    lv_coord_t y = SMART_ISLAND_Y;
    lv_coord_t w = SMART_ISLAND_W;
    lv_coord_t h = SMART_ISLAND_COMPACT_H;

    if (g_si_ctx.objects.root == NULL || !lv_obj_is_valid(g_si_ctx.objects.root)) return;

    if (visual == SMART_ISLAND_VISUAL_MINI) {
        w = SMART_ISLAND_MINI_W;
        x = SMART_ISLAND_X + (SMART_ISLAND_W - w) / 2;
    } else if (visual == SMART_ISLAND_VISUAL_EXPANDED) {
        y = SMART_ISLAND_Y - (SMART_ISLAND_ACTION_EXPAND_H - SMART_ISLAND_COMPACT_H);
        h = SMART_ISLAND_ACTION_EXPAND_H;
    } else if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_COUNTING ||
               g_si_ctx.view.scene == SMART_ISLAND_SCENE_RESULT) {
        w = SMART_ISLAND_COUNT_W;
        h = SMART_ISLAND_COUNT_H;
        x = SMART_ISLAND_X - (w - SMART_ISLAND_W) / 2;
        y = SMART_ISLAND_Y - (h - SMART_ISLAND_COMPACT_H) / 2;
    }

    lv_obj_set_pos(g_si_ctx.objects.root, x, y);
    lv_obj_set_size(g_si_ctx.objects.root, w, h);
    lv_obj_set_style_radius(g_si_ctx.objects.root,
                            h == SMART_ISLAND_COUNT_H ? h / 2 : SMART_ISLAND_RADIUS,
                            0);

    smart_island_reset_time_position();

    smart_island_update_pages_visible();
    smart_island_modal_update();
}

static void smart_island_visual_apply_anim(smart_island_visual_t visual)
{
    lv_anim_t a;
    lv_coord_t dst_x = SMART_ISLAND_X;
    lv_coord_t dst_y = SMART_ISLAND_Y;
    lv_coord_t dst_w = SMART_ISLAND_W;
    lv_coord_t dst_h = SMART_ISLAND_COMPACT_H;
    uint32_t anim_time = SMART_ISLAND_EXPAND_TIME;

    if (g_si_ctx.objects.root == NULL || !lv_obj_is_valid(g_si_ctx.objects.root)) return;

    if (visual == SMART_ISLAND_VISUAL_MINI) {
        anim_time = SMART_ISLAND_COLLAPSE_TIME;
        dst_w = SMART_ISLAND_MINI_W;
        dst_x = SMART_ISLAND_X + (SMART_ISLAND_W - dst_w) / 2;
    } else if (visual == SMART_ISLAND_VISUAL_EXPANDED) {
        dst_y = SMART_ISLAND_Y - (SMART_ISLAND_ACTION_EXPAND_H - SMART_ISLAND_COMPACT_H);
        dst_h = SMART_ISLAND_ACTION_EXPAND_H;
    } else if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_COUNTING ||
               g_si_ctx.view.scene == SMART_ISLAND_SCENE_RESULT) {
        dst_w = SMART_ISLAND_COUNT_W;
        dst_h = SMART_ISLAND_COUNT_H;
        dst_x = SMART_ISLAND_X - (dst_w - SMART_ISLAND_W) / 2;
        dst_y = SMART_ISLAND_Y - (dst_h - SMART_ISLAND_COMPACT_H) / 2;
    }

    g_si_ctx.view.anim_running = true;

    lv_anim_del(g_si_ctx.objects.root, smart_island_anim_x_cb);
    lv_anim_del(g_si_ctx.objects.root, smart_island_anim_y_cb);
    lv_anim_del(g_si_ctx.objects.root, smart_island_anim_w_cb);
    lv_anim_del(g_si_ctx.objects.root, smart_island_anim_h_cb);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_si_ctx.objects.root);
    lv_anim_set_exec_cb(&a, smart_island_anim_x_cb);
    lv_anim_set_values(&a, lv_obj_get_x(g_si_ctx.objects.root), dst_x);
    lv_anim_set_time(&a, anim_time);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, smart_island_anim_w_cb);
    lv_anim_set_values(&a, lv_obj_get_width(g_si_ctx.objects.root), dst_w);
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, smart_island_anim_y_cb);
    lv_anim_set_values(&a, lv_obj_get_y(g_si_ctx.objects.root), dst_y);
    lv_anim_set_time(&a, anim_time);
    lv_anim_set_path_cb(&a, (visual == SMART_ISLAND_VISUAL_EXPANDED) ? lv_anim_path_overshoot : lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, smart_island_anim_h_cb);
    lv_anim_set_values(&a, lv_obj_get_height(g_si_ctx.objects.root), dst_h);
    lv_anim_set_ready_cb(&a, smart_island_anim_finish_cb);
    lv_anim_start(&a);

    lv_obj_set_style_radius(g_si_ctx.objects.root,
                            dst_h == SMART_ISLAND_COUNT_H ? dst_h / 2 : SMART_ISLAND_RADIUS,
                            0);

    smart_island_reset_time_position();

    smart_island_update_pages_visible();
    smart_island_modal_update();
}

static void smart_island_pulse_stop(void)
{
    if (g_si_ctx.objects.root == NULL || !lv_obj_is_valid(g_si_ctx.objects.root)) return;
    lv_anim_del(g_si_ctx.objects.root, smart_island_anim_zoom_cb);
    lv_anim_del(g_si_ctx.objects.root, smart_island_anim_stretch_cb);
    lv_obj_set_style_transform_zoom(g_si_ctx.objects.root, 256, 0);
    lv_obj_set_style_transform_width(g_si_ctx.objects.root, 0, 0);
    if (g_si_ctx.objects.dot && lv_obj_is_valid(g_si_ctx.objects.dot)) {
        lv_anim_del(g_si_ctx.objects.dot, smart_island_anim_translate_x_cb);
        lv_obj_set_style_translate_x(g_si_ctx.objects.dot, 0, 0);
    }
    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title)) {
        lv_anim_del(g_si_ctx.objects.title, smart_island_anim_translate_x_cb);
        lv_obj_set_style_translate_x(g_si_ctx.objects.title, 0, 0);
    }
    g_si_ctx.view.message_pulse_running = false;
}

void smart_island_view_message_pulse(void)
{
    lv_anim_t animation;
    int32_t stretch;

    if (g_si_ctx.lifecycle.suspended ||
        g_si_ctx.view.visual == SMART_ISLAND_VISUAL_EXPANDED ||
        g_si_ctx.objects.root == NULL || !lv_obj_is_valid(g_si_ctx.objects.root)) {
        return;
    }

    stretch = (g_si_ctx.view.scene == SMART_ISLAND_SCENE_COUNTING ||
               g_si_ctx.view.scene == SMART_ISLAND_SCENE_RESULT) ? 16 : 22;
    smart_island_pulse_stop();

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, g_si_ctx.objects.root);
    lv_anim_set_exec_cb(&animation, smart_island_anim_stretch_cb);
    lv_anim_set_values(&animation, 0, stretch);
    lv_anim_set_time(&animation, SMART_ISLAND_MESSAGE_PULSE_TIME);
    lv_anim_set_playback_time(&animation, SMART_ISLAND_MESSAGE_PULSE_TIME + 40U);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&animation, smart_island_message_pulse_finish_cb);
    lv_anim_start(&animation);
    g_si_ctx.view.message_pulse_running = true;

    /* The capsule and its message move as one composition: the status dot
     * eases toward the new left edge while text opens toward the right. */
    if (g_si_ctx.objects.dot && lv_obj_is_valid(g_si_ctx.objects.dot) &&
        !lv_obj_has_flag(g_si_ctx.objects.dot, LV_OBJ_FLAG_HIDDEN)) {
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.dot);
        lv_anim_set_exec_cb(&animation, smart_island_anim_translate_x_cb);
        lv_anim_set_values(&animation, 0, -8);
        lv_anim_set_time(&animation, SMART_ISLAND_MESSAGE_PULSE_TIME);
        lv_anim_set_playback_time(&animation,
                                  SMART_ISLAND_MESSAGE_PULSE_TIME + 40U);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title) &&
        !lv_obj_has_flag(g_si_ctx.objects.title, LV_OBJ_FLAG_HIDDEN)) {
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.title);
        lv_anim_set_exec_cb(&animation, smart_island_anim_translate_x_cb);
        lv_anim_set_values(&animation, 0, 8);
        lv_anim_set_time(&animation, SMART_ISLAND_MESSAGE_PULSE_TIME);
        lv_anim_set_playback_time(&animation,
                                  SMART_ISLAND_MESSAGE_PULSE_TIME + 40U);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
}

void smart_island_view_notice_reset(void)
{
    smart_island_pulse_stop();
}

void smart_island_view_notice_expand(void)
{
    lv_anim_t animation;

    if (g_si_ctx.lifecycle.suspended ||
        g_si_ctx.view.visual == SMART_ISLAND_VISUAL_EXPANDED ||
        g_si_ctx.objects.root == NULL ||
        !lv_obj_is_valid(g_si_ctx.objects.root)) {
        return;
    }

    smart_island_pulse_stop();

    /* A warning is a held presentation, not a pulse.  Expand once and leave
     * the shell plus its content at the end positions until notice expiry. */
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, g_si_ctx.objects.root);
    lv_anim_set_exec_cb(&animation, smart_island_anim_stretch_cb);
    lv_anim_set_values(&animation, 0, 22);
    lv_anim_set_time(&animation, 260);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);

    if (g_si_ctx.objects.dot && lv_obj_is_valid(g_si_ctx.objects.dot) &&
        !lv_obj_has_flag(g_si_ctx.objects.dot, LV_OBJ_FLAG_HIDDEN)) {
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.dot);
        lv_anim_set_exec_cb(&animation, smart_island_anim_translate_x_cb);
        lv_anim_set_values(&animation, 0, -8);
        lv_anim_set_time(&animation, 260);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title) &&
        !lv_obj_has_flag(g_si_ctx.objects.title, LV_OBJ_FLAG_HIDDEN)) {
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.title);
        lv_anim_set_exec_cb(&animation, smart_island_anim_translate_x_cb);
        lv_anim_set_values(&animation, 0, 8);
        lv_anim_set_time(&animation, 260);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
}

void smart_island_view_notice_collapse(lv_anim_ready_cb_t ready_cb)
{
    lv_anim_t animation;
    lv_coord_t stretch;
    lv_coord_t dot_tx = 0;
    lv_coord_t title_tx = 0;

    if (g_si_ctx.lifecycle.suspended ||
        g_si_ctx.objects.root == NULL ||
        !lv_obj_is_valid(g_si_ctx.objects.root)) {
        if (ready_cb) ready_cb(NULL);
        return;
    }

    stretch = lv_obj_get_style_transform_width(g_si_ctx.objects.root,
                                                LV_PART_MAIN);
    lv_anim_del(g_si_ctx.objects.root, smart_island_anim_stretch_cb);

    if (g_si_ctx.objects.dot && lv_obj_is_valid(g_si_ctx.objects.dot)) {
        dot_tx = lv_obj_get_style_translate_x(g_si_ctx.objects.dot,
                                              LV_PART_MAIN);
        lv_anim_del(g_si_ctx.objects.dot, smart_island_anim_translate_x_cb);
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.dot);
        lv_anim_set_exec_cb(&animation, smart_island_anim_translate_x_cb);
        lv_anim_set_values(&animation, dot_tx, 0);
        lv_anim_set_time(&animation, 260);
        /* Reverse of notice_expand's ease_out curve: the same motion played
         * backward, so the dot and the shell settle with identical timing. */
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_in);
        lv_anim_start(&animation);
    }
    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title)) {
        title_tx = lv_obj_get_style_translate_x(g_si_ctx.objects.title,
                                                LV_PART_MAIN);
        lv_anim_del(g_si_ctx.objects.title, smart_island_anim_translate_x_cb);
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.title);
        lv_anim_set_exec_cb(&animation, smart_island_anim_translate_x_cb);
        lv_anim_set_values(&animation, title_tx, 0);
        lv_anim_set_time(&animation, 260);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_in);
        lv_anim_start(&animation);
    }

    /* Start the callback-bearing shell animation last.  Scene restoration is
     * therefore deferred until the visible content has also converged. */
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, g_si_ctx.objects.root);
    lv_anim_set_exec_cb(&animation, smart_island_anim_stretch_cb);
    lv_anim_set_values(&animation, stretch, 0);
    lv_anim_set_time(&animation, 260);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&animation, ready_cb);
    lv_anim_start(&animation);
}

/* A cached island can be moved from the main page to another live page (PURE
 * currently does this).  Page switching may happen while collapse/page-slide
 * animations are still running.  Their ready callbacks are not a reliable
 * lifecycle boundary after reparenting, so settle the component explicitly
 * before it is attached to the new page. */
static void smart_island_prepare_reparent(void)
{
    lv_obj_t *animated_objects[] = {
        g_si_ctx.objects.root,
        g_si_ctx.objects.page_info,
        g_si_ctx.objects.page_action,
        g_si_ctx.objects.action_track,
        g_si_ctx.objects.title,
        g_si_ctx.objects.dot,
        g_si_ctx.objects.time,
        g_si_ctx.objects.counting_bill,
        g_si_ctx.objects.counting_value,
        g_si_ctx.objects.counting_value_next,
    };
    size_t i;

    for (i = 0; i < sizeof(animated_objects) / sizeof(animated_objects[0]); i++) {
        if (animated_objects[i] && lv_obj_is_valid(animated_objects[i])) {
            lv_anim_del(animated_objects[i], NULL);
        }
    }

    g_si_ctx.view.anim_running = false;
    g_si_ctx.view.bg_anim_running = false;
    g_si_ctx.view.message_pulse_running = false;
    g_si_ctx.counting.gate_anim_running = false;
    g_si_ctx.view.page_slide_dir = 0;
    g_si_ctx.view.swipe.pressed = false;
    g_si_ctx.view.swipe.swiped = false;
    g_si_ctx.action.ignore_click_once = false;
    g_si_ctx.action.ignore_action_click_once = false;
}

void smart_island_create(lv_obj_t *parent)
{
    if (parent == NULL || !lv_obj_is_valid(parent)) return;

    if (g_si_ctx.lifecycle.created && g_si_ctx.objects.root && lv_obj_is_valid(g_si_ctx.objects.root)) {
        lv_obj_t *cur_parent = lv_obj_get_parent(g_si_ctx.objects.root);
        if (cur_parent != parent) {
            smart_island_prepare_reparent();
            if (g_si_ctx.objects.modal && lv_obj_is_valid(g_si_ctx.objects.modal)) {
                lv_obj_set_parent(g_si_ctx.objects.modal, parent);
                lv_obj_set_pos(g_si_ctx.objects.modal, 0, 0);
            }
            lv_obj_set_parent(g_si_ctx.objects.root, parent);
            lv_obj_move_foreground(g_si_ctx.objects.root);

            /* create(parent) means that this instance is active on parent.
             * Do not inherit the cached main page's suspended state. */
            g_si_ctx.lifecycle.suspended = false;
            smart_island_visual_apply_now(g_si_ctx.view.visual);
            smart_island_reset_page_positions();
            if (g_si_ctx.lifecycle.dirty) {
                smart_island_rebuild_scene_texts();
                smart_island_apply_scene_style();
                g_si_ctx.lifecycle.dirty = false;
            }
            smart_island_modal_update();
        }
        return;
    }

    if (g_si_ctx.lifecycle.created) {
        smart_island_destroy();
    }

    memset(&g_si_ctx.view.content, 0, sizeof(g_si_ctx.view.content));
    memset(g_si_ctx.warning.text, 0, sizeof(g_si_ctx.warning.text));
    memset(g_si_ctx.text.result, 0, sizeof(g_si_ctx.text.result));
    memset(g_si_ctx.text.info_extra, 0, sizeof(g_si_ctx.text.info_extra));
    memset(g_si_ctx.text.idle_line1, 0, sizeof(g_si_ctx.text.idle_line1));
    memset(g_si_ctx.text.idle_line2, 0, sizeof(g_si_ctx.text.idle_line2));
    memset(g_si_ctx.text.idle_line3, 0, sizeof(g_si_ctx.text.idle_line3));
    memset(&g_si_ctx.counting, 0, sizeof(g_si_ctx.counting));
    g_si_ctx.counting.mode = machine_state_mode();

    g_si_ctx.objects.modal = lv_obj_create(parent);
    lv_obj_remove_style_all(g_si_ctx.objects.modal);
    lv_obj_set_size(g_si_ctx.objects.modal, 1280, 400);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.modal, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(g_si_ctx.objects.modal, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_si_ctx.objects.modal, smart_island_modal_click_cb, LV_EVENT_CLICKED, NULL);

    g_si_ctx.objects.root = lv_obj_create(parent);
    lv_obj_remove_style_all(g_si_ctx.objects.root);
    lv_obj_set_pos(g_si_ctx.objects.root, SMART_ISLAND_X, SMART_ISLAND_Y);
    lv_obj_set_size(g_si_ctx.objects.root, SMART_ISLAND_W, SMART_ISLAND_COMPACT_H);
    lv_obj_set_style_radius(g_si_ctx.objects.root, SMART_ISLAND_RADIUS, 0);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.root, lv_color_hex(SMART_ISLAND_BG_IDLE), 0);
    lv_obj_set_style_border_width(g_si_ctx.objects.root, 0, 0);
    lv_obj_set_style_outline_width(g_si_ctx.objects.root, 0, 0);
    lv_obj_set_style_shadow_width(g_si_ctx.objects.root, 0, 0);
    smart_island_enable_gesture_on_obj(g_si_ctx.objects.root);
    lv_obj_add_event_cb(g_si_ctx.objects.root, smart_island_click_cb, LV_EVENT_CLICKED, NULL);

    g_si_ctx.objects.dot = lv_obj_create(g_si_ctx.objects.root);
    lv_obj_remove_style_all(g_si_ctx.objects.dot);
    lv_obj_set_size(g_si_ctx.objects.dot, 8, 8);
    lv_obj_set_pos(g_si_ctx.objects.dot, 20, 18);
    lv_obj_set_style_radius(g_si_ctx.objects.dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.dot, LV_OPA_COVER, 0);

    g_si_ctx.objects.title = lv_label_create(g_si_ctx.objects.root);
    lv_label_set_text(g_si_ctx.objects.title, ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_READY_TITLE));
    lv_obj_set_width(g_si_ctx.objects.title, SMART_ISLAND_W - 36 - 14);
    lv_obj_set_pos(g_si_ctx.objects.title, 36, 13);
    lv_obj_set_style_text_font(g_si_ctx.objects.title, &lv_font_instrument_sans_semibold_14, 0);

    g_si_ctx.objects.subtitle = lv_label_create(g_si_ctx.objects.root);
    lv_obj_add_flag(g_si_ctx.objects.subtitle, LV_OBJ_FLAG_HIDDEN);

    g_si_ctx.objects.time = lv_label_create(g_si_ctx.objects.root);
    lv_label_set_text(g_si_ctx.objects.time, "00:00:00");
    lv_obj_set_style_text_font(g_si_ctx.objects.time, &lv_font_instrument_sans_medium_18, 0);
    lv_obj_set_style_text_color(g_si_ctx.objects.time, lv_color_hex(SMART_ISLAND_TEXT_LIGHT), 0);
    lv_obj_align(g_si_ctx.objects.time, LV_ALIGN_RIGHT_MID, -14, 0);
    lv_obj_clear_flag(g_si_ctx.objects.time, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);

    g_si_ctx.objects.badge = lv_obj_create(g_si_ctx.objects.root);
    lv_obj_remove_style_all(g_si_ctx.objects.badge);
    lv_obj_add_flag(g_si_ctx.objects.badge, LV_OBJ_FLAG_HIDDEN);

    g_si_ctx.objects.progress = lv_bar_create(g_si_ctx.objects.root);
    lv_obj_set_size(g_si_ctx.objects.progress, 160, 4);
    lv_obj_align(g_si_ctx.objects.progress, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(g_si_ctx.objects.progress, lv_color_hex(0x2E2E2E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_si_ctx.objects.progress, lv_color_hex(0x00E676), LV_PART_INDICATOR);
    lv_obj_add_flag(g_si_ctx.objects.progress, LV_OBJ_FLAG_HIDDEN);

    /* Compact running presentation.  It is deliberately independent from the
     * expanded info/action pages so those pages keep their existing content,
     * pagination and button behavior. */
    g_si_ctx.objects.counting_root = lv_obj_create(g_si_ctx.objects.root);
    lv_obj_remove_style_all(g_si_ctx.objects.counting_root);
    lv_obj_set_pos(g_si_ctx.objects.counting_root, 0, 0);
    lv_obj_set_size(g_si_ctx.objects.counting_root,
                    SMART_ISLAND_COUNT_W, SMART_ISLAND_COUNT_H);
    lv_obj_clear_flag(g_si_ctx.objects.counting_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_si_ctx.objects.counting_root, LV_OBJ_FLAG_HIDDEN);

    g_si_ctx.objects.counting_gate = lv_obj_create(g_si_ctx.objects.counting_root);
    lv_obj_remove_style_all(g_si_ctx.objects.counting_gate);
    lv_obj_set_pos(g_si_ctx.objects.counting_gate, 12, 5);
    lv_obj_set_size(g_si_ctx.objects.counting_gate, 46, 34);
    lv_obj_clear_flag(g_si_ctx.objects.counting_gate, LV_OBJ_FLAG_SCROLLABLE);

    g_si_ctx.objects.counting_gate_left = lv_obj_create(g_si_ctx.objects.counting_gate);
    lv_obj_remove_style_all(g_si_ctx.objects.counting_gate_left);
    lv_obj_set_pos(g_si_ctx.objects.counting_gate_left, 4, 3);
    lv_obj_set_size(g_si_ctx.objects.counting_gate_left, 2, 28);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.counting_gate_left, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.counting_gate_left, lv_color_white(), 0);

    g_si_ctx.objects.counting_gate_right = lv_obj_create(g_si_ctx.objects.counting_gate);
    lv_obj_remove_style_all(g_si_ctx.objects.counting_gate_right);
    lv_obj_set_pos(g_si_ctx.objects.counting_gate_right, 36, 3);
    lv_obj_set_size(g_si_ctx.objects.counting_gate_right, 2, 28);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.counting_gate_right, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.counting_gate_right, lv_color_white(), 0);

    g_si_ctx.objects.counting_sensor = lv_obj_create(g_si_ctx.objects.counting_gate);
    lv_obj_remove_style_all(g_si_ctx.objects.counting_sensor);
    lv_obj_set_pos(g_si_ctx.objects.counting_sensor, 8, 8);
    lv_obj_set_size(g_si_ctx.objects.counting_sensor, 2, 18);
    lv_obj_set_style_bg_color(g_si_ctx.objects.counting_sensor,
                              lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.counting_sensor, LV_OPA_30, 0);

    g_si_ctx.objects.counting_bill = lv_obj_create(g_si_ctx.objects.counting_gate);
    lv_obj_remove_style_all(g_si_ctx.objects.counting_bill);
    lv_obj_set_pos(g_si_ctx.objects.counting_bill, -16, 8);
    lv_obj_set_size(g_si_ctx.objects.counting_bill, 15, 1);
    lv_obj_set_style_radius(g_si_ctx.objects.counting_bill, 1, 0);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.counting_bill, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.counting_bill, lv_color_white(), 0);

    g_si_ctx.objects.counting_bill_mid = lv_obj_create(g_si_ctx.objects.counting_gate);
    lv_obj_remove_style_all(g_si_ctx.objects.counting_bill_mid);
    lv_obj_set_pos(g_si_ctx.objects.counting_bill_mid, -16, 16);
    lv_obj_set_size(g_si_ctx.objects.counting_bill_mid, 10, 1);
    lv_obj_set_style_radius(g_si_ctx.objects.counting_bill_mid, 1, 0);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.counting_bill_mid, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.counting_bill_mid,
                              lv_color_white(), 0);

    g_si_ctx.objects.counting_bill_low = lv_obj_create(g_si_ctx.objects.counting_gate);
    lv_obj_remove_style_all(g_si_ctx.objects.counting_bill_low);
    lv_obj_set_pos(g_si_ctx.objects.counting_bill_low, -16, 24);
    lv_obj_set_size(g_si_ctx.objects.counting_bill_low, 16, 1);
    lv_obj_set_style_radius(g_si_ctx.objects.counting_bill_low, 1, 0);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.counting_bill_low, LV_OPA_60, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.counting_bill_low,
                              lv_color_white(), 0);

    g_si_ctx.objects.counting_value = lv_label_create(g_si_ctx.objects.counting_root);
    lv_label_set_text(g_si_ctx.objects.counting_value, "0");
    lv_obj_set_pos(g_si_ctx.objects.counting_value, 69, 10);
    lv_obj_set_width(g_si_ctx.objects.counting_value, 58);
    lv_label_set_long_mode(g_si_ctx.objects.counting_value, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(g_si_ctx.objects.counting_value,
                               &lv_font_instrument_sans_semibold_20, 0);
    lv_obj_set_style_text_color(g_si_ctx.objects.counting_value,
                                lv_color_white(), 0);

    g_si_ctx.objects.counting_value_next = lv_label_create(g_si_ctx.objects.counting_root);
    lv_label_set_text(g_si_ctx.objects.counting_value_next, "0");
    lv_obj_set_pos(g_si_ctx.objects.counting_value_next, 69, SMART_ISLAND_COUNT_H);
    lv_obj_set_width(g_si_ctx.objects.counting_value_next, 58);
    lv_label_set_long_mode(g_si_ctx.objects.counting_value_next, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(g_si_ctx.objects.counting_value_next,
                               &lv_font_instrument_sans_semibold_20, 0);
    lv_obj_set_style_text_color(g_si_ctx.objects.counting_value_next,
                                lv_color_white(), 0);
    lv_obj_add_flag(g_si_ctx.objects.counting_value_next, LV_OBJ_FLAG_HIDDEN);

    /* Shared empty-detail treatment: denomination uses the same scan language
     * as serial, but its shorter field keeps the two placeholders distinct. */
    g_si_ctx.objects.counting_value_track = lv_obj_create(g_si_ctx.objects.counting_root);
    lv_obj_remove_style_all(g_si_ctx.objects.counting_value_track);
    lv_obj_set_pos(g_si_ctx.objects.counting_value_track, 83, 21);
    lv_obj_set_size(g_si_ctx.objects.counting_value_track, 34, 2);
    lv_obj_set_style_radius(g_si_ctx.objects.counting_value_track, 1, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.counting_value_track,
                              lv_color_hex(0x303238), 0);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.counting_value_track, LV_OPA_COVER, 0);
    lv_obj_add_flag(g_si_ctx.objects.counting_value_track, LV_OBJ_FLAG_HIDDEN);

    g_si_ctx.objects.counting_value_scan = lv_obj_create(g_si_ctx.objects.counting_value_track);
    lv_obj_remove_style_all(g_si_ctx.objects.counting_value_scan);
    lv_obj_set_pos(g_si_ctx.objects.counting_value_scan, 0, 0);
    lv_obj_set_size(g_si_ctx.objects.counting_value_scan, 10, 2);
    lv_obj_set_style_radius(g_si_ctx.objects.counting_value_scan, 1, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.counting_value_scan,
                              lv_color_hex(0x8E9299), 0);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.counting_value_scan, LV_OPA_COVER, 0);

    g_si_ctx.objects.counting_unit = lv_label_create(g_si_ctx.objects.counting_root);
    lv_label_set_text(g_si_ctx.objects.counting_unit, "PCS");
    lv_obj_set_pos(g_si_ctx.objects.counting_unit, 192, 16);
    lv_obj_set_style_text_font(g_si_ctx.objects.counting_unit,
                               &lv_font_instrument_sans_medium_14, 0);
    lv_obj_set_style_text_color(g_si_ctx.objects.counting_unit,
                                lv_color_hex(0xA3A3A3), 0);

    g_si_ctx.objects.counting_divider = lv_obj_create(g_si_ctx.objects.counting_root);
    lv_obj_remove_style_all(g_si_ctx.objects.counting_divider);
    lv_obj_set_pos(g_si_ctx.objects.counting_divider, 134, 12);
    lv_obj_set_size(g_si_ctx.objects.counting_divider, 1, 20);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.counting_divider, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.counting_divider, lv_color_white(), 0);

    g_si_ctx.objects.counting_serial = lv_label_create(g_si_ctx.objects.counting_root);
    lv_label_set_text(g_si_ctx.objects.counting_serial, "");
    lv_obj_set_pos(g_si_ctx.objects.counting_serial, 142, 12);
    lv_obj_set_width(g_si_ctx.objects.counting_serial, 154);
    lv_label_set_long_mode(g_si_ctx.objects.counting_serial, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(g_si_ctx.objects.counting_serial, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(g_si_ctx.objects.counting_serial,
                               &lv_font_instrument_sans_medium_16, 0);
    lv_obj_set_style_text_color(g_si_ctx.objects.counting_serial,
                                lv_color_white(), 0);

    g_si_ctx.objects.counting_serial_track = lv_obj_create(g_si_ctx.objects.counting_root);
    lv_obj_remove_style_all(g_si_ctx.objects.counting_serial_track);
    lv_obj_set_pos(g_si_ctx.objects.counting_serial_track, 178, 21);
    lv_obj_set_size(g_si_ctx.objects.counting_serial_track, 70, 2);
    lv_obj_set_style_radius(g_si_ctx.objects.counting_serial_track, 1, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.counting_serial_track,
                              lv_color_hex(0x303238), 0);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.counting_serial_track, LV_OPA_COVER, 0);

    g_si_ctx.objects.counting_serial_scan = lv_obj_create(g_si_ctx.objects.counting_serial_track);
    lv_obj_remove_style_all(g_si_ctx.objects.counting_serial_scan);
    lv_obj_set_pos(g_si_ctx.objects.counting_serial_scan, 0, 0);
    lv_obj_set_size(g_si_ctx.objects.counting_serial_scan, 16, 2);
    lv_obj_set_style_radius(g_si_ctx.objects.counting_serial_scan, 1, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.counting_serial_scan,
                              lv_color_hex(0x8E9299), 0);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.counting_serial_scan, LV_OPA_COVER, 0);

    g_si_ctx.objects.page_root = lv_obj_create(g_si_ctx.objects.root);
    lv_obj_remove_style_all(g_si_ctx.objects.page_root);
    lv_obj_set_size(g_si_ctx.objects.page_root, SMART_ISLAND_W, SMART_ISLAND_ACTION_EXPAND_H);
    smart_island_enable_gesture_on_obj(g_si_ctx.objects.page_root);
    lv_obj_add_flag(g_si_ctx.objects.page_root, LV_OBJ_FLAG_HIDDEN);

    g_si_ctx.objects.page_info = lv_obj_create(g_si_ctx.objects.page_root);
    lv_obj_remove_style_all(g_si_ctx.objects.page_info);
    lv_obj_set_size(g_si_ctx.objects.page_info, SMART_ISLAND_W, SMART_ISLAND_ACTION_EXPAND_H);
    smart_island_enable_gesture_on_obj(g_si_ctx.objects.page_info);

    g_si_ctx.objects.expand_title = lv_label_create(g_si_ctx.objects.page_info);
    lv_label_set_text(g_si_ctx.objects.expand_title, ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_EXPAND_TITLE));
    lv_label_set_long_mode(g_si_ctx.objects.expand_title, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(g_si_ctx.objects.expand_title, SMART_ISLAND_W - 24 - 12);
    lv_obj_set_pos(g_si_ctx.objects.expand_title, 20, 18);
    lv_obj_set_style_text_font(g_si_ctx.objects.expand_title, &lv_font_instrument_sans_semibold_12, 0);
    lv_obj_set_style_text_color(g_si_ctx.objects.expand_title, lv_color_hex(SMART_ISLAND_TEXT_SUB), 0);
    lv_obj_add_flag(g_si_ctx.objects.expand_title, LV_OBJ_FLAG_HIDDEN);

    g_si_ctx.objects.expand_subtitle = lv_label_create(g_si_ctx.objects.page_info);
    lv_label_set_text(g_si_ctx.objects.expand_subtitle, ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_EXPAND_SUBTITLE));
    lv_label_set_long_mode(g_si_ctx.objects.expand_subtitle, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(g_si_ctx.objects.expand_subtitle, 173);
    lv_obj_set_pos(g_si_ctx.objects.expand_subtitle, 68, 36);
    lv_obj_set_style_text_font(g_si_ctx.objects.expand_subtitle, &lv_font_instrument_sans_semibold_12, 0);
    lv_obj_set_style_text_color(g_si_ctx.objects.expand_subtitle, lv_color_hex(SMART_ISLAND_TEXT_LIGHT), 0);
    lv_obj_set_style_text_align(g_si_ctx.objects.expand_subtitle, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_recolor(g_si_ctx.objects.expand_subtitle, true);

    g_si_ctx.objects.expand_last = lv_label_create(g_si_ctx.objects.page_info);
    lv_label_set_text(g_si_ctx.objects.expand_last, ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_LAST_TAG));
    lv_label_set_long_mode(g_si_ctx.objects.expand_last, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(g_si_ctx.objects.expand_last, 40);
    lv_obj_set_pos(g_si_ctx.objects.expand_last, 20, 36);
    lv_obj_set_style_text_align(g_si_ctx.objects.expand_last, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(g_si_ctx.objects.expand_last, &lv_font_instrument_sans_medium_12, 0);
    lv_obj_set_style_text_color(g_si_ctx.objects.expand_last, lv_color_hex(SMART_ISLAND_LAST_TEXT_GRAY), 0);

    g_si_ctx.objects.expand_divider = lv_obj_create(g_si_ctx.objects.page_info);
    lv_obj_remove_style_all(g_si_ctx.objects.expand_divider);
    lv_obj_set_size(g_si_ctx.objects.expand_divider, SMART_ISLAND_W - 40, 1);
    lv_obj_set_pos(g_si_ctx.objects.expand_divider, 20, 56);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.expand_divider, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.expand_divider, lv_color_hex(0x515151), 0);

    g_si_ctx.objects.expand_footer = lv_label_create(g_si_ctx.objects.page_info);
    lv_label_set_text(g_si_ctx.objects.expand_footer, "");
    lv_label_set_long_mode(g_si_ctx.objects.expand_footer, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(g_si_ctx.objects.expand_footer, 150);
    lv_obj_set_pos(g_si_ctx.objects.expand_footer, 20, 62);
    lv_obj_set_style_text_font(g_si_ctx.objects.expand_footer, &lv_font_instrument_sans_medium_12, 0);
    lv_obj_set_style_text_color(g_si_ctx.objects.expand_footer, lv_color_hex(SMART_ISLAND_TEXT_SUB), 0);
    lv_obj_add_flag(g_si_ctx.objects.expand_footer, LV_OBJ_FLAG_HIDDEN);

    g_si_ctx.objects.expand_extra = lv_label_create(g_si_ctx.objects.page_info);
    lv_label_set_text(g_si_ctx.objects.expand_extra, "");
    lv_label_set_long_mode(g_si_ctx.objects.expand_extra, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(g_si_ctx.objects.expand_extra, 150);
    lv_obj_set_pos(g_si_ctx.objects.expand_extra, 20, 80);
    lv_obj_set_style_text_font(g_si_ctx.objects.expand_extra, &lv_font_instrument_sans_medium_12, 0);
    lv_obj_set_style_text_color(g_si_ctx.objects.expand_extra, lv_color_hex(SMART_ISLAND_TEXT_SUB), 0);
    lv_obj_add_flag(g_si_ctx.objects.expand_extra, LV_OBJ_FLAG_HIDDEN);

    g_si_ctx.objects.quality_bar_bg = lv_obj_create(g_si_ctx.objects.page_info);
    lv_obj_remove_style_all(g_si_ctx.objects.quality_bar_bg);
    lv_obj_set_size(g_si_ctx.objects.quality_bar_bg, 56, 8);
    lv_obj_set_pos(g_si_ctx.objects.quality_bar_bg, 166, 64);
    lv_obj_set_style_radius(g_si_ctx.objects.quality_bar_bg, 4, 0);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.quality_bar_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.quality_bar_bg, lv_color_hex(SMART_ISLAND_RESULT_ISSUE_COLOR), 0);
    lv_obj_add_flag(g_si_ctx.objects.quality_bar_bg, LV_OBJ_FLAG_HIDDEN);

    g_si_ctx.objects.quality_bar_fg = lv_obj_create(g_si_ctx.objects.quality_bar_bg);
    lv_obj_remove_style_all(g_si_ctx.objects.quality_bar_fg);
    lv_obj_set_size(g_si_ctx.objects.quality_bar_fg, 56, 8);
    lv_obj_set_pos(g_si_ctx.objects.quality_bar_fg, 0, 0);
    lv_obj_set_style_radius(g_si_ctx.objects.quality_bar_fg, 4, 0);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.quality_bar_fg, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_si_ctx.objects.quality_bar_fg, lv_color_hex(SMART_ISLAND_RESULT_OK_COLOR), 0);

    g_si_ctx.objects.quality_percent = lv_label_create(g_si_ctx.objects.page_info);
    lv_label_set_text(g_si_ctx.objects.quality_percent, "100%");
    lv_obj_set_pos(g_si_ctx.objects.quality_percent, 228, 62);
    lv_obj_set_style_text_font(g_si_ctx.objects.quality_percent, &lv_font_instrument_sans_medium_12, 0);
    lv_obj_set_style_text_color(g_si_ctx.objects.quality_percent, lv_color_hex(SMART_ISLAND_RESULT_OK_COLOR), 0);
    lv_obj_add_flag(g_si_ctx.objects.quality_percent, LV_OBJ_FLAG_HIDDEN);

    g_si_ctx.objects.page_action = lv_obj_create(g_si_ctx.objects.page_root);
    lv_obj_remove_style_all(g_si_ctx.objects.page_action);
    lv_obj_set_size(g_si_ctx.objects.page_action, SMART_ISLAND_W, SMART_ISLAND_ACTION_EXPAND_H);
    smart_island_enable_gesture_on_obj(g_si_ctx.objects.page_action);
    lv_obj_add_flag(g_si_ctx.objects.page_action, LV_OBJ_FLAG_HIDDEN);

    g_si_ctx.objects.page_indicator = lv_capsule_pagination_create(g_si_ctx.objects.root);
    if (g_si_ctx.objects.page_indicator && lv_obj_is_valid(g_si_ctx.objects.page_indicator)) {
        lv_obj_align(g_si_ctx.objects.page_indicator, LV_ALIGN_BOTTOM_MID, 0, SMART_ISLAND_PAGE_INDICATOR_Y);
        lv_obj_add_flag(g_si_ctx.objects.page_indicator, LV_OBJ_FLAG_HIDDEN);
    }
    smart_island_action_btn_create();

    g_si_ctx.view.scene = SMART_ISLAND_SCENE_IDLE;
    g_si_ctx.view.visual = SMART_ISLAND_VISUAL_COMPACT;
    g_si_ctx.view.page = SMART_ISLAND_PAGE_INFO;
    g_si_ctx.text.idle_quality_percent = 100;
    g_si_ctx.text.idle_has_issue = false;
    g_si_ctx.text.idle_has_data = false;
    g_si_ctx.text.idle_no_count = true;
    g_si_ctx.lifecycle.count_session_active = false;
    g_si_ctx.view.bg_current = SMART_ISLAND_BG_IDLE;
    g_si_ctx.view.bg_from = SMART_ISLAND_BG_IDLE;
    g_si_ctx.view.bg_to = SMART_ISLAND_BG_IDLE;
    g_si_ctx.view.bg_anim_running = false;
    g_si_ctx.lifecycle.created = true;
    smart_island_page_indicator_sync(false);
    smart_island_rebuild_scene_texts();
    smart_island_apply_scene_style();
    smart_island_update_idle_time();
    smart_island_update_pages_visible();
    smart_island_modal_update();
}

void smart_island_view_destroy_objects(void)
{
    smart_island_pulse_stop();
    smart_island_counting_anim_stop();
    if (g_si_ctx.objects.root && lv_obj_is_valid(g_si_ctx.objects.root)) {
        lv_obj_del(g_si_ctx.objects.root);
    }
    if (g_si_ctx.objects.modal && lv_obj_is_valid(g_si_ctx.objects.modal)) {
        lv_obj_del(g_si_ctx.objects.modal);
    }
    smart_island_clear_object_refs();
}

void smart_island_view_apply_visual(smart_island_visual_t visual, bool anim_en)
{
    if (g_si_ctx.lifecycle.suspended) {
        g_si_ctx.lifecycle.dirty = true;
        return;
    }
    if (anim_en) {
        smart_island_visual_apply_anim(visual);
    } else {
        smart_island_visual_apply_now(visual);
    }
    smart_island_update_pages_visible();
    smart_island_modal_update();
    smart_island_apply_scene_style();
}

void smart_island_view_refresh_scene(void)
{
    if (g_si_ctx.lifecycle.suspended) {
        g_si_ctx.lifecycle.dirty = true;
        return;
    }
    smart_island_rebuild_scene_texts();
    smart_island_apply_scene_style();
}

void smart_island_view_update_counting(void)
{
    if (g_si_ctx.lifecycle.suspended) {
        g_si_ctx.lifecycle.dirty = true;
        return;
    }
    if (g_si_ctx.view.scene != SMART_ISLAND_SCENE_COUNTING) {
        return;
    }

    smart_island_counting_apply_visibility();
}

void smart_island_view_result_collapse(void)
{
    lv_anim_t animation;
    lv_obj_t *root = g_si_ctx.objects.root;

    if (g_si_ctx.lifecycle.suspended || root == NULL || !lv_obj_is_valid(root)) {
        smart_island_restore_idle();
        return;
    }

    lv_anim_del(root, smart_island_anim_x_cb);
    lv_anim_del(root, smart_island_anim_w_cb);
    if (g_si_ctx.objects.dot && lv_obj_is_valid(g_si_ctx.objects.dot)) {
        lv_anim_del(g_si_ctx.objects.dot, smart_island_anim_x_cb);
        lv_anim_del(g_si_ctx.objects.dot, smart_island_dot_opa_anim_cb);
        /* The root itself moves right by half the width delta.  Keeping the
         * child at x=16 makes this very same dot land at READY's screen
         * coordinate; an extra child translation creates a visible duplicate. */
        lv_obj_set_x(g_si_ctx.objects.dot, 16);
        lv_obj_set_style_bg_opa(g_si_ctx.objects.dot, LV_OPA_COVER, 0);
    }
    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title)) {
        lv_anim_del(g_si_ctx.objects.title, smart_island_anim_x_cb);
        lv_anim_del(g_si_ctx.objects.title, smart_island_text_opa_anim_cb);
    }
    g_si_ctx.view.anim_running = true;

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, root);
    lv_anim_set_exec_cb(&animation, smart_island_anim_x_cb);
    lv_anim_set_values(&animation, lv_obj_get_x(root), SMART_ISLAND_X);
    lv_anim_set_time(&animation, 300);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);

    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title)) {
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.title);
        lv_anim_set_exec_cb(&animation, smart_island_text_opa_anim_cb);
        lv_anim_set_values(&animation, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_time(&animation, 220);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_in);
        lv_anim_start(&animation);
    }

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, root);
    lv_anim_set_exec_cb(&animation, smart_island_anim_w_cb);
    lv_anim_set_values(&animation, lv_obj_get_width(root), SMART_ISLAND_W);
    lv_anim_set_time(&animation, 300);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&animation, smart_island_result_collapse_finish_cb);
    lv_anim_start(&animation);
}

void smart_island_view_result_enter(void)
{
    lv_anim_t animation;

    if (g_si_ctx.lifecycle.suspended ||
        g_si_ctx.objects.dot == NULL ||
        !lv_obj_is_valid(g_si_ctx.objects.dot) ||
        g_si_ctx.objects.title == NULL ||
        !lv_obj_is_valid(g_si_ctx.objects.title)) {
        return;
    }

    lv_anim_del(g_si_ctx.objects.dot, smart_island_anim_x_cb);
    lv_anim_del(g_si_ctx.objects.dot, smart_island_dot_opa_anim_cb);
    lv_anim_del(g_si_ctx.objects.title, smart_island_anim_x_cb);
    lv_anim_del(g_si_ctx.objects.title, smart_island_text_opa_anim_cb);

    /* Both content halves emerge from the visual centre while the 310 px
     * shell stays still: dot travels left, result text travels right. */
    lv_obj_set_x(g_si_ctx.objects.dot, 30);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.dot, LV_OPA_TRANSP, 0);
    lv_obj_set_x(g_si_ctx.objects.title, 22);
    lv_obj_set_style_text_opa(g_si_ctx.objects.title, LV_OPA_TRANSP, 0);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, g_si_ctx.objects.dot);
    lv_anim_set_exec_cb(&animation, smart_island_anim_x_cb);
    lv_anim_set_values(&animation, 30, 16);
    lv_anim_set_time(&animation, 240);
    lv_anim_set_path_cb(&animation, lv_anim_path_overshoot);
    lv_anim_start(&animation);

    lv_anim_set_exec_cb(&animation, smart_island_dot_opa_anim_cb);
    lv_anim_set_values(&animation, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, g_si_ctx.objects.title);
    lv_anim_set_exec_cb(&animation, smart_island_anim_x_cb);
    lv_anim_set_values(&animation, 22, 36);
    lv_anim_set_time(&animation, 240);
    lv_anim_set_path_cb(&animation, lv_anim_path_overshoot);
    lv_anim_start(&animation);

    lv_anim_set_exec_cb(&animation, smart_island_text_opa_anim_cb);
    lv_anim_set_values(&animation, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

void smart_island_view_idle_enter(void)
{
    lv_anim_t animation;

    if (g_si_ctx.lifecycle.suspended ||
        g_si_ctx.view.scene != SMART_ISLAND_SCENE_IDLE) {
        return;
    }

    /* Result text is already transparent when the geometry settles.  Apply
     * READY atomically, then reveal its label and clock without a hard frame. */
    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title)) {
        lv_anim_del(g_si_ctx.objects.title, smart_island_text_opa_anim_cb);
        lv_obj_set_style_text_opa(g_si_ctx.objects.title, LV_OPA_TRANSP, 0);
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.title);
        lv_anim_set_exec_cb(&animation, smart_island_text_opa_anim_cb);
        lv_anim_set_values(&animation, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_time(&animation, 180);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
    if (g_si_ctx.objects.time && lv_obj_is_valid(g_si_ctx.objects.time)) {
        lv_anim_del(g_si_ctx.objects.time, smart_island_text_opa_anim_cb);
        lv_obj_set_style_text_opa(g_si_ctx.objects.time, LV_OPA_TRANSP, 0);
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.time);
        lv_anim_set_exec_cb(&animation, smart_island_text_opa_anim_cb);
        lv_anim_set_values(&animation, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_time(&animation, 180);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
}

void smart_island_view_set_idle_line(char *dst,
                                     size_t dst_size,
                                     const char *text)
{
    smart_island_apply_idle_line_text(dst, dst_size, text);
}
