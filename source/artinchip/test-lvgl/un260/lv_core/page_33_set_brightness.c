#include "page_33_set_brightness.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_system/backlight_service.h"
#include "lv_port_indev.h"
#include <string.h>

static lv_settings_frame_t frame;
static struct {
    lv_obj_t *slider, *value, *keep, *revert;
    lv_timer_t *timer;
    int previous, shown_seconds;
    uint32_t changed;
    bool pending, save_failed;
} preview;

static void refresh(void)
{
    if (!frame.root) return;
    int maximum = backlight_service_max();
    int level = backlight_service_level();
    if (maximum > 0) lv_label_set_text_fmt(preview.value, "%d%%", level * 100 / maximum);
    else lv_label_set_text(preview.value, "--");
    if (preview.pending) {
        lv_obj_clear_state(preview.keep, LV_STATE_DISABLED);
        lv_obj_clear_state(preview.revert, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(preview.keep, LV_STATE_DISABLED);
        lv_obj_add_state(preview.revert, LV_STATE_DISABLED);
    }
}

static void restore(void)
{
    if (!preview.pending) return;
    int previous = preview.previous < 1 ? 1 : preview.previous;
    bool ok = backlight_service_set(previous);
    preview.pending = false;
    preview.save_failed = false;
    if (preview.timer) lv_timer_pause(preview.timer);
    if (preview.slider) lv_slider_set_value(preview.slider, backlight_service_level(), LV_ANIM_OFF);
    refresh();
    if (frame.message) lv_label_set_text(frame.message, ok ?
        "Previous brightness restored." : "Could not restore the previous brightness.");
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    if (!preview.pending || !preview.slider) return;
    if (!lv_obj_is_visible(preview.slider)) { restore(); return; }
    if (lv_slider_is_dragged(preview.slider)) return;
    uint32_t elapsed = lv_tick_elaps(preview.changed);
    if (elapsed >= 8000) { restore(); return; }
    if (preview.save_failed) return;
    int seconds = (int)((8000 - elapsed + 999) / 1000);
    if (seconds != preview.shown_seconds) {
        preview.shown_seconds = seconds;
        lv_label_set_text_fmt(frame.message, "Preview - reverts in %d seconds unless you keep it.", seconds);
    }
}

static void keep(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !preview.pending) return;
    if (!backlight_service_save()) {
        preview.save_failed = true;
        lv_label_set_text(frame.message, "Save failed. The preview will revert automatically.");
        return;
    }
    preview.pending = false;
    if (preview.timer) lv_timer_pause(preview.timer);
    refresh();
    lv_label_set_text(frame.message, "Brightness saved.");
}

static void revert(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) restore();
}

static void slider_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_RELEASED && preview.pending) {
        preview.changed = lv_tick_get();
        preview.shown_seconds = -1;
        return;
    }
    if (code != LV_EVENT_VALUE_CHANGED || !preview.timer) return;
    int requested = lv_slider_get_value(preview.slider);
    if (!preview.pending) preview.previous = backlight_service_level();
    if (!backlight_service_set(requested)) {
        lv_slider_set_value(preview.slider, backlight_service_level(), LV_ANIM_OFF);
        lv_label_set_text(frame.message, "Could not change brightness.");
        return;
    }
    preview.pending = true;
    preview.save_failed = false;
    preview.changed = lv_tick_get();
    preview.shown_seconds = -1;
    lv_timer_reset(preview.timer);
    lv_timer_resume(preview.timer);
    refresh();
    lv_label_set_text(frame.message, "Preview - keep this brightness or let it revert.");
}

static void back(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    restore();
    ui_manager_pop_page();
}

static void deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) != frame.root) return;
    restore();
    if (preview.timer) lv_timer_del(preview.timer);
    memset(&preview, 0, sizeof(preview));
    memset(&frame, 0, sizeof(frame));
}

void ui_page_33_set_brightness_create(lv_obj_t *parent)
{
    if (frame.root) return;
    lv_settings_header_t header = { .title = "Brightness", .icon = "Sun", .back = back };
    frame = lv_settings_frame_create(parent, &header);
    lv_obj_add_event_cb(frame.root, deleted, LV_EVENT_DELETE, NULL);
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    lv_settings_label(frame.body, "Display brightness", 0, 20,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    preview.value = lv_settings_label(frame.body, "--", 0, 70,
        &lv_font_instrument_sans_semibold_28, 0x1D2B34);
    lv_obj_t *track = lv_settings_box(frame.body, 304, 12, 928, 202, 0xFFFFFF);
    lv_obj_set_style_radius(track, 14, 0);
    lv_settings_label(track, "Adjust to preview", 28, 22,
        &lv_font_instrument_sans_medium_16, 0x1D2B34);
    preview.slider = lv_slider_create(track);
    lv_port_indev_set_drag_obj(preview.slider, true);
    lv_obj_set_pos(preview.slider, 42, 91);
    lv_obj_set_size(preview.slider, 840, 14);
    lv_obj_set_ext_click_area(preview.slider, 20);
    lv_obj_set_style_bg_color(preview.slider, lv_color_hex(0xE3E9ED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(preview.slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(preview.slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(preview.slider, lv_color_hex(0x1462CC), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(preview.slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(preview.slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(preview.slider, lv_color_hex(0x1462CC), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(preview.slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(preview.slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(preview.slider, 10, LV_PART_KNOB);
    lv_obj_add_event_cb(preview.slider, slider_event, LV_EVENT_ALL, NULL);
    lv_settings_label(track, "Minimum", 28, 140, &lv_font_instrument_sans_medium_14, 0x586B78);
    lv_settings_label(track, "Maximum", 826, 140, &lv_font_instrument_sans_medium_14, 0x586B78);
    preview.revert = lv_settings_button(frame.footer, 964, 0, 124, 46, "Revert", false, revert, NULL);
    preview.keep = lv_settings_button(frame.footer, 1100, 0, 132, 46, "Keep", true, keep, NULL);
    preview.timer = lv_timer_create(tick, 100, NULL);
    if (preview.timer) lv_timer_pause(preview.timer);
    bool available = backlight_service_probe() && backlight_service_max() > 0 && preview.timer;
    if (available) {
        lv_slider_set_range(preview.slider, 1, backlight_service_max());
        lv_slider_set_value(preview.slider, backlight_service_level(), LV_ANIM_OFF);
        lv_label_set_text(frame.message, "Unsaved brightness automatically reverts after 8 seconds.");
    } else {
        lv_obj_add_state(preview.slider, LV_STATE_DISABLED);
        lv_label_set_text(frame.message, "Adjustable backlight is unavailable.");
    }
    refresh();
}

void ui_page_33_set_brightness_destroy(void)
{
    restore();
    if (frame.root) lv_obj_del(frame.root);
}
