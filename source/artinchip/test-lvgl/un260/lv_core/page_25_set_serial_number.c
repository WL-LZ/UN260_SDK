#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_components/ui_notice.h"
#include "page_25_set_serial_number.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/serial_number/serial_number.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include <stdint.h>
#include <string.h>

static lv_settings_frame_t frame;
static lv_obj_t *options[4], *confirmed;
static bool pending;
static const ui_text_id_t names[] = {
    UI_TEXT_SETTINGS_SERIAL_LEVEL_OFF, UI_TEXT_SETTINGS_SERIAL_LEVEL_1,
    UI_TEXT_SETTINGS_SERIAL_LEVEL_2, UI_TEXT_SETTINGS_SERIAL_LEVEL_3
};

static void refresh(void)
{
    if (!frame.root) return;
    uint8_t level = serial_number_state_level();
    lv_label_set_text_fmt(confirmed, ui_tr("Confirmed: %s"), level <= SERIAL_NUMBER_LEVEL_MAX ? ui_text_get(names[level]) : ui_tr("Unknown"));
    for (unsigned i = 0; i < 4; ++i) {
        bool selected = level == i;
        if (selected) lv_obj_add_state(options[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(options[i], LV_STATE_CHECKED);
        if (pending) settings_detail_action_block(options[i], UI_N_("Wait for the controller to confirm the current setting."));
        else settings_detail_action_block(options[i], NULL);
    }
}

static void back(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) ui_manager_pop_page();
}

static void choose(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || pending) return;
    uint8_t level = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    if (level == serial_number_state_level()) return;
    if (!serial_number_service_request(level != SERIAL_NUMBER_LEVEL_OFF, level)) {
        ui_notice_post_text(UI_NOTICE_WARNING,"settings.serial_number",UI_N_("Serial number"),UI_N_("Wait for the current setting result."));
        return;
    }
    if (!settings_detail_send_command(0x32, &level, 1)) {
        serial_number_service_cancel_request();
        ui_notice_post_text(UI_NOTICE_ERROR,"settings.serial_number",UI_N_("Serial number"),UI_N_("Request not sent. Try again."));
        return;
    }
    pending = true;
    ui_notice_post_text(UI_NOTICE_PROGRESS,"settings.serial_number",UI_N_("Serial number"),UI_N_("Waiting for controller..."));
    refresh();
}

void ui_page_25_set_serial_number_create(lv_obj_t *parent)
{
    if (frame.root) return;
    lv_settings_header_t header = {
        .title = ui_text_get(UI_TEXT_SETTINGS_SERIAL_LEVEL_TITLE), .icon = "ShieldCheck", .back = back
    };
    frame = lv_settings_frame_create(parent, &header);
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    lv_obj_t *row=lv_settings_panel(frame.body,0,0,1232,112);
    lv_settings_label(row,ui_tr("Serial number recognition"),24,25,&lv_font_instrument_sans_medium_22,0x1D2B34);
    lv_settings_label(row,ui_tr("Choose the recognition level; Off disables recognition."),24,61,&lv_font_instrument_sans_medium_14,0x586B78);
    lv_obj_t *base=lv_settings_segment_base(row,712,30,492,52);
    confirmed=lv_settings_label(frame.body,"",24,136,&lv_font_instrument_sans_medium_16,0x586B78);
    for(unsigned i=0;i<4;i++)
        options[i]=lv_settings_segment(base,i,4,ui_text_get(names[i]),choose,(void*)(uintptr_t)i);
    lv_label_set_text(frame.message, "");
    refresh();
}

void ui_page_25_set_serial_number_destroy(void)
{
    if (frame.root) lv_obj_del(frame.root);
    memset(&frame, 0, sizeof(frame));
    memset(options, 0, sizeof(options));
    confirmed = NULL;
}

void ui_page_25_set_serial_number_on_boot_setting(uint8_t level)
{
    (void)level;
    pending = false;
    refresh();
    if (frame.message) lv_label_set_text(frame.message, "");
}

void ui_page_25_set_serial_number_on_reply(uint8_t level, uint8_t res)
{
    (void)level;(void)res;
    pending = false;
    refresh();

}
