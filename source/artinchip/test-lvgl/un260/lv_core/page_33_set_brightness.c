#include "page_33_set_brightness.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_system/backlight_service.h"
#include <string.h>

static lv_obj_t *brightness_page;

static struct {
    lv_obj_t *slider, *value;
    lv_timer_t *timer;
    int previous;
    uint32_t changed;
    bool pending;
} brightness_preview;

static void brightness_restore_preview(void)
{
    if(!brightness_preview.pending) return;
    int previous = brightness_preview.previous;
    /* The UI must never restore an off level even if the driver booted at 0. */
    if(previous < 1) previous = 1;
    bool ok = backlight_service_set(previous);
    brightness_preview.pending = false;
    if(brightness_preview.timer) lv_timer_pause(brightness_preview.timer);
    if(brightness_preview.slider && lv_obj_is_valid(brightness_preview.slider))
        lv_slider_set_value(brightness_preview.slider, backlight_service_level(), LV_ANIM_OFF);
    if(brightness_preview.value && lv_obj_is_valid(brightness_preview.value))
        lv_label_set_text(brightness_preview.value, ok ? "Previous brightness restored. Adjust and press KEEP to save." :
                                                       "Backlight restore failed. Please check the driver.");
}

static void brightness_preview_tick(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if(brightness_preview.pending &&
       ((!lv_slider_is_dragged(brightness_preview.slider) &&
         lv_tick_elaps(brightness_preview.changed) >= 8000) ||
        !lv_obj_is_visible(brightness_preview.slider))) brightness_restore_preview();
}

static void brightness_preview_deleted(lv_event_t *event)
{
    LV_UNUSED(event);
    brightness_preview.slider = brightness_preview.value = NULL;
    brightness_restore_preview();
    if(brightness_preview.timer) lv_timer_del(brightness_preview.timer);
    memset(&brightness_preview, 0, sizeof(brightness_preview));
}

static void brightness_keep(lv_event_t *event)
{
    LV_UNUSED(event);
    if(!brightness_preview.pending) return;
    if(!backlight_service_save()) {
        lv_label_set_text(brightness_preview.value, "Saving failed. Preview will revert automatically.");
        return;
    }
    brightness_preview.pending = false;
    lv_timer_pause(brightness_preview.timer);
    lv_label_set_text_fmt(brightness_preview.value, "Saved: %d%%   /   Level %d of %d",
                         backlight_service_level() * 100 / backlight_service_max(),
                         backlight_service_level(), backlight_service_max());
}

static void brightness_slider_event(lv_event_t *event)
{
    lv_obj_t *slider = lv_event_get_target(event);
    lv_obj_t *label = lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    if(code == LV_EVENT_VALUE_CHANGED) {
        int requested = lv_slider_get_value(slider);
        if(!brightness_preview.pending) brightness_preview.previous = backlight_service_level();
        if(!backlight_service_set(requested)) {
            lv_slider_set_value(slider, backlight_service_level(), LV_ANIM_OFF);
            lv_label_set_text(label, "Brightness write failed. Please check the backlight driver.");
            return;
        }
        brightness_preview.pending = true;
        brightness_preview.changed = lv_tick_get();
        if(!brightness_preview.timer) brightness_preview.timer = lv_timer_create(brightness_preview_tick, 100, NULL);
        lv_timer_reset(brightness_preview.timer); lv_timer_resume(brightness_preview.timer);
        lv_label_set_text_fmt(label, "Preview %d%%   /   Level %d of %d   -   KEEP within 8 seconds",
            backlight_service_level() * 100 / backlight_service_max(),
            backlight_service_level(), backlight_service_max());
    }
}

static void brightness_refresh(lv_obj_t *slider)
{
    brightness_restore_preview();
    bool available = backlight_service_probe();
    lv_obj_t *label = brightness_preview.value;
    if(!available) {
        lv_obj_add_state(slider, LV_STATE_DISABLED);
        lv_label_set_text(label, "No adjustable backlight detected.");
        return;
    }
    lv_obj_clear_state(slider, LV_STATE_DISABLED);
    lv_slider_set_range(slider, 1, backlight_service_max());
    lv_slider_set_value(slider, backlight_service_level(), LV_ANIM_OFF);
    lv_label_set_text_fmt(label, "%d%%   /   Level %d of %d", backlight_service_level() * 100 / backlight_service_max(),
                         backlight_service_level(), backlight_service_max());
}


static void brightness_back(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    brightness_restore_preview();
    ui_manager_pop_page();
}

void ui_page_33_set_brightness_create(lv_obj_t *parent)
{
    if(brightness_page) return;
    lv_obj_t *content = NULL;
    brightness_page = settings_detail_create_page(parent, "BRIGHTNESS SETTING",
                                                  brightness_back, &content);
    lv_obj_t *card = settings_detail_create_card(content, 38, 18, 1204, 274);
    settings_detail_create_label(card, "DISPLAY BRIGHTNESS",
        &lv_font_instrument_sans_bold_18, lv_color_hex(0x243746), 32, 24);
    brightness_preview.value = settings_detail_create_label(card, "",
        &lv_font_instrument_sans_medium_16, lv_color_hex(0x2E85FF), 32, 64);
    lv_obj_set_width(brightness_preview.value, 1128);
    lv_obj_t *slider = lv_slider_create(card);
    brightness_preview.slider = slider;
    lv_obj_set_pos(slider, 48, 132);
    lv_obj_set_size(slider, 1108, 12);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xDCEAFF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x2E85FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x2E85FF), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 9, LV_PART_KNOB);
    settings_detail_create_label(card, "MIN",
        &lv_font_instrument_sans_medium_14, lv_color_hex(0x5686A5), 42, 162);
    settings_detail_create_label(card, "MAX",
        &lv_font_instrument_sans_medium_14, lv_color_hex(0x5686A5), 1128, 162);
    lv_obj_t *note = settings_detail_create_label(card,
        "Adjust to preview. Press KEEP within 8 seconds to save.",
        &lv_font_instrument_sans_medium_14, lv_color_hex(0x5686A5), 32, 219);
    lv_obj_set_width(note, 870);
    lv_obj_t *keep = settings_detail_create_button(card, 1008, 203, 152, 40,
        "KEEP", lv_color_hex(0x2E85FF), brightness_keep, NULL);
    lv_obj_add_event_cb(slider, brightness_slider_event, LV_EVENT_VALUE_CHANGED,
                        brightness_preview.value);
    lv_obj_add_event_cb(slider, brightness_preview_deleted, LV_EVENT_DELETE, NULL);
    brightness_refresh(slider);
    if(!backlight_service_max()) lv_obj_add_state(keep, LV_STATE_DISABLED);
    lv_obj_t *device = settings_detail_create_label(content,
        backlight_service_device(), &lv_font_instrument_sans_medium_12,
        lv_color_hex(0x5686A5), 70, 309);
    lv_obj_set_width(device, 1130);
}

void ui_page_33_set_brightness_destroy(void)
{
    /* This is a transient page. Roll back an unconfirmed preview before its
       controls/timer are destroyed, including navigation by global gestures. */
    brightness_restore_preview();
    if(brightness_page && lv_obj_is_valid(brightness_page)) lv_obj_del(brightness_page);
    brightness_page = NULL;
}
