#include "un260/lv_components/smart_island/smart_island_internal.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_text.h"
#include "lvgl/src/misc/lv_txt.h"
#include <string.h>

#define SMART_ISLAND_WARNING_SCROLL_SPEED  45U /* Pixels/second, independent of text length. */
#define SMART_ISLAND_WARNING_START_HOLD    1000U
#define SMART_ISLAND_WARNING_END_HOLD      1400U
#define SMART_ISLAND_WARNING_FLASH_TIME     1000U

static void smart_island_warning_apply_static_layout(void);
static void smart_island_warning_marquee_start(void);
static void smart_island_warning_finish_notice(void);
static void smart_island_warning_finish_commit(void);

static void smart_island_warning_anim_x_cb(void *var, int32_t value)
{
    lv_obj_set_x((lv_obj_t *)var, (lv_coord_t)value);
}

static void smart_island_warning_anim_text_opa_cb(void *var, int32_t value)
{
    lv_obj_set_style_text_opa((lv_obj_t *)var, (lv_opa_t)value, 0);
}

static smart_island_fault_phase_cb_t fault_phase_cb;
void smart_island_register_fault_phase_cb(smart_island_fault_phase_cb_t callback) { fault_phase_cb = callback; }
static machine_fault_key_t warning_key(void)
{
    return (machine_fault_key_t){g_si_ctx.warning.fault.source,
        g_si_ctx.warning.fault.fault_type,g_si_ctx.warning.fault.code};
}
static void fault_phase(smart_island_fault_phase_t phase)
{
    if (g_si_ctx.warning.fault.valid && fault_phase_cb) fault_phase_cb(warning_key(),phase);
}
void smart_island_faults_changed(void)
{
    if (g_si_ctx.warning.fault.valid && !machine_fault_find(warning_key(),NULL))
        smart_island_restore_idle();
}

bool smart_island_fault_hit_test(const lv_point_t *point)
{
    lv_obj_t *root=g_si_ctx.objects.root;
    if(!point||!g_si_ctx.warning.fault.valid||
       g_si_ctx.view.scene!=SMART_ISLAND_SCENE_WARNING||
       !root||!lv_obj_is_valid(root)||!lv_obj_is_visible(root))return false;
    lv_area_t area;lv_obj_get_coords(root,&area);
    return point->x>=area.x1&&point->x<=area.x2&&
           point->y>=area.y1&&point->y<=area.y2;
}

void smart_island_warning_fault_clear(void)
{
    g_si_ctx.warning.fault.valid = false;
    g_si_ctx.warning.fault.source = FAULT_SRC_START_COUNT;
    g_si_ctx.warning.fault.fault_type = 0;
    g_si_ctx.warning.fault.code = 0;
}

