#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_components/ui_notice.h"
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
static lv_obj_t *options[3], *confirmed;
static bool pending, edited, unconfirmed;
static uint8_t draft, submitted;
static lv_obj_t *save_button;
static const ui_text_id_t names[] = {
    UI_TEXT_SETTINGS_DOUBLE_NOTE_LEVEL_1, UI_TEXT_SETTINGS_DOUBLE_NOTE_LEVEL_2,
    UI_TEXT_SETTINGS_DOUBLE_NOTE_LEVEL_3
};

static void refresh(void)
{
    if (!frame.root) return;
    uint8_t level = machine_state_double_note_level();
    lv_label_set_text_fmt(confirmed, ui_tr("Confirmed level: %u"), (unsigned)level);
    for (unsigned i = 0; i < 3; ++i) {
        bool selected = draft == i + 1;
        if (selected) lv_obj_add_state(options[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(options[i], LV_STATE_CHECKED);
    }
    settings_detail_action_wait(save_button, pending ? UI_N_("Waiting for controller...") :
        draft == machine_state_double_note_level() && !unconfirmed ? UI_N_("No changes to save.") : NULL);
    lv_label_set_text(frame.message, pending ? ui_tr("Waiting for controller...") :
        unconfirmed ? ui_tr("No response received. Check device status.") :
        draft != machine_state_double_note_level() ? ui_tr("Unsaved changes") : ui_tr("No changes"));
}

static void back(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) ui_manager_pop_page();
}

static void save(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || pending) return;
    uint8_t target = draft;
    uint8_t previous = machine_state_double_note_level();
    if (target == previous && !unconfirmed) return;
    if (!setting_service_request_double_note_level(target, previous)) {
        ui_notice_post_text(UI_NOTICE_ERROR,"settings.double_note",UI_N_("Double-note detection"),UI_N_("Request not sent. Try again."));
        return;
    }
    submitted = draft;
    edited = false;
    pending = true;
    ui_notice_post_text(UI_NOTICE_PROGRESS,"settings.double_note",UI_N_("Double-note detection"),UI_N_("Waiting for controller..."));
    refresh();
}

static void choose(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    draft = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    edited = true;
    refresh();
}

void ui_page_22_set_double_note_create(lv_obj_t *parent)
{
    if (frame.root) return;
    draft = pending ? submitted : machine_state_double_note_level();
    edited = false;
    lv_settings_header_t header = {
        .title = ui_text_get(UI_TEXT_SETTINGS_DOUBLE_NOTE_TITLE), .icon = "Layers", .back = back
    };
    frame = lv_settings_frame_create(parent, &header);
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    lv_obj_t *row=lv_settings_panel(frame.body,0,0,1232,112);
    lv_settings_label(row,ui_tr("Double-note detection"),24,25,&lv_font_instrument_sans_medium_22,0x1D2B34);
    lv_settings_label(row,ui_tr("Select the detection level for overlapping notes."),24,61,&lv_font_instrument_sans_medium_14,0x586B78);
    lv_obj_t *base=lv_settings_segment_base(row,766,30,438,52);
    confirmed = lv_settings_label(frame.body, "", 24,136,
        &lv_font_instrument_sans_medium_16, 0x586B78);
    for (unsigned i = 0; i < 3; ++i) {
        options[i] = lv_settings_segment(base,i,3,ui_text_get(names[i]),choose,(void *)(uintptr_t)(i+1));
    }
    lv_settings_action_button(frame.footer, 0, 0, 132, 46, ui_tr("Cancel"), LV_SETTINGS_ACTION_RETURN, back, NULL);
    save_button = lv_settings_button(frame.footer, 0, 0, 132, 46, ui_tr("Save"), true, save, NULL);
    refresh();
}

void ui_page_22_set_double_note_destroy(void)
{
    if (frame.root) lv_obj_del(frame.root);
    memset(&frame, 0, sizeof(frame));
    memset(options, 0, sizeof(options));
    confirmed = save_button = NULL;
}

void ui_page_22_set_double_note_on_boot_setting(void)
{
    setting_service_clear_double_note_level_request();
    pending = false;
    unconfirmed = false;
    if (!edited) draft = machine_state_double_note_level();
    refresh();
}

void ui_page_22_set_double_note_on_reply(const setting_value_result_t *result)
{
    if (!result) return;
    unconfirmed = result->timeout;
    if (result->success && !edited) draft = machine_state_double_note_level();
    pending = false;
    refresh();

}
