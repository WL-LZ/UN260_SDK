#include "smart_island.h"
#include <string.h>

#include "un260/lv_components/smart_island/smart_island_internal.h"
#include "un260/lv_system/ui_text.h"

/* 内部状态统一收口，后续子模块通过 internal.h 共享。 */
smart_island_context_t g_si_ctx = {
    .action.page_count = SMART_ISLAND_ACTION_PAGE_DEFAULT_COUNT,
    .view.scene = SMART_ISLAND_SCENE_IDLE,
    .view.visual = SMART_ISLAND_VISUAL_COMPACT,
    .view.page = SMART_ISLAND_PAGE_INFO,
    .view.bg_current = SMART_ISLAND_BG_IDLE,
    .view.bg_from = SMART_ISLAND_BG_IDLE,
    .view.bg_to = SMART_ISLAND_BG_IDLE,
    .warning.level = SMART_ISLAND_WARNING_LEVEL_WARNING,
    .warning.fault.source = FAULT_SRC_START_COUNT,
    .text.idle_quality_percent = 100,
};

void smart_island_destroy(void)
{
    smart_island_result_stop_timer();
    smart_island_warning_stop();
    smart_island_view_destroy_objects();
    g_si_ctx.view.page_slide_dir = 0;
    g_si_ctx.view.anim_running = false;
    g_si_ctx.action.ignore_click_once = false;
    g_si_ctx.action.ignore_action_click_once = false;
    g_si_ctx.warning.marquee_running = false;
    g_si_ctx.warning.marquee_step = 0;
    g_si_ctx.warning.text_width_compact = 0;
    g_si_ctx.warning.text_width_expand = 0;
    g_si_ctx.warning.resume_animation_pending = false;
    g_si_ctx.warning.resume_counting = false;
    g_si_ctx.view.swipe.pressed = false;
    g_si_ctx.view.swipe.swiped = false;
    g_si_ctx.view.swipe.start_pt.x = 0;
    g_si_ctx.view.swipe.start_pt.y = 0;
    smart_island_warning_fault_clear();
    g_si_ctx.view.bg_current = SMART_ISLAND_BG_IDLE;
    g_si_ctx.view.bg_from = SMART_ISLAND_BG_IDLE;
    g_si_ctx.view.bg_to = SMART_ISLAND_BG_IDLE;
    g_si_ctx.view.bg_anim_running = false;
    g_si_ctx.text.idle_line1[0] = '\0';
    g_si_ctx.text.idle_line2[0] = '\0';
    g_si_ctx.text.idle_line3[0] = '\0';
    g_si_ctx.text.info_extra[0] = '\0';
    g_si_ctx.text.idle_quality_percent = 100;
    g_si_ctx.text.idle_has_issue = false;
    g_si_ctx.text.idle_has_data = false;
    g_si_ctx.text.idle_no_count = true;
    g_si_ctx.lifecycle.count_session_active = false;
    g_si_ctx.lifecycle.result_transition_pending = false;
    memset(&g_si_ctx.counting, 0, sizeof(g_si_ctx.counting));

    g_si_ctx.lifecycle.created = false;
    g_si_ctx.lifecycle.suspended = false;
    g_si_ctx.lifecycle.dirty = false;
}

bool smart_island_is_attached_to(lv_obj_t *parent)
{
    if (parent == NULL || !lv_obj_is_valid(parent)) {
        return false;
    }

    if (g_si_ctx.objects.root == NULL || !lv_obj_is_valid(g_si_ctx.objects.root)) {
        return false;
    }

    return lv_obj_get_parent(g_si_ctx.objects.root) == parent;
}

void smart_island_refresh_time(void)
{
    if (g_si_ctx.lifecycle.suspended) {
        g_si_ctx.lifecycle.dirty = true;
        return;
    }
    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_IDLE &&
        !(g_si_ctx.view.visual == SMART_ISLAND_VISUAL_EXPANDED &&
          g_si_ctx.view.page == SMART_ISLAND_PAGE_ACTION)) {
        smart_island_update_idle_time();
    }
}
void smart_island_set_visual(smart_island_visual_t visual, bool anim_en)
{
    if ((unsigned int)visual > (unsigned int)SMART_ISLAND_VISUAL_EXPANDED) return;
    if (g_si_ctx.objects.root == NULL || !lv_obj_is_valid(g_si_ctx.objects.root)) return;

    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_RESULT && visual == SMART_ISLAND_VISUAL_EXPANDED) {
        visual = SMART_ISLAND_VISUAL_COMPACT;
    }

    g_si_ctx.view.visual = visual;
    if (g_si_ctx.lifecycle.suspended) {
        g_si_ctx.lifecycle.dirty = true;
        return;
    }
    smart_island_view_apply_visual(visual, anim_en);
}

