#include "page_40_set_impeller.h"
#include "un260/app_service/setting_service.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_core/lv_page_manager.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "lv_port_indev.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static lv_settings_frame_t frame;
static lv_obj_t *slider, *value, *confirmed, *read_button, *save_button;
static uint8_t draft;
static bool edited;
static const char *read_message;

/* Keep legacy controller readback truthful; only the editable draft is bounded. */
static uint8_t editable_speed(uint8_t actual)
{
    return actual < IMPELLER_SPEED_MIN ? IMPELLER_SPEED_MIN :
        actual > IMPELLER_SPEED_MAX ? IMPELLER_SPEED_MAX : actual;
}

static void refresh(void)
{
    if (!frame.root) return;
    bool pending = setting_service_impeller_speed_is_pending();
    bool known = machine_state_impeller_speed_known();
    lv_label_set_text_fmt(lv_obj_get_child(value, 0), "%u", (unsigned)draft);
    lv_slider_set_value(slider, draft, LV_ANIM_OFF);
    if (known) lv_label_set_text_fmt(confirmed, ui_tr("Confirmed speed: %u"),
        (unsigned)machine_state_impeller_speed());
    else lv_label_set_text(confirmed, ui_tr("Speed not confirmed"));
    settings_detail_action_wait(read_button, pending ? UI_N_("Waiting for controller...") : NULL);
    settings_detail_action_wait(save_button, pending ? UI_N_("Waiting for controller...") : NULL);
    lv_label_set_text(frame.message, pending ? ui_tr("Waiting for controller...") :
        ui_tr(read_message ? read_message : UI_N_("Adjust, then save to apply.")));
}

static void edit_value(int target)
{
    if (target < IMPELLER_SPEED_MIN) target = IMPELLER_SPEED_MIN;
    if (target > IMPELLER_SPEED_MAX) target = IMPELLER_SPEED_MAX;
    draft = (uint8_t)target;
    edited = setting_service_impeller_speed_is_pending() ||
        !machine_state_impeller_speed_known() || draft != machine_state_impeller_speed();
    refresh();
}

static void slider_changed(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED)
        edit_value(lv_slider_get_value(slider));
}

static void step(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED)
        edit_value(draft + (int)(intptr_t)lv_event_get_user_data(event));
}

static void keyboard_done(const char *text, void *context)
{
    (void)context;
    if (!frame.root || !text || !*text) return;
    char *end;
    long target = strtol(text, &end, 10);
    if (*end || target < IMPELLER_SPEED_MIN || target > IMPELLER_SPEED_MAX) {
        ui_notice_post_text(UI_NOTICE_WARNING, "settings.impeller",
            UI_N_("Impeller motor speed"), UI_N_("Enter a value from 15 to 50."));
        return;
    }
    edit_value((int)target);
}

static void keyboard_open(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    char text[4];
    lv_snprintf(text, sizeof(text), "%u", (unsigned)draft);
    settings_detail_keyboard_show(ui_tr("Impeller motor speed"), text, 2,
        SETTINGS_DETAIL_KEYBOARD_UINT, keyboard_done, NULL);
}

static void request_failed(void)
{
    if (!setting_service_impeller_speed_is_pending())
        ui_notice_post_text(UI_NOTICE_ERROR, "settings.impeller",
            UI_N_("Impeller motor speed"), UI_N_("Request not sent. Try again."));
}

static void read_value(lv_event_t *event)
{
    if (event && lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    if (setting_service_impeller_speed_is_pending()) { refresh(); return; }
    read_message = NULL;
    if (!setting_service_request_impeller_speed_read())
        read_message = UI_N_("Request not sent. Try again.");
    refresh();
}

static void save(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED ||
        setting_service_impeller_speed_is_pending()) return;
    read_message = NULL;
    if (!setting_service_request_impeller_speed(draft)) request_failed();
    refresh();
}

