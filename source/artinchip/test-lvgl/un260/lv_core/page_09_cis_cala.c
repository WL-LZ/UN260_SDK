#include "un260/lv_core/page_09_cis_cala.h"

#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/app_clock.h"
#include "un260/diagnostic/diagnostic.h"
#include "un260/lv_system/ui_text.h"

#include <stdbool.h>

typedef struct {
    lv_obj_t* card;
    lv_obj_t* accent;
    lv_obj_t* icon_box;
    lv_obj_t* icon;
    lv_obj_t* status_dot;
    lv_obj_t* status;
    lv_obj_t* button;
} calib_panel_t;

static lv_obj_t* cis_page = NULL;
static calib_panel_t cis_panel = { 0 };
static calib_panel_t cb_panel = { 0 };

static lv_obj_t* calib_create_plain(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                    lv_coord_t w, lv_coord_t h, uint32_t color,
                                    lv_coord_t radius)
{
    lv_obj_t* obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static void calib_set_button_enabled(lv_obj_t* button, bool enabled)
{
    if (!button || !lv_obj_is_valid(button)) return;

    if (enabled) {
        lv_obj_clear_state(button, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(button, LV_STATE_DISABLED);
    }
}

static void calib_update_panel(calib_panel_t* panel, const char* text,
                               lv_color_t color, const char* symbol)
{
    if (!panel || !panel->card || !lv_obj_is_valid(panel->card)) return;

    lv_obj_set_style_border_color(panel->card, color, 0);
    lv_obj_set_style_bg_color(panel->accent, color, 0);
    lv_obj_set_style_bg_color(panel->icon_box, lv_color_mix(color, lv_color_white(), LV_OPA_20), 0);
    lv_obj_set_style_text_color(panel->icon, color, 0);
    lv_label_set_text(panel->icon, symbol);
    lv_obj_set_style_bg_color(panel->status_dot, color, 0);
    lv_label_set_text(panel->status, text);
    lv_obj_set_style_text_color(panel->status, color, 0);
}

static void cis_panel_refresh(cis_calib_state_t state)
{
    switch (state) {
    case CIS_CALIB_RUNNING:
        calib_update_panel(&cis_panel, ui_text_get(UI_TEXT_SETTINGS_CIS_STARTED),
                           lv_color_hex(0x1F6FE5), LV_SYMBOL_REFRESH);
        break;
    case CIS_CALIB_SUCCESS:
        calib_update_panel(&cis_panel, ui_text_get(UI_TEXT_SETTINGS_CIS_SUCCESS),
                           lv_color_hex(0x24B47E), LV_SYMBOL_OK);
        break;
    case CIS_CALIB_FAIL_UPPER:
        calib_update_panel(&cis_panel, ui_text_get(UI_TEXT_SETTINGS_CIS_FAIL_UPPER),
                           lv_color_hex(0xE5484D), LV_SYMBOL_CLOSE);
        break;
    case CIS_CALIB_FAIL_LOWER:
        calib_update_panel(&cis_panel, ui_text_get(UI_TEXT_SETTINGS_CIS_FAIL_LOWER),
                           lv_color_hex(0xE5484D), LV_SYMBOL_CLOSE);
        break;
    case CIS_CALIB_FAIL_IR:
        calib_update_panel(&cis_panel, ui_text_get(UI_TEXT_SETTINGS_CIS_FAIL_IR),
                           lv_color_hex(0xE5484D), LV_SYMBOL_CLOSE);
        break;
    case CIS_CALIB_IDLE:
    default:
        calib_update_panel(&cis_panel, ui_text_get(UI_TEXT_SETTINGS_CIS_IDLE),
                           lv_color_hex(0x7A8AA0), LV_SYMBOL_IMAGE);
        break;
    }
}

static void cb_panel_refresh(cb_calib_state_t state)
{
    switch (state) {
    case CB_CALIB_RUNNING:
        calib_update_panel(&cb_panel, ui_text_get(UI_TEXT_SETTINGS_CB_STARTED),
                           lv_color_hex(0x1F6FE5), LV_SYMBOL_REFRESH);
        break;
    case CB_CALIB_SUCCESS:
        calib_update_panel(&cb_panel, ui_text_get(UI_TEXT_SETTINGS_CB_SUCCESS),
                           lv_color_hex(0x24B47E), LV_SYMBOL_OK);
        break;
    case CB_CALIB_FAIL_IR:
        calib_update_panel(&cb_panel, ui_text_get(UI_TEXT_SETTINGS_CB_FAIL_IR),
                           lv_color_hex(0xE5484D), LV_SYMBOL_CLOSE);
        break;
    case CB_CALIB_IDLE:
    default:
        calib_update_panel(&cb_panel, ui_text_get(UI_TEXT_SETTINGS_CIS_IDLE),
                           lv_color_hex(0x7A8AA0), LV_SYMBOL_REFRESH);
        break;
    }
}

void cis_enter_btn_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_push_page(UI_PAGE_CIS_CALIB);
}

static void cis_esc_btn_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_pop_page();
}

static void cis_start_btn_cb(lv_event_t* e)
{
    calibration_state_snapshot_t state;
    uint8_t sub = 0x01;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    diagnostic_calibration_get_snapshot(&state);
    if (state.cis_state == CIS_CALIB_RUNNING ||
        state.cb_state == CB_CALIB_RUNNING) return;
    if (!diagnostic_calibration_begin(CALIB_TARGET_CIS,
                                      app_clock_uptime_ms())) return;
    if (!settings_detail_send_command(0x5B, &sub, 1)) {
        diagnostic_calibration_end_session();
        return;
    }

    cis_calib_ui_refresh();
}

