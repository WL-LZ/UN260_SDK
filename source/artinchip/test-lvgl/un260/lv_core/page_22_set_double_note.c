#include "page_22_set_double_note.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include <stdint.h>
#include <string.h>

static lv_settings_frame_t frame;
static lv_obj_t *options[3], *option_titles[3], *option_hints[3], *checks[3], *confirmed;
static bool pending;
static const ui_text_id_t names[] = {
    UI_TEXT_SETTINGS_DOUBLE_NOTE_LEVEL_1, UI_TEXT_SETTINGS_DOUBLE_NOTE_LEVEL_2,
    UI_TEXT_SETTINGS_DOUBLE_NOTE_LEVEL_3
};

static void refresh(void)
{
    if (!frame.root) return;
    uint8_t level = machine_state_double_note_level();
    lv_label_set_text_fmt(confirmed, "Confirmed level: %u", (unsigned)level);
    for (unsigned i = 0; i < 3; ++i) {
        bool selected = level == i + 1;
        if (selected) lv_obj_clear_flag(checks[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(checks[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(option_titles[i], lv_color_hex(pending ? 0x586B78 : selected ? 0x1462CC : 0x1D2B34), 0);
        lv_label_set_text(option_hints[i], selected ? "Current level" : pending ? "" : "Tap to apply");
        if (selected) lv_obj_add_state(options[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(options[i], LV_STATE_CHECKED);
        if (pending) lv_obj_add_state(options[i], LV_STATE_DISABLED);
        else lv_obj_clear_state(options[i], LV_STATE_DISABLED);
    }
}

static void back(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) ui_manager_pop_page();
}

static void choose(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || pending) return;
    uint8_t target = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    uint8_t previous = machine_state_double_note_level();
    if (target == previous) return;
    if (!setting_service_request_double_note_level(target, previous)) {
        lv_label_set_text(frame.message, "Could not send the change. Please try again.");
        return;
    }
    pending = true;
    lv_label_set_text_fmt(frame.message, "Applying level %u - waiting for controller.", (unsigned)target);
    refresh();
}

void ui_page_22_set_double_note_create(lv_obj_t *parent)
{
    if (frame.root) return;
    lv_settings_header_t header = {
        .title = ui_text_get(UI_TEXT_SETTINGS_DOUBLE_NOTE_TITLE), .icon = "Layers", .back = back
    };
    frame = lv_settings_frame_create(parent, &header);
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    confirmed = lv_settings_label(frame.body, "", 0, 12,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    for (unsigned i = 0; i < 3; ++i) {
        options[i] = lv_settings_button(frame.body, (int)i * 416, 56, 400, 168,
            "", false, choose, (void *)(uintptr_t)(i + 1));
        lv_obj_set_style_bg_color(options[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(options[i], 1, 0);
        lv_obj_set_style_border_color(options[i], lv_color_hex(0xE3E9ED), 0);
        option_titles[i] = lv_settings_label(options[i], ui_text_get(names[i]), 24, 36,
            &lv_font_instrument_sans_medium_28, 0x1D2B34);
        option_hints[i] = lv_settings_label(options[i], "", 24, 118,
            &lv_font_instrument_sans_medium_14, 0x586B78);
        checks[i] = lv_settings_icon(options[i], "Check-active", 352, 22);
    }
    lv_label_set_text(frame.message, pending ? "Waiting for controller." : "Select a level to apply.");
    refresh();
}

void ui_page_22_set_double_note_destroy(void)
{
    if (frame.root) lv_obj_del(frame.root);
    memset(&frame, 0, sizeof(frame));
    memset(options, 0, sizeof(options));
    memset(option_titles, 0, sizeof(option_titles));
    memset(option_hints, 0, sizeof(option_hints));
    memset(checks, 0, sizeof(checks));
    confirmed = NULL;
}

void ui_page_22_set_double_note_on_boot_setting(void)
{
    setting_service_clear_double_note_level_request();
    pending = false;
    refresh();
    if (frame.message) lv_label_set_text(frame.message, "");
}

void ui_page_22_set_double_note_on_reply(const setting_value_result_t *result)
{
    if (!result) return;
    pending = false;
    refresh();
    if (frame.message) lv_label_set_text(frame.message, result->success ? "Change confirmed." :
        result->timeout ? "No confirmation received. Previous level retained." : "Change rejected. Previous level retained.");
}
