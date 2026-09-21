#include "page_23_set_flap.h"
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
static lv_obj_t *options[2], *confirmed;
static bool pending;
static const ui_text_id_t names[] = { UI_TEXT_SETTINGS_FLAP_UP, UI_TEXT_SETTINGS_FLAP_DOWN };
static const uint8_t positions[] = { FLAP_POSITION_UP, FLAP_POSITION_DOWN };

static void refresh(void)
{
    if (!frame.root) return;
    uint8_t position = machine_state_flap_position();
    const char *name = position == FLAP_POSITION_UP ? ui_text_get(names[0]) :
                       position == FLAP_POSITION_DOWN ? ui_text_get(names[1]) : "Unknown";
    char text[128];
    lv_snprintf(text,sizeof(text),"Confirmed position: %s",name);
    if(strcmp(lv_label_get_text(confirmed),text))lv_label_set_text(confirmed,text);
    for (unsigned i = 0; i < 2; ++i) {
        bool selected = position == positions[i];
        if (selected) lv_obj_add_state(options[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(options[i], LV_STATE_CHECKED);
        if (pending) settings_detail_action_block(options[i], "Wait for the current flap movement to be confirmed.");
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
    uint8_t target = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    uint8_t previous = machine_state_flap_position();
    if (target == previous) return;
    if (!setting_service_request_flap_position(target, previous)) {
        lv_label_set_text(frame.message, "Could not send the command. Please try again.");
        return;
    }
    pending = true;
    /* Keep the footer stable during short ACK round trips; selection is locked. */
    refresh();
}

void ui_page_23_set_flap_create(lv_obj_t *parent)
{
    if (frame.root) return;
    lv_settings_header_t header = {
        .title = ui_text_get(UI_TEXT_SETTINGS_FLAP_TITLE), .icon = "Settings", .back = back
    };
    frame = lv_settings_frame_create(parent, &header);
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    lv_obj_t *row=lv_settings_panel(frame.body,0,0,1232,112);
    lv_settings_label(row,"Flap position",24,25,&lv_font_instrument_sans_medium_22,0x1D2B34);
    lv_settings_label(row,"Keep the note path clear when moving the flap.",24,61,&lv_font_instrument_sans_medium_14,0x586B78);
    lv_obj_t *base=lv_settings_segment_base(row,766,30,438,52);
    confirmed = lv_settings_label(frame.body, "", 24,136,
        &lv_font_instrument_sans_medium_16, 0x586B78);
    for (unsigned i = 0; i < 2; ++i) {
        options[i]=lv_settings_segment(base,i,2,ui_text_get(names[i]),choose,(void *)(uintptr_t)positions[i]);
    }
    lv_label_set_text(frame.message, pending ? "Waiting for controller." : "Select a position to move the flap.");
    refresh();
}

void ui_page_23_set_flap_destroy(void)
{
    if (frame.root) lv_obj_del(frame.root);
    memset(&frame, 0, sizeof(frame));
    memset(options, 0, sizeof(options));
    confirmed = NULL;
}

void ui_page_23_set_flap_on_reply(const setting_value_result_t *result)
{
    if (!result) return;
    pending = false;
    refresh();
    const char *message=result->success ? "Position confirmed." :
        result->timeout ? "No confirmation received." : "Controller rejected the command.";
    if (frame.message&&strcmp(lv_label_get_text(frame.message),message))lv_label_set_text(frame.message,message);
}
