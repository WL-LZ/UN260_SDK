#include "un260/lv_components/smart_island/smart_island_internal.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_system/ui_text.h"
#include "lvgl/src/misc/lv_txt.h"
#include <string.h>

#define SMART_ISLAND_WARNING_MARQUEE_TIME   1680U
#define SMART_ISLAND_WARNING_MARQUEE_CYCLES 2U
#define SMART_ISLAND_WARNING_FLASH_TIME     1000U

static void smart_island_warning_apply_static_layout(void);
static void smart_island_warning_marquee_start(void);
static void smart_island_warning_marquee_run_step(void);

static void smart_island_warning_anim_x_cb(void *var, int32_t value)
{
    lv_obj_set_x((lv_obj_t *)var, (lv_coord_t)value);
}

static void smart_island_warning_anim_text_opa_cb(void *var, int32_t value)
{
    lv_obj_set_style_text_opa((lv_obj_t *)var, (lv_opa_t)value, 0);
}

static void smart_island_warning_fault_capture(void)
{
    fault_source_t source;
    uint8_t fault_type;
    uint8_t code;

    if (fault_popup_get_pending_fault(&source, &fault_type, &code)) {
        g_si_ctx.warning.fault.valid = true;
        g_si_ctx.warning.fault.source = source;
        g_si_ctx.warning.fault.fault_type = fault_type;
        g_si_ctx.warning.fault.code = code;
    } else {
        smart_island_warning_fault_clear();
    }
}

bool smart_island_warning_fault_show(void)
{
    if (!g_si_ctx.warning.fault.valid) {
        return false;
    }

    switch (g_si_ctx.warning.fault.source) {
    case FAULT_SRC_START_COUNT:
        show_start_fault_popup(g_si_ctx.warning.fault.fault_type,
                               g_si_ctx.warning.fault.code);
        return true;
    case FAULT_SRC_RUNTIME:
        show_runtime_fault_popup(g_si_ctx.warning.fault.code);
        return true;
    case FAULT_SRC_BOOT:
    default:
        return false;
    }
}

void smart_island_warning_fault_clear(void)
{
    g_si_ctx.warning.fault.valid = false;
    g_si_ctx.warning.fault.source = FAULT_SRC_START_COUNT;
    g_si_ctx.warning.fault.fault_type = 0;
    g_si_ctx.warning.fault.code = 0;
}

static bool smart_island_warning_pocket_confirm(void)
{
    uint8_t clear_cmd = 0x01;
    uint8_t code;

    if (!g_si_ctx.warning.fault.valid ||
        g_si_ctx.warning.fault.source != FAULT_SRC_START_COUNT) {
        return false;
    }

    code = g_si_ctx.warning.fault.code;
    if (code != 0x05 && code != 0x07 && code != 0x08) {
        return false;
    }

    protocol_send(0x3D, &clear_cmd, 1);
    return true;
}

void smart_island_warning_stop(void)
{
    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title)) {
        lv_anim_del(g_si_ctx.objects.title, smart_island_warning_anim_x_cb);
        lv_anim_del(g_si_ctx.objects.title, smart_island_warning_anim_text_opa_cb);
        lv_obj_set_style_text_opa(g_si_ctx.objects.title, LV_OPA_COVER, 0);
    }
    if (g_si_ctx.objects.expand_title && lv_obj_is_valid(g_si_ctx.objects.expand_title)) {
        lv_anim_del(g_si_ctx.objects.expand_title, smart_island_warning_anim_x_cb);
        lv_anim_del(g_si_ctx.objects.expand_title, smart_island_warning_anim_text_opa_cb);
        lv_obj_set_style_text_opa(g_si_ctx.objects.expand_title, LV_OPA_COVER, 0);
    }

    g_si_ctx.warning.marquee_running = false;
    g_si_ctx.warning.marquee_step = 0;
    g_si_ctx.warning.text_width_compact = 0;
    g_si_ctx.warning.text_width_expand = 0;
    smart_island_reset_compact_header_position();
    smart_island_warning_apply_static_layout();
}