static void defaults(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) edit_value(IMPELLER_SPEED_DEFAULT);
}

static void back(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    settings_detail_keyboard_hide();
    ui_manager_pop_page();
}

static void deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) != frame.root) return;
    memset(&frame, 0, sizeof(frame));
    slider = value = confirmed = read_button = save_button = NULL;
    edited = false;
    read_message = NULL;
}

void ui_page_40_set_impeller_create(lv_obj_t *parent)
{
    if (frame.root) return;
    draft = editable_speed(machine_state_impeller_speed());
    edited = false;
    read_message = NULL;
    lv_settings_header_t header = {
        .title = ui_tr("Impeller motor speed"), .icon = "EntryMotor", .back = back
    };
    frame = lv_settings_frame_create(parent, &header);
    lv_obj_add_event_cb(frame.root, deleted, LV_EVENT_DELETE, NULL);
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    lv_obj_t *panel = lv_settings_panel(frame.body, 0, 0, 1232, 236);
    lv_obj_t *heading = lv_settings_label(panel, ui_tr("Impeller motor speed"), 24, 0,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    lv_obj_t *range = lv_settings_label(panel, ui_tr("Range: 15-50. Default: 30."), 24, 0,
        &lv_font_instrument_sans_medium_14, 0x586B78);
    value = lv_settings_button(panel, 1060, 0, 144, 58, "", false, keyboard_open, NULL);
    lv_obj_set_style_text_font(lv_obj_get_child(value, 0),
        &lv_font_instrument_sans_semibold_28, 0);
    lv_settings_align_row(heading, range, value, 18, 70, 8);
    lv_settings_box(panel, 24, 106, 1180, 1, 0xE8EDF0);
    lv_settings_button(panel, 24, 132, 60, 52, "-", false, step, (void *)(intptr_t)-1);
    lv_settings_button(panel, 1144, 132, 60, 52, "+", false, step, (void *)(intptr_t)1);
    slider = lv_slider_create(panel);
    lv_port_indev_set_drag_obj(slider, true);
    lv_obj_set_pos(slider, 124, 151);
    lv_obj_set_size(slider, 980, 14);
    lv_obj_set_ext_click_area(slider, 18);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xE3E9ED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x1462CC), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x1462CC), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 10, LV_PART_KNOB);
    lv_slider_set_range(slider, IMPELLER_SPEED_MIN, IMPELLER_SPEED_MAX);
    lv_slider_set_value(slider, draft, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, slider_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_settings_label(panel, "15", 118, 198, &lv_font_instrument_sans_medium_14, 0x586B78);
    lv_settings_label(panel, "50", 1090, 198, &lv_font_instrument_sans_medium_14, 0x586B78);
    confirmed = lv_settings_label(frame.body, "", 24, 252,
        &lv_font_instrument_sans_medium_16, 0x536B79);
    lv_settings_button(frame.footer, 0, 0, 156, 46, ui_tr("Default"), false, defaults, NULL);
    read_button = lv_settings_button(frame.footer, 0, 0, 136, 46, ui_tr("Read"), false, read_value, NULL);
    save_button = lv_settings_button(frame.footer, 0, 0, 132, 46, ui_tr("Save"), true, save, NULL);
    refresh();
    read_value(NULL);
}

void ui_page_40_set_impeller_destroy(void)
{
    settings_detail_keyboard_hide();
    if (frame.root) lv_obj_del(frame.root);
}

void ui_page_40_set_impeller_on_update(const setting_impeller_result_t *result)
{
    if (!frame.root) return;
    read_message = result && result->reading && !result->success
        ? (result->timeout ? UI_N_("No response received. Check device status.")
                           : UI_N_("Could not read the setting. Try again."))
        : NULL;
    if (machine_state_impeller_speed_known()) {
        uint8_t actual = machine_state_impeller_speed();
        if (!edited) draft = editable_speed(actual);
        if (draft == actual) edited = false;
    }
    refresh();
}
