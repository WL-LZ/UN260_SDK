#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_components/ui_notice.h"
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
static bool pending, edited, unconfirmed;
static uint8_t draft, submitted;
static lv_obj_t *save_button;

static void refresh(void)
{
    if (!frame.root) return;
    uint8_t capacity = draft;
    lv_label_set_text_fmt(confirmed, "%u %s", (unsigned)capacity,
        ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_PCS));
    for (unsigned i = 0; i < 8; ++i) {
        if (draft == 30 + i * 10) lv_obj_add_state(presets[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(presets[i], LV_STATE_CHECKED);
    }

    settings_detail_action_wait(save_button, pending ? UI_N_("Waiting for controller...") :
        draft == machine_state_reject_pocket_max() && !unconfirmed ? UI_N_("No changes to save.") : NULL);
    lv_label_set_text(frame.message, pending ? ui_tr("Waiting for controller...") :
        unconfirmed ? ui_tr("No response received. Check device status.") :
        draft != machine_state_reject_pocket_max() ? ui_tr("Unsaved changes") : ui_tr("No changes"));
}

static void save(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || pending) return;
    uint8_t capacity = draft;
    if (capacity < REJECT_POCKET_MIN_CAPACITY) capacity = REJECT_POCKET_MIN_CAPACITY;
    if (capacity > REJECT_POCKET_MAX_CAPACITY) capacity = REJECT_POCKET_MAX_CAPACITY;
    uint8_t previous = machine_state_reject_pocket_max();
    if (capacity == previous && !unconfirmed) return;
    if (!setting_service_request_reject_pocket_max(capacity, previous)) {
        ui_notice_post_text(UI_NOTICE_ERROR,"settings.reject_pocket",UI_N_("Reject pocket"),UI_N_("Request not sent. Try again."));
        return;
    }
    submitted = draft;
    edited = false;
    pending = true;
    ui_notice_post_text(UI_NOTICE_PROGRESS,"settings.reject_pocket",UI_N_("Reject pocket"),UI_N_("Waiting for controller..."));
    refresh();
}

static void keyboard_done(const char *value, void *user_data)
{
    (void)user_data;
    if (!value || !*value || !frame.root) return;
    char *end;
    long capacity = strtol(value, &end, 10);
    if (*end || capacity < REJECT_POCKET_MIN_CAPACITY || capacity > REJECT_POCKET_MAX_CAPACITY) {
        ui_notice_post_text(UI_NOTICE_WARNING,"settings.reject_pocket",UI_N_("Invalid capacity"),UI_N_("Enter 30-100 notes."));
        return;
    }
    draft = (uint8_t)capacity; edited = true; refresh();
}

static void edit(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    char value[8];
    lv_snprintf(value, sizeof(value), "%u", (unsigned)draft);
    settings_detail_keyboard_show(ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_CAPACITY),
        value, 3, SETTINGS_DETAIL_KEYBOARD_UINT, keyboard_done, NULL);
}

static void choose(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    draft = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    edited = true;
    refresh();
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
    draft = pending ? submitted : machine_state_reject_pocket_max();
    edited = false;
    lv_settings_header_t header = {
        .title = ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_TITLE), .icon = "Layers", .back = back
    };
    frame = lv_settings_frame_create(parent, &header);
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    lv_obj_t *row=lv_settings_panel(frame.body,0,0,1232,208);
    lv_settings_label(row, ui_tr("Reject pocket capacity"), 24, 22,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    lv_settings_label(row,ui_tr("Select a preset or enter a custom capacity."),24,54,&lv_font_instrument_sans_medium_14,0x586B78);
    confirmed = lv_settings_label(row, "", 776, 26,
        &lv_font_instrument_sans_semibold_22, 0x1D2B34);
    value_button = lv_settings_action_button(row, 976, 19, 228, 52, ui_tr("Enter capacity"), LV_SETTINGS_ACTION_INPUT_TINTED, edit, NULL);
    lv_settings_box(row,24,90,1180,1,0xE8EDF0);
    lv_settings_label(row,ui_tr("Quick selection"),24,128,&lv_font_instrument_sans_medium_16,0x536B79);
    lv_obj_t *base=lv_settings_segment_base(row,270,114,934,52);
    for (unsigned i = 0; i < 8; ++i) {
        char text[8];
        lv_snprintf(text, sizeof(text), "%u", 30 + i * 10);
        presets[i] = lv_settings_segment(base,i,8,text,choose,(void *)(uintptr_t)(30+i*10));
    }
    lv_settings_action_button(frame.footer, 0, 0, 132, 46, ui_tr("Cancel"), LV_SETTINGS_ACTION_RETURN, back, NULL);
    save_button = lv_settings_button(frame.footer, 0, 0, 132, 46, ui_tr("Save"), true, save, NULL);
    refresh();
}

void ui_page_24_set_reject_pocket_destroy(void)
{
    settings_detail_keyboard_hide();
    if (frame.root) lv_obj_del(frame.root);
    memset(&frame, 0, sizeof(frame));
    memset(presets, 0, sizeof(presets));
    value_button = confirmed = save_button = NULL;
}

void ui_page_24_set_reject_pocket_on_boot_setting(void)
{
    setting_service_clear_reject_pocket_max_request();
    pending = false;
    unconfirmed = false;
    if (!edited) draft = machine_state_reject_pocket_max();
    refresh();
}

void ui_page_24_set_reject_pocket_on_reply(const setting_value_result_t *result)
{
    if (!result) return;
    unconfirmed = result->timeout;
    if (result->success && !edited) draft = machine_state_reject_pocket_max();
    pending = false;
    refresh();

}