static void smart_island_warning_apply_static_layout(void)
{
    lv_coord_t compact_visible = SMART_ISLAND_WIDTH - 36 - 14;
    lv_coord_t expand_visible = SMART_ISLAND_WIDTH - 32 - 12;

    if (g_si_ctx.view.scene != SMART_ISLAND_SCENE_WARNING) {
        return;
    }

    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title)) {
        lv_label_set_long_mode(g_si_ctx.objects.title, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(g_si_ctx.objects.title, compact_visible);
        lv_obj_set_x(g_si_ctx.objects.title, 36);
        lv_obj_set_y(g_si_ctx.objects.title, 13);
    }

    if (g_si_ctx.objects.expand_title && lv_obj_is_valid(g_si_ctx.objects.expand_title)) {
        lv_label_set_long_mode(g_si_ctx.objects.expand_title, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(g_si_ctx.objects.expand_title, expand_visible);
        lv_obj_set_x(g_si_ctx.objects.expand_title, 20);
        lv_obj_set_y(g_si_ctx.objects.expand_title, 18);
    }
}

static void smart_island_warning_marquee_finish_cb(lv_anim_t *animation)
{
    LV_UNUSED(animation);
    if (!g_si_ctx.warning.marquee_running) {
        return;
    }

    g_si_ctx.warning.marquee_step++;
    smart_island_warning_marquee_run_step();
}

static void smart_island_warning_flash_finish_cb(lv_anim_t *animation)
{
    bool pocket_confirmed;

    LV_UNUSED(animation);
    smart_island_warning_stop();
    pocket_confirmed = smart_island_warning_pocket_confirm();
    if (!fault_popup_is_showing()) {
        smart_island_restore_idle();
        if (!pocket_confirmed) {
            fault_popup_schedule_auto_confirm();
        }
    }
}

static void smart_island_warning_marquee_start(void)
{
    lv_anim_t animation;
    lv_coord_t text_width;
    lv_coord_t expand_text_width;
    const char *title_text;
    const char *expand_text;
    const lv_font_t *title_font;
    const lv_font_t *expand_font;
    lv_coord_t compact_visible = SMART_ISLAND_WIDTH - 36 - 14;
    lv_coord_t expand_visible = SMART_ISLAND_WIDTH - 32 - 12;

    if (g_si_ctx.objects.title == NULL || !lv_obj_is_valid(g_si_ctx.objects.title)) {
        return;
    }
    if (g_si_ctx.view.scene != SMART_ISLAND_SCENE_WARNING) {
        return;
    }

    smart_island_warning_pocket_confirm();
    smart_island_warning_stop();
    title_text = lv_label_get_text(g_si_ctx.objects.title);
    title_font = lv_obj_get_style_text_font(g_si_ctx.objects.title, LV_PART_MAIN);
    text_width = (lv_coord_t)lv_txt_get_width(
        title_text ? title_text : "",
        (uint32_t)strlen(title_text ? title_text : ""),
        title_font,
        lv_obj_get_style_text_letter_space(g_si_ctx.objects.title, LV_PART_MAIN),
        LV_TEXT_FLAG_NONE);
    g_si_ctx.warning.text_width_compact = text_width;

    if (g_si_ctx.objects.expand_title && lv_obj_is_valid(g_si_ctx.objects.expand_title)) {
        expand_text = lv_label_get_text(g_si_ctx.objects.expand_title);
        expand_font = lv_obj_get_style_text_font(g_si_ctx.objects.expand_title, LV_PART_MAIN);
        expand_text_width = (lv_coord_t)lv_txt_get_width(
            expand_text ? expand_text : "",
            (uint32_t)strlen(expand_text ? expand_text : ""),
            expand_font,
            lv_obj_get_style_text_letter_space(g_si_ctx.objects.expand_title, LV_PART_MAIN),
            LV_TEXT_FLAG_NONE);
        g_si_ctx.warning.text_width_expand = expand_text_width;
    }

    if (g_si_ctx.warning.text_width_compact <= compact_visible) {
        lv_coord_t compact_center_x =
            (SMART_ISLAND_WIDTH - g_si_ctx.warning.text_width_compact) / 2;
        if (compact_center_x < 0) compact_center_x = 0;

        lv_label_set_long_mode(g_si_ctx.objects.title, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(g_si_ctx.objects.title, LV_SIZE_CONTENT);
        lv_obj_set_x(g_si_ctx.objects.title, compact_center_x);
        lv_obj_set_y(g_si_ctx.objects.title, 13);

        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.title);
        lv_anim_set_exec_cb(&animation, smart_island_warning_anim_text_opa_cb);
        lv_anim_set_values(&animation, LV_OPA_100, LV_OPA_40);
        lv_anim_set_time(&animation, SMART_ISLAND_WARNING_FLASH_TIME);
        lv_anim_set_playback_time(&animation, SMART_ISLAND_WARNING_FLASH_TIME);
        lv_anim_set_repeat_count(&animation, 3);
        lv_anim_set_path_cb(&animation, lv_anim_path_linear);
        lv_anim_set_ready_cb(&animation, smart_island_warning_flash_finish_cb);
        lv_anim_start(&animation);

        if (g_si_ctx.objects.expand_title && lv_obj_is_valid(g_si_ctx.objects.expand_title)) {
            lv_coord_t expand_center_x =
                (SMART_ISLAND_WIDTH - g_si_ctx.warning.text_width_expand) / 2;
            if (expand_center_x < 0) expand_center_x = 0;

            lv_label_set_long_mode(g_si_ctx.objects.expand_title, LV_LABEL_LONG_CLIP);
            lv_obj_set_width(g_si_ctx.objects.expand_title, LV_SIZE_CONTENT);
            lv_obj_set_x(g_si_ctx.objects.expand_title, expand_center_x);
            lv_obj_set_y(g_si_ctx.objects.expand_title, 30);

            lv_anim_init(&animation);
            lv_anim_set_var(&animation, g_si_ctx.objects.expand_title);
            lv_anim_set_exec_cb(&animation, smart_island_warning_anim_text_opa_cb);
            lv_anim_set_values(&animation, LV_OPA_100, LV_OPA_40);
            lv_anim_set_time(&animation, SMART_ISLAND_WARNING_FLASH_TIME);
            lv_anim_set_playback_time(&animation, SMART_ISLAND_WARNING_FLASH_TIME);
            lv_anim_set_repeat_count(&animation, 3);
            lv_anim_set_path_cb(&animation, lv_anim_path_linear);
            lv_anim_start(&animation);
        }

        g_si_ctx.warning.marquee_running = true;
        return;
    }

    g_si_ctx.warning.marquee_running = true;
    g_si_ctx.warning.marquee_step = 0;
    lv_label_set_long_mode(g_si_ctx.objects.title, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(g_si_ctx.objects.title, compact_visible);
    lv_obj_set_x(g_si_ctx.objects.title, 36);
    if (g_si_ctx.objects.expand_title && lv_obj_is_valid(g_si_ctx.objects.expand_title)) {
        lv_label_set_long_mode(g_si_ctx.objects.expand_title, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(g_si_ctx.objects.expand_title, expand_visible);
        lv_obj_set_x(g_si_ctx.objects.expand_title, 32);
    }

    smart_island_warning_marquee_run_step();
}

static void smart_island_warning_marquee_run_step(void)
{
    lv_anim_t animation;
    lv_coord_t compact_visible = SMART_ISLAND_WIDTH - 36 - 14;
    lv_coord_t expand_visible = SMART_ISLAND_WIDTH - 32 - 12;
    lv_coord_t compact_left = (lv_coord_t)(36 - g_si_ctx.warning.text_width_compact);
    lv_coord_t compact_right = (lv_coord_t)(36 + compact_visible);
    lv_coord_t expand_left = (lv_coord_t)(32 - g_si_ctx.warning.text_width_expand);
    lv_coord_t expand_right = (lv_coord_t)(32 + expand_visible);
    bool left_to_right;
    lv_coord_t from_x;
    lv_coord_t to_x;

    if (!g_si_ctx.warning.marquee_running) {
        return;
    }

    if (g_si_ctx.warning.marquee_step >= SMART_ISLAND_WARNING_MARQUEE_CYCLES * 2U) {
        bool pocket_confirmed;

        smart_island_warning_stop();
        pocket_confirmed = smart_island_warning_pocket_confirm();
        if (!fault_popup_is_showing()) {
            smart_island_restore_idle();
            if (!pocket_confirmed) {
                fault_popup_schedule_auto_confirm();
            }
        }
        return;
    }

    left_to_right = ((g_si_ctx.warning.marquee_step % 2U) == 0U);
    from_x = left_to_right ? compact_left : compact_right;
    to_x = left_to_right ? compact_right : compact_left;

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, g_si_ctx.objects.title);
    lv_anim_set_exec_cb(&animation, smart_island_warning_anim_x_cb);
    lv_anim_set_values(&animation, from_x, to_x);
    lv_anim_set_time(&animation, SMART_ISLAND_WARNING_MARQUEE_TIME);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_set_ready_cb(&animation, smart_island_warning_marquee_finish_cb);
    lv_anim_start(&animation);

    if (g_si_ctx.objects.expand_title && lv_obj_is_valid(g_si_ctx.objects.expand_title) &&
        g_si_ctx.warning.text_width_expand > expand_visible) {
        from_x = left_to_right ? expand_left : expand_right;
        to_x = left_to_right ? expand_right : expand_left;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, g_si_ctx.objects.expand_title);
        lv_anim_set_exec_cb(&animation, smart_island_warning_anim_x_cb);
        lv_anim_set_values(&animation, from_x, to_x);
        lv_anim_set_time(&animation, SMART_ISLAND_WARNING_MARQUEE_TIME);
        lv_anim_set_path_cb(&animation, lv_anim_path_linear);
        lv_anim_start(&animation);
    }
}

void smart_island_notify_warning_level(const char *warn_text,
                                       smart_island_warning_level_t level)
{
    char next_warning_text[sizeof(g_si_ctx.warning.text)];

    if ((unsigned int)level > (unsigned int)SMART_ISLAND_WARNING_LEVEL_ERROR) {
        return;
    }

    if (warn_text && warn_text[0] != '\0') {
        lv_snprintf(next_warning_text, sizeof(next_warning_text), "%s", warn_text);
    } else {
        lv_snprintf(next_warning_text, sizeof(next_warning_text), "%s",
                    ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_COUNT_ERROR));
    }

    /* 同一条异常重复上报时不重启动画，避免按键动作触发重复闪烁。 */
    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_WARNING &&
        g_si_ctx.warning.level == level &&
        strcmp(g_si_ctx.warning.text, next_warning_text) == 0) {
        if (g_si_ctx.lifecycle.suspended) {
            g_si_ctx.lifecycle.dirty = true;
            return;
        }
        if (!g_si_ctx.warning.marquee_running) {
            smart_island_warning_apply_static_layout();
            if (!fault_popup_is_showing()) {
                smart_island_warning_marquee_start();
            }
        }
        return;
    }

    g_si_ctx.warning.level = level;
    smart_island_warning_fault_capture();
    lv_snprintf(g_si_ctx.warning.text, sizeof(g_si_ctx.warning.text), "%s",
                next_warning_text);

    smart_island_set_scene(SMART_ISLAND_SCENE_WARNING, g_si_ctx.warning.text, NULL);
    if (g_si_ctx.lifecycle.suspended) return;
    g_si_ctx.view.page = SMART_ISLAND_PAGE_INFO;
    smart_island_set_visual(SMART_ISLAND_VISUAL_COMPACT, true);
    smart_island_reset_page_positions();
    smart_island_reset_compact_header_position();
    smart_island_reset_time_position();
    smart_island_warning_marquee_start();
}

void smart_island_notify_warning(const char *warn_text)
{
    smart_island_notify_warning_level(warn_text, SMART_ISLAND_WARNING_LEVEL_WARNING);
}