void smart_island_set_suspended(bool suspended)
{
    if (g_si_ctx.lifecycle.suspended == suspended) return;

    g_si_ctx.lifecycle.suspended = suspended;
    if (suspended) {
        /*
         * Suspended only means that LVGL objects must not be touched while the
         * main page is hidden.  It is not itself a data change.  Model update
         * entry points set dirty when an update really arrives while hidden.
         * Keeping a clean island clean avoids rebuilding the complete scene on
         * every cached main-page resume.
         */
        if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_WARNING) {
            bool keep_fault = g_si_ctx.warning.fault.valid;

            smart_island_warning_stop();
            if (keep_fault) {
                g_si_ctx.warning.resume_animation_pending = true;
            } else {
                g_si_ctx.warning.text[0] = '\0';
                g_si_ctx.view.scene = SMART_ISLAND_SCENE_IDLE;
                g_si_ctx.view.visual = SMART_ISLAND_VISUAL_COMPACT;
                g_si_ctx.lifecycle.dirty = true;
            }
        }
        return;
    }

    if (g_si_ctx.lifecycle.dirty && g_si_ctx.lifecycle.created) {
        smart_island_view_apply_visual(g_si_ctx.view.visual, false);
        smart_island_view_refresh_scene();
        g_si_ctx.lifecycle.dirty = false;
    }
    smart_island_warning_resume_if_pending();
}

void smart_island_set_scene(smart_island_scene_t scene, const char *title, const char *subtitle)
{
    if ((unsigned int)scene > (unsigned int)SMART_ISLAND_SCENE_QR) return;

    g_si_ctx.view.scene = scene;
    smart_island_result_stop_timer();

    if (title && title[0] != '\0') lv_snprintf(g_si_ctx.view.content.title, sizeof(g_si_ctx.view.content.title), "%s", title);
    else g_si_ctx.view.content.title[0] = '\0';

    if (subtitle && subtitle[0] != '\0') lv_snprintf(g_si_ctx.view.content.subtitle, sizeof(g_si_ctx.view.content.subtitle), "%s", subtitle);
    else g_si_ctx.view.content.subtitle[0] = '\0';

    if (g_si_ctx.lifecycle.suspended) {
        g_si_ctx.lifecycle.dirty = true;
        return;
    }
    smart_island_view_refresh_scene();
}

void smart_island_notify_update(uint16_t progress, const char *text)
{
    if (progress > 100U) progress = 100U;
    g_si_ctx.view.content.progress = progress;
    smart_island_set_scene(SMART_ISLAND_SCENE_UPDATE,
        text,
        ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_UPDATE_SUBTITLE));
    if (g_si_ctx.lifecycle.suspended) return;
    if (g_si_ctx.objects.progress && lv_obj_is_valid(g_si_ctx.objects.progress)) {
        lv_obj_clear_flag(g_si_ctx.objects.progress, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(g_si_ctx.objects.progress, progress, LV_ANIM_ON);
    }
    smart_island_view_message_pulse();
    smart_island_open_info_page();
}

void smart_island_notify_qr(const char *text)
{
    smart_island_set_scene(SMART_ISLAND_SCENE_QR,
        text,
        ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_QR_INFO_SUBTITLE));
    if (g_si_ctx.lifecycle.suspended) return;
    if (g_si_ctx.objects.progress && lv_obj_is_valid(g_si_ctx.objects.progress)) lv_obj_add_flag(g_si_ctx.objects.progress, LV_OBJ_FLAG_HIDDEN);
    smart_island_view_message_pulse();
    smart_island_open_info_page();
}