void smart_island_warning_stop(void)
{
    fault_phase(SMART_ISLAND_FAULT_CANCEL);
    smart_island_view_notice_reset();
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

    if (g_si_ctx.objects.title_clip) {
        lv_obj_set_x(g_si_ctx.objects.title_clip, 0);
        lv_obj_set_width(g_si_ctx.objects.title_clip, LV_PCT(100));
    }
    if (g_si_ctx.objects.expand_title_clip) {
        lv_obj_set_x(g_si_ctx.objects.expand_title_clip, 0);
        lv_obj_set_width(g_si_ctx.objects.expand_title_clip, LV_PCT(100));
    }
    g_si_ctx.warning.marquee_running = false;
    g_si_ctx.warning.collapse_running = false;
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

static void smart_island_warning_flash_finish_cb(lv_anim_t *animation)
{
    LV_UNUSED(animation);
    smart_island_warning_finish_notice();
}

static void smart_island_warning_collapse_finish_cb(lv_anim_t *animation)
{
    LV_UNUSED(animation);
    g_si_ctx.warning.collapse_running = false;
    smart_island_warning_finish_commit();
}

static void smart_island_warning_finish_notice(void)
{
    if (g_si_ctx.warning.collapse_running) {
        return;
    }

    g_si_ctx.warning.marquee_running = false;
    g_si_ctx.warning.collapse_running = true;
    smart_island_view_notice_collapse(
        smart_island_warning_collapse_finish_cb);
}

static void smart_island_warning_finish_commit(void)
{
    bool resume_counting = g_si_ctx.warning.resume_counting;

    smart_island_warning_stop();
    if (fault_popup_is_showing()) {
        return;
    }

    bool is_fault = g_si_ctx.warning.fault.valid;
    machine_fault_key_t key = warning_key();
    if (resume_counting && g_si_ctx.lifecycle.count_session_active) {
        g_si_ctx.warning.text[0] = '\0';
        g_si_ctx.warning.resume_counting = false;
        smart_island_warning_fault_clear();
        smart_island_set_scene(SMART_ISLAND_SCENE_COUNTING);
        smart_island_set_visual(SMART_ISLAND_VISUAL_COMPACT, true);
        smart_island_view_update_counting();
    } else {
        smart_island_restore_idle();
    }
    if (is_fault && fault_phase_cb) fault_phase_cb(key,SMART_ISLAND_FAULT_END);
}

static void smart_island_warning_scroll_finish_cb(lv_anim_t *animation)
{
    LV_UNUSED(animation);
    /* Keep the end of the sentence visible before returning to idle. */
    lv_anim_t hold;
    lv_anim_init(&hold);
    lv_anim_set_var(&hold, g_si_ctx.objects.title);
    lv_anim_set_exec_cb(&hold, smart_island_warning_anim_text_opa_cb);
    lv_anim_set_values(&hold, LV_OPA_COVER, LV_OPA_COVER);
    lv_anim_set_time(&hold, SMART_ISLAND_WARNING_END_HOLD);
    lv_anim_set_ready_cb(&hold, smart_island_warning_flash_finish_cb);
    if (!lv_anim_start(&hold)) smart_island_warning_finish_notice();
}

static lv_anim_t *smart_island_warning_scroll_label(lv_obj_t *label,
    lv_obj_t *clip, lv_coord_t left, lv_coord_t visible, lv_coord_t text_width)
{
    if (!label || !lv_obj_is_valid(label)) return NULL;
    /* Fixed viewport + full-width label: never clip the sentence before it
     * scrolls, and never move text across the status dot. One owner controls
     * this finite notice; LVGL's repeating label animation is not involved. */
    lv_obj_set_x(clip, left);
    lv_obj_set_width(clip, visible);
    lv_obj_set_width(label, text_width);
    lv_obj_set_x(label, 0);
    if (text_width <= visible) return NULL;
    lv_anim_t scroll;
    lv_anim_init(&scroll);
    lv_anim_set_var(&scroll, label);
    lv_anim_set_exec_cb(&scroll, smart_island_warning_anim_x_cb);
    lv_anim_set_values(&scroll, 0, visible - text_width);
    lv_anim_set_time(&scroll, lv_anim_speed_to_time(SMART_ISLAND_WARNING_SCROLL_SPEED,
                                                   0, text_width - visible));
    lv_anim_set_delay(&scroll, SMART_ISLAND_WARNING_START_HOLD);
    lv_anim_set_path_cb(&scroll, lv_anim_path_linear);
    return lv_anim_start(&scroll);
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

    smart_island_warning_stop();
    fault_phase(SMART_ISLAND_FAULT_BEGIN);
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
        lv_anim_set_values(&animation, LV_OPA_COVER, LV_OPA_40);
        uint32_t flash_time = g_si_ctx.warning.level == SMART_ISLAND_WARNING_LEVEL_PRESET ? 420U : SMART_ISLAND_WARNING_FLASH_TIME;
        lv_anim_set_time(&animation, flash_time);
        lv_anim_set_playback_time(&animation, flash_time);
        /* Preset uses three shorter pulses; other warnings use two flashes. */
        lv_anim_set_repeat_count(&animation, g_si_ctx.warning.level == SMART_ISLAND_WARNING_LEVEL_PRESET ? 2 : 1);
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
            lv_anim_set_values(&animation, LV_OPA_COVER, LV_OPA_40);
            lv_anim_set_time(&animation, SMART_ISLAND_WARNING_FLASH_TIME);
            lv_anim_set_playback_time(&animation, SMART_ISLAND_WARNING_FLASH_TIME);
            lv_anim_set_repeat_count(&animation, 1);
            lv_anim_set_path_cb(&animation, lv_anim_path_linear);
            lv_anim_start(&animation);
        }

        g_si_ctx.warning.marquee_running = true;
        return;
    }

    g_si_ctx.warning.marquee_running = true;
    lv_label_set_long_mode(g_si_ctx.objects.title, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(g_si_ctx.objects.title, compact_visible);
    lv_obj_set_x(g_si_ctx.objects.title, 36);
    if (g_si_ctx.objects.expand_title && lv_obj_is_valid(g_si_ctx.objects.expand_title)) {
        lv_label_set_long_mode(g_si_ctx.objects.expand_title, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(g_si_ctx.objects.expand_title, expand_visible);
        lv_obj_set_x(g_si_ctx.objects.expand_title, 32);
    }

    lv_anim_t *compact = smart_island_warning_scroll_label(g_si_ctx.objects.title,
        g_si_ctx.objects.title_clip, 36, compact_visible, g_si_ctx.warning.text_width_compact);
    lv_anim_t *expanded = smart_island_warning_scroll_label(g_si_ctx.objects.expand_title,
        g_si_ctx.objects.expand_title_clip, 32, expand_visible, g_si_ctx.warning.text_width_expand);
    lv_anim_t *last = compact;
    if (expanded && (!last || expanded->time > last->time)) last = expanded;
    if (last) lv_anim_set_ready_cb(last, smart_island_warning_scroll_finish_cb);
    else smart_island_warning_scroll_finish_cb(NULL);
}

static void notify_warning(const char *warn_text,
                           smart_island_warning_level_t level,
                           const machine_fault_key_t *key)
{
    char next_warning_text[sizeof(g_si_ctx.warning.text)];

    if ((unsigned int)level > (unsigned int)SMART_ISLAND_WARNING_LEVEL_PRESET) {
        return;
    }

    if (warn_text && warn_text[0] != '\0') {
        lv_snprintf(next_warning_text, sizeof(next_warning_text), "%s", ui_tr(warn_text));
    } else {
        lv_snprintf(next_warning_text, sizeof(next_warning_text), "%s",
                    ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_COUNT_ERROR));
    }

    /* 同一条异常重复上报时不重启动画，避免按键动作触发重复闪烁。 */
    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_WARNING &&
        g_si_ctx.warning.level == level &&
        g_si_ctx.warning.fault.valid == (key != NULL) &&
        (!key || machine_fault_key_equal(warning_key(),*key)) &&
        strcmp(g_si_ctx.warning.text, next_warning_text) == 0) {
        if (g_si_ctx.lifecycle.suspended) {
            g_si_ctx.lifecycle.dirty = true;
            return;
        }
        if (!g_si_ctx.warning.marquee_running && !g_si_ctx.warning.collapse_running) {
            smart_island_warning_apply_static_layout();
            if (!fault_popup_is_showing()) {
                smart_island_warning_marquee_start();
            }
        }
        return;
    }

    /* An unrelated hidden-page notice must not overwrite a suspended fault. */
    if (g_si_ctx.lifecycle.suspended && !key) return;
    smart_island_warning_stop();
    smart_island_warning_fault_clear();
    if (key) {
        g_si_ctx.warning.fault.valid = true;
        g_si_ctx.warning.fault.source = key->source;
        g_si_ctx.warning.fault.fault_type = key->type;
        g_si_ctx.warning.fault.code = key->code;
    }

    g_si_ctx.warning.resume_counting =
        g_si_ctx.lifecycle.count_session_active;
    g_si_ctx.warning.level = level;
    lv_snprintf(g_si_ctx.warning.text, sizeof(g_si_ctx.warning.text), "%s",
                next_warning_text);

    smart_island_set_scene(SMART_ISLAND_SCENE_WARNING);
    if (g_si_ctx.lifecycle.suspended) {
        g_si_ctx.warning.resume_animation_pending = true;
        return;
    }
    g_si_ctx.view.page = SMART_ISLAND_PAGE_INFO;
    smart_island_set_visual(SMART_ISLAND_VISUAL_COMPACT, true);
    smart_island_reset_page_positions();
    smart_island_reset_compact_header_position();
    smart_island_reset_time_position();
    smart_island_warning_marquee_start();
    smart_island_view_notice_expand();
}