static void cb_start_btn_cb(lv_event_t* e)
{
    calibration_state_snapshot_t state;
    uint8_t sub = 0x01;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    diagnostic_calibration_get_snapshot(&state);
    if (state.cis_state == CIS_CALIB_RUNNING ||
        state.cb_state == CB_CALIB_RUNNING) return;
    if (!diagnostic_calibration_begin(CALIB_TARGET_CB,
                                      app_clock_uptime_ms())) return;
    if (!settings_detail_send_command(0x5F, &sub, 1)) {
        diagnostic_calibration_end_session();
        return;
    }

    cis_calib_ui_refresh();
}

static void calib_create_panel(lv_obj_t* parent, calib_panel_t* panel,
                               lv_coord_t x, const char* title,
                               const char* symbol, lv_color_t button_color,
                               lv_event_cb_t button_cb)
{
    lv_obj_t* divider;
    lv_obj_t* status_band;
    lv_obj_t* title_label;

    panel->card = settings_detail_create_card(parent, x, 18, 580, 306);
    lv_obj_set_style_shadow_width(panel->card, 12, 0);
    lv_obj_set_style_shadow_opa(panel->card, LV_OPA_10, 0);

    panel->accent = calib_create_plain(panel->card, 0, 0, 6, 306, 0x2E85FF, 0);
    panel->icon_box = calib_create_plain(panel->card, 26, 22, 44, 44, 0xEAF3FF, 6);
    panel->icon = settings_detail_create_label(panel->icon_box, symbol,
                                               &lv_font_montserrat_20,
                                               lv_color_hex(0x1F6FE5), 0, 0);
    lv_obj_center(panel->icon);

    title_label = settings_detail_create_label(panel->card, title,
                                               &lv_font_instrument_sans_semibold_20,
                                               lv_color_hex(0x17223B), 86, 31);
    lv_obj_set_width(title_label, 450);

    divider = calib_create_plain(panel->card, 26, 82, 528, 1, 0xE4EBF5, 0);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE);

    status_band = calib_create_plain(panel->card, 26, 103, 528, 82, 0xF6F8FB, 6);
    lv_obj_set_style_border_width(status_band, 1, 0);
    lv_obj_set_style_border_color(status_band, lv_color_hex(0xE4EBF5), 0);

    panel->status_dot = calib_create_plain(status_band, 24, 35, 12, 12, 0x7A8AA0, 6);
    lv_obj_align(panel->status_dot, LV_ALIGN_LEFT_MID, 24, 0);
    panel->status = settings_detail_create_label(status_band,
                                                 ui_text_get(UI_TEXT_SETTINGS_CIS_IDLE),
                                                 &lv_font_instrument_sans_medium_18,
                                                 lv_color_hex(0x7A8AA0), 54, 27);
    lv_obj_set_width(panel->status, 440);
    lv_label_set_long_mode(panel->status, LV_LABEL_LONG_DOT);
    lv_obj_align(panel->status, LV_ALIGN_LEFT_MID, 54, 0);

    panel->button = settings_detail_create_button(panel->card, 26, 218, 528, 58,
                                                  title, button_color,
                                                  button_cb, NULL);
    lv_obj_set_style_opa(panel->button, LV_OPA_50, LV_STATE_DISABLED);
}

void ui_page_cis_calib_create(lv_obj_t* parent)
{
    lv_obj_t* content = NULL;

    if (cis_page) return;

    cis_page = settings_detail_create_page(parent,
                                           ui_text_get(UI_TEXT_SETTINGS_CIS_CALIBRATION),
                                           cis_esc_btn_cb, &content);

    calib_create_panel(content, &cis_panel, 38,
                       ui_text_get(UI_TEXT_SETTINGS_CIS_CALIBRATION),
                       LV_SYMBOL_IMAGE, lv_color_hex(0x1F6FE5),
                       cis_start_btn_cb);
    calib_create_panel(content, &cb_panel, 662,
                       ui_text_get(UI_TEXT_SETTINGS_COLOR_BALANCE),
                       LV_SYMBOL_REFRESH, lv_color_hex(0x24B47E),
                       cb_start_btn_cb);
    cis_calib_ui_refresh();
}

void ui_page_cis_calib_destroy(void)
{
    if (!cis_page) return;

    lv_obj_del(cis_page);
    cis_page = NULL;
    cis_panel = (calib_panel_t){ 0 };
    cb_panel = (calib_panel_t){ 0 };
    diagnostic_calibration_end_session();
}

void cis_calib_ui_refresh(void)
{
    calibration_state_snapshot_t state;
    bool running;

    if (!cis_page || !lv_obj_is_valid(cis_page)) return;

    diagnostic_calibration_get_snapshot(&state);
    cis_panel_refresh(state.cis_state);
    cb_panel_refresh(state.cb_state);
    if (state.timed_out) {
        if (state.target == CALIB_TARGET_CB) {
            calib_update_panel(&cb_panel,
                               ui_text_get(UI_TEXT_SETTINGS_CALIB_TIMEOUT),
                               lv_color_hex(0xE5484D), LV_SYMBOL_CLOSE);
        } else {
            calib_update_panel(&cis_panel,
                               ui_text_get(UI_TEXT_SETTINGS_CALIB_TIMEOUT),
                               lv_color_hex(0xE5484D), LV_SYMBOL_CLOSE);
        }
    }

    running = state.cis_state == CIS_CALIB_RUNNING ||
              state.cb_state == CB_CALIB_RUNNING;
    calib_set_button_enabled(cis_panel.button, !running);
    calib_set_button_enabled(cb_panel.button, !running);
}