void smart_island_restore_idle(void)
{
    bool from_result = g_si_ctx.view.scene == SMART_ISLAND_SCENE_RESULT;

    g_si_ctx.lifecycle.count_session_active = false;
    if (g_si_ctx.lifecycle.suspended) {
        g_si_ctx.warning.level = SMART_ISLAND_WARNING_LEVEL_WARNING;
        g_si_ctx.warning.text[0] = '\0';
        g_si_ctx.warning.resume_animation_pending = false;
        g_si_ctx.warning.resume_counting = false;
        smart_island_warning_fault_clear();
        g_si_ctx.text.result[0] = '\0';
        g_si_ctx.view.scene = SMART_ISLAND_SCENE_IDLE;
        g_si_ctx.view.visual = SMART_ISLAND_VISUAL_COMPACT;
        g_si_ctx.lifecycle.dirty = true;
        return;
    }
    smart_island_warning_stop();
    g_si_ctx.warning.level = SMART_ISLAND_WARNING_LEVEL_WARNING;
    smart_island_warning_fault_clear();
    if (g_si_ctx.objects.progress && lv_obj_is_valid(g_si_ctx.objects.progress)) {
        lv_obj_add_flag(g_si_ctx.objects.progress, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(g_si_ctx.objects.progress, 0, LV_ANIM_OFF);
    }
    g_si_ctx.warning.text[0] = '\0';
    g_si_ctx.warning.resume_animation_pending = false;
    g_si_ctx.warning.resume_counting = false;
    g_si_ctx.text.result[0] = '\0';
    smart_island_set_scene(SMART_ISLAND_SCENE_IDLE, NULL, NULL);
    /* The result collapse has already landed on compact geometry.  Reapplying
     * visual state here performs the complete style/layout pass twice in one
     * callback and causes a perceptible final hitch on the target CPU. */
    if (!from_result) {
        smart_island_set_visual(SMART_ISLAND_VISUAL_COMPACT, true);
    } else {
        g_si_ctx.view.visual = SMART_ISLAND_VISUAL_COMPACT;
    }
    smart_island_update_idle_time();
    smart_island_reset_compact_header_position();
    smart_island_reset_time_position();
    if (from_result) {
        smart_island_view_idle_enter();
    }
}

bool smart_island_is_expanded(void) { return g_si_ctx.view.visual == SMART_ISLAND_VISUAL_EXPANDED; }

void smart_island_refresh_language_texts(void)
{
    if (g_si_ctx.lifecycle.suspended) {
        g_si_ctx.lifecycle.dirty = true;
        return;
    }
    smart_island_action_page_refresh_language_texts();
    smart_island_view_refresh_scene();
}

void smart_island_refresh_summary(void)
{
    if (g_si_ctx.lifecycle.suspended) {
        g_si_ctx.lifecycle.dirty = true;
        return;
    }
    /* Warning 场景中避免外部刷新重刷样式，防止 ESC/CLEAR 造成文本“重新闪烁” */
    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_WARNING) {
        return;
    }

    smart_island_view_refresh_scene();
}

void smart_island_set_idle_info_line1(const char *text)
{
    const char *value = text != NULL ? text : "";

    if (strcmp(g_si_ctx.text.idle_line1, value) == 0) return;
    smart_island_view_set_idle_line(g_si_ctx.text.idle_line1, sizeof(g_si_ctx.text.idle_line1), text);
    if (g_si_ctx.lifecycle.suspended) {
        g_si_ctx.lifecycle.dirty = true;
        return;
    }
    smart_island_view_refresh_scene();
}

void smart_island_set_idle_info_line2(const char *text)
{
    const char *value = text != NULL ? text : "";

    if (strcmp(g_si_ctx.text.idle_line2, value) == 0) return;
    smart_island_view_set_idle_line(g_si_ctx.text.idle_line2, sizeof(g_si_ctx.text.idle_line2), text);
    if (g_si_ctx.lifecycle.suspended) {
        g_si_ctx.lifecycle.dirty = true;
        return;
    }
    smart_island_view_refresh_scene();
}

void smart_island_set_idle_info_line3(const char *text)
{
    const char *value = text != NULL ? text : "";

    if (strcmp(g_si_ctx.text.idle_line3, value) == 0) return;
    smart_island_view_set_idle_line(g_si_ctx.text.idle_line3, sizeof(g_si_ctx.text.idle_line3), text);
    if (g_si_ctx.lifecycle.suspended) {
        g_si_ctx.lifecycle.dirty = true;
        return;
    }
    smart_island_view_refresh_scene();
}

void smart_island_set_pure_count_enabled(bool enabled)
{
    g_si_ctx.lifecycle.pure_count_enabled = enabled;
}

bool smart_island_pure_count_is_enabled(void)
{
    return g_si_ctx.lifecycle.pure_count_enabled;
}