void smart_island_warning_resume_if_pending(void)
{
    if (!g_si_ctx.warning.resume_animation_pending ||
        g_si_ctx.lifecycle.suspended ||
        g_si_ctx.view.scene != SMART_ISLAND_SCENE_WARNING) {
        return;
    }

    g_si_ctx.warning.resume_animation_pending = false;
    g_si_ctx.view.page = SMART_ISLAND_PAGE_INFO;
    smart_island_set_visual(SMART_ISLAND_VISUAL_COMPACT, false);
    smart_island_reset_page_positions();
    smart_island_reset_compact_header_position();
    smart_island_reset_time_position();
    smart_island_warning_marquee_start();
    smart_island_view_notice_expand();
}

void smart_island_notify_warning(const char *warn_text)
{
    smart_island_notify_warning_level(warn_text, SMART_ISLAND_WARNING_LEVEL_WARNING);
}

void smart_island_notify_warning_level(const char *text, smart_island_warning_level_t level)
{
    notify_warning(text,level,NULL);
}
void smart_island_notify_fault(const char *text, machine_fault_key_t key)
{
    notify_warning(text,SMART_ISLAND_WARNING_LEVEL_ERROR,&key);
}
void smart_island_notify_no_note(machine_fault_key_t key)
{
    notify_warning(ui_tr("No banknotes detected"),SMART_ISLAND_WARNING_LEVEL_WARNING,&key);
}
void smart_island_notify_preset_full(machine_fault_key_t key)
{
    notify_warning(UI_N_("Preset count reached"),SMART_ISLAND_WARNING_LEVEL_PRESET,&key);
}
