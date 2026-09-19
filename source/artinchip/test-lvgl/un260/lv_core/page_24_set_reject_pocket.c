#include "page_24_set_reject_pocket.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static lv_settings_frame_t frame;
static lv_obj_t *value_button, *confirmed, *presets[8];
static bool pending;

static void refresh(void)
{
    if (!frame.root) return;
    uint8_t capacity = machine_state_reject_pocket_max();
    lv_label_set_text_fmt(confirmed, "%u %s", (unsigned)capacity,
        ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_PCS));
    for (unsigned i = 0; i < 8; ++i) {
        if (capacity == 30 + i * 10) lv_obj_add_state(presets[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(presets[i], LV_STATE_CHECKED);
        if (pending) lv_obj_add_state(presets[i], LV_STATE_DISABLED);
        else lv_obj_clear_state(presets[i], LV_STATE_DISABLED);
    }
    if (pending) lv_obj_add_state(value_button, LV_STATE_DISABLED);
    else lv_obj_clear_state(value_button, LV_STATE_DISABLED);
}

static void request_capacity(uint8_t capacity)
{
    if (pending) return;
    if (capacity < REJECT_POCKET_MIN_CAPACITY) capacity = REJECT_POCKET_MIN_CAPACITY;
    if (capacity > REJECT_POCKET_MAX_CAPACITY) capacity = REJECT_POCKET_MAX_CAPACITY;
    uint8_t previous = machine_state_reject_pocket_max();
    if (capacity == previous) return;
    if (!setting_service_request_reject_pocket_max(capacity, previous)) {
        lv_label_set_text(frame.message, "Could not send the change. Please try again.");
        return;
    }
    pending = true;
    lv_label_set_text_fmt(frame.message, "Applying %u - waiting for controller.", (unsigned)capacity);
    refresh();
}

static void keyboard_done(const char *value, void *user_data)
{
    (void)user_data;
    if (!value || !*value || !frame.root) return;
    char *end;
    long capacity = strtol(value, &end, 10);
    if (*end || capacity < REJECT_POCKET_MIN_CAPACITY || capacity > REJECT_POCKET_MAX_CAPACITY) {
        lv_label_set_text_fmt(frame.message, "Enter a capacity from %u to %u.",
            REJECT_POCKET_MIN_CAPACITY, REJECT_POCKET_MAX_CAPACITY);
        return;
    }
    request_capacity((uint8_t)capacity);
}

static void edit(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || pending) return;
    char value[8];
    lv_snprintf(value, sizeof(value), "%u", (unsigned)machine_state_reject_pocket_max());
    settings_detail_keyboard_show(ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_CAPACITY),
        value, 3, SETTINGS_DETAIL_KEYBOARD_NUM, keyboard_done, NULL);
}

static void choose(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED)
        request_capacity((uint8_t)(uintptr_t)lv_event_get_user_data(event));
}

static void back(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    settings_detail_keyboard_hide();
    ui_manager_pop_page();
}

void ui_page_24_set_reject_pocket_create(lv_obj_t *parent)
{
    if (frame.root) return;
    lv_settings_header_t header = {
        .title = ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_TITLE), .icon = "Layers", .back = back
    };
    frame = lv_settings_frame_create(parent, &header);
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    lv_settings_label(frame.body, "Confirmed capacity", 0, 12,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    confirmed = lv_settings_label(frame.body, "", 0, 52,
        &lv_font_instrument_sans_semibold_28, 0x1D2B34);
    value_button = lv_settings_button(frame.body, 0, 114, 288, 52, "Enter capacity", false, edit, NULL);
    lv_settings_label(frame.body, "Quick selection", 352, 12,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    for (unsigned i = 0; i < 8; ++i) {
        char text[8];
        lv_snprintf(text, sizeof(text), "%u", 30 + i * 10);
        presets[i] = lv_settings_button(frame.body, 352 + (int)(i % 4) * 224,
            54 + (int)(i / 4) * 72, 208, 60, text, false, choose,
            (void *)(uintptr_t)(30 + i * 10));
        lv_obj_set_style_bg_color(presets[i], lv_color_hex(0xFFFFFF), 0);
    }
    lv_label_set_text(frame.message, pending ? "Waiting for controller." : "Select a capacity to apply.");
    refresh();
}

void ui_page_24_set_reject_pocket_destroy(void)
{
    settings_detail_keyboard_hide();
    if (frame.root) lv_obj_del(frame.root);
    memset(&frame, 0, sizeof(frame));
    memset(presets, 0, sizeof(presets));
    value_button = confirmed = NULL;
}

void ui_page_24_set_reject_pocket_on_boot_setting(void)
{
    setting_service_clear_reject_pocket_max_request();
    pending = false;
    refresh();
    if (frame.message) lv_label_set_text(frame.message, "");
}

void ui_page_24_set_reject_pocket_on_reply(const setting_value_result_t *result)
{
    if (!result) return;
    pending = false;
    refresh();
    if (frame.message) lv_label_set_text(frame.message, result->success ? "Capacity confirmed." :
        result->timeout ? "No confirmation received. Previous capacity retained." : "Change rejected. Previous capacity retained.");
}
