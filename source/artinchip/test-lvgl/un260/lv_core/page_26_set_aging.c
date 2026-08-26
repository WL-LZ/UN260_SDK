#include "page_26_set_aging.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_system/ui_text.h"
#include "un260/app_service/setting_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AGING_DOT_COUNT 5
#define AGING_SCAN_TOP 88
#define AGING_SCAN_BOTTOM 222

static lv_obj_t* aging_page = NULL;
static lv_obj_t* start_btn = NULL;
static lv_obj_t* start_label = NULL;
static lv_obj_t* status_label = NULL;
static lv_obj_t* scan_line = NULL;
static lv_obj_t* data_dots[AGING_DOT_COUNT] = { NULL };
static lv_timer_t* aging_anim_timer = NULL;
static uint16_t anim_tick = 0;

static void aging_refresh_view(void);

static void aging_anim_timer_cb(lv_timer_t* timer)
{
    uint16_t phase;
    lv_coord_t scan_y;

    (void)timer;

    if (!machine_state_aging_running()) return;

    anim_tick = (uint16_t)(anim_tick + 1);
    phase = (uint16_t)(anim_tick % 96);
    scan_y = (lv_coord_t)(AGING_SCAN_TOP +
                          (phase < 48 ? phase : 96 - phase) *
                          (AGING_SCAN_BOTTOM - AGING_SCAN_TOP) / 48);

    if (scan_line && lv_obj_is_valid(scan_line)) {
        lv_obj_set_y(scan_line, scan_y);
        lv_obj_set_style_shadow_opa(scan_line, phase < 48 ? LV_OPA_50 : LV_OPA_30, 0);
    }

    for (uint8_t i = 0; i < AGING_DOT_COUNT; i++) {
        uint16_t dot_phase = (uint16_t)((anim_tick * 4 + i * 22) % 180);
        if (!data_dots[i] || !lv_obj_is_valid(data_dots[i])) continue;

        lv_obj_set_x(data_dots[i], (lv_coord_t)(42 + dot_phase));
        lv_obj_set_style_opa(data_dots[i],
                             dot_phase < 24 || dot_phase > 156 ? LV_OPA_30 : LV_OPA_COVER,
                             0);
    }
}

static void aging_anim_start(void)
{
    if (aging_anim_timer) {
        lv_timer_resume(aging_anim_timer);
        return;
    }

    aging_anim_timer = lv_timer_create(aging_anim_timer_cb, 36, NULL);
}

static void aging_anim_stop(void)
{
    if (aging_anim_timer) {
        lv_timer_del(aging_anim_timer);
        aging_anim_timer = NULL;
    }

    if (scan_line && lv_obj_is_valid(scan_line)) {
        lv_obj_set_y(scan_line, AGING_SCAN_TOP);
        lv_obj_set_style_shadow_opa(scan_line, LV_OPA_20, 0);
    }
}

static void aging_confirm_start(void* user_data)
{
    (void)user_data;

    if (machine_state_aging_running()) return;

    if (!setting_service_request_aging_start()) {
        return;
    }
}

static void aging_start_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (machine_state_aging_running()) return;

    settings_detail_dialog_show(ui_text_get(UI_TEXT_SETTINGS_AGING_CONFIRM_TITLE),
                                ui_text_get(UI_TEXT_SETTINGS_AGING_CONFIRM_CONTENT),
                                ui_text_get(UI_TEXT_SETTINGS_DIALOG_CONFIRM),
                                ui_text_get(UI_TEXT_SETTINGS_DIALOG_CANCEL),
                                aging_confirm_start, NULL, NULL);
}

static void aging_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    settings_detail_dialog_hide();
    ui_manager_pop_page();
}

static lv_obj_t* aging_create_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                   lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* card = settings_detail_create_card(parent, x, y, w, h);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    return card;
}

static void aging_create_panel(lv_obj_t* parent)
{
    lv_obj_t* card = aging_create_card(parent, 38, 18, 730, 306);

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_AGING_PANEL),
                                 &lv_font_instrument_sans_medium_16, lv_color_hex(0x0D3440), 24, 20);

    lv_obj_t* accent = lv_obj_create(card);
    lv_obj_remove_style_all(accent);
    lv_obj_set_pos(accent, 314, 26);
    lv_obj_set_size(accent, 102, 8);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x08C5D6), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(accent, 4, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);

    start_btn = settings_detail_create_button(card, 250, 118, 230, 74,
                                              ui_text_get(UI_TEXT_SETTINGS_AGING_START),
                                              lv_color_hex(0x0878C8),
                                              aging_start_cb, NULL);
    lv_obj_set_style_radius(start_btn, 6, 0);
    lv_obj_set_style_shadow_width(start_btn, 14, 0);

    start_label = lv_obj_get_child(start_btn, 0);

    lv_obj_t* hint = settings_detail_create_label(card,
                                                  ui_text_get(UI_TEXT_SETTINGS_AGING_CONFIRM_CONTENT),
                                                  &lv_font_instrument_sans_medium_14,
                                                  lv_color_hex(0x5686A5), 182, 220);
    lv_obj_set_width(hint, 370);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
}

static lv_obj_t* aging_create_scene_obj(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                        lv_coord_t w, lv_coord_t h,
                                        uint32_t color_hex, uint8_t opa)
{
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color_hex), 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(color_hex), 0);
    lv_obj_set_style_radius(obj, 4, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static void aging_create_preview(lv_obj_t* parent)
{
    lv_obj_t* card = settings_detail_create_card(parent, 820, 18, 370, 306);
    lv_obj_t* scene;

    lv_obj_set_style_shadow_width(card, 8, 0);

    lv_obj_t* header = lv_obj_create(card);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 370, 42);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x08C5D6), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    settings_detail_create_label(header, ui_text_get(UI_TEXT_SETTINGS_AGING_PREVIEW),
                                 &lv_font_instrument_sans_medium_18, lv_color_hex(0xFFFFFF), 150, 12);

    scene = lv_obj_create(card);
    lv_obj_remove_style_all(scene);
    lv_obj_set_pos(scene, 35, 58);
    lv_obj_set_size(scene, 300, 208);
    lv_obj_set_style_bg_color(scene, lv_color_hex(0x102233), 0);
    lv_obj_set_style_bg_opa(scene, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scene, 1, 0);
    lv_obj_set_style_border_color(scene, lv_color_hex(0x164865), 0);
    lv_obj_set_style_radius(scene, 8, 0);
    lv_obj_clear_flag(scene, LV_OBJ_FLAG_SCROLLABLE);

    aging_create_scene_obj(scene, 48, 74, 204, 92, 0x11BFE0, LV_OPA_10);
    aging_create_scene_obj(scene, 66, 54, 168, 34, 0x11BFE0, LV_OPA_10);
    aging_create_scene_obj(scene, 72, 112, 156, 18, 0x11BFE0, LV_OPA_10);
    aging_create_scene_obj(scene, 88, 144, 36, 36, 0x11BFE0, LV_OPA_10);
    aging_create_scene_obj(scene, 176, 144, 36, 36, 0x11BFE0, LV_OPA_10);

    for (uint8_t i = 0; i < AGING_DOT_COUNT; i++) {
        data_dots[i] = lv_obj_create(scene);
        lv_obj_remove_style_all(data_dots[i]);
        lv_obj_set_pos(data_dots[i], (lv_coord_t)(42 + i * 32), (lv_coord_t)(38 + i * 11));
        lv_obj_set_size(data_dots[i], 7, 7);
        lv_obj_set_style_bg_color(data_dots[i], lv_color_hex(0x62E6FF), 0);
        lv_obj_set_style_bg_opa(data_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(data_dots[i], 4, 0);
        lv_obj_set_style_shadow_width(data_dots[i], 8, 0);
        lv_obj_set_style_shadow_color(data_dots[i], lv_color_hex(0x62E6FF), 0);
        lv_obj_clear_flag(data_dots[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    scan_line = lv_obj_create(card);
    lv_obj_remove_style_all(scan_line);
    lv_obj_set_pos(scan_line, 58, AGING_SCAN_TOP);
    lv_obj_set_size(scan_line, 254, 3);
    lv_obj_set_style_bg_color(scan_line, lv_color_hex(0x62E6FF), 0);
    lv_obj_set_style_bg_opa(scan_line, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(scan_line, 18, 0);
    lv_obj_set_style_shadow_color(scan_line, lv_color_hex(0x62E6FF), 0);
    lv_obj_set_style_shadow_opa(scan_line, LV_OPA_20, 0);
    lv_obj_clear_flag(scan_line, LV_OBJ_FLAG_SCROLLABLE);

    status_label = settings_detail_create_label(card, "", &lv_font_instrument_sans_medium_16,
                                                lv_color_hex(0x0878C8), 0, 276);
    lv_obj_set_width(status_label, 370);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
}

static void aging_refresh_view(void)
{
    bool running = machine_state_aging_running();

    if (start_btn && lv_obj_is_valid(start_btn)) {
        lv_obj_set_style_bg_color(start_btn,
                                  running ? lv_color_hex(0x6B7A90) : lv_color_hex(0x0878C8),
                                  0);
    }

    if (start_label && lv_obj_is_valid(start_label)) {
        lv_label_set_text(start_label,
                          running ? ui_text_get(UI_TEXT_SETTINGS_AGING_RUNNING) :
                          ui_text_get(UI_TEXT_SETTINGS_AGING_START));
        lv_obj_center(start_label);
    }

    if (status_label && lv_obj_is_valid(status_label)) {
        lv_label_set_text(status_label,
                          running ? ui_text_get(UI_TEXT_SETTINGS_AGING_RUNNING) :
                          ui_text_get(UI_TEXT_SETTINGS_AGING_IDLE));
        lv_obj_set_style_text_color(status_label,
                                    running ? lv_color_hex(0x0878C8) : lv_color_hex(0x5686A5),
                                    0);
    }

    if (running) {
        aging_anim_start();
    } else {
        aging_anim_stop();
    }
}

void ui_page_26_set_aging_create(lv_obj_t* parent)
{
    lv_obj_t* content = NULL;

    if (aging_page) return;

    aging_page = settings_detail_create_page(parent,
                                             ui_text_get(UI_TEXT_SETTINGS_AGING_TITLE),
                                             aging_esc_cb, &content);

    aging_create_panel(content);
    aging_create_preview(content);
    aging_refresh_view();
}

void ui_page_26_set_aging_destroy(void)
{
    settings_detail_dialog_hide();
    aging_anim_stop();

    if (aging_page && lv_obj_is_valid(aging_page)) {
        lv_obj_del(aging_page);
    }

    aging_page = NULL;
    start_btn = NULL;
    start_label = NULL;
    status_label = NULL;
    scan_line = NULL;
    anim_tick = 0;

    for (uint8_t i = 0; i < AGING_DOT_COUNT; i++) {
        data_dots[i] = NULL;
    }
}

void ui_page_26_set_aging_on_reply(uint8_t res)
{
    if (res == 0x00) {
        machine_state_confirm_aging_running(true);
        if (aging_page) {
            aging_refresh_view();
        }
        return;
    }

    if (res == 0x02) {
        machine_state_confirm_aging_running(false);
        if (aging_page) {
            aging_refresh_view();
        }
        if (aging_page) {
            settings_detail_dialog_show(ui_text_get(UI_TEXT_SETTINGS_AGING_COMPLETE_TITLE),
                                        ui_text_get(UI_TEXT_SETTINGS_AGING_COMPLETE_CONTENT),
                                        ui_text_get(UI_TEXT_SETTINGS_DIALOG_CONFIRM),
                                        NULL, NULL, NULL, NULL);
        }
        return;
    }

    if (res == 0x01) {
        machine_state_confirm_aging_running(false);
        if (aging_page) {
            aging_refresh_view();
        }
        if (aging_page) {
            settings_detail_dialog_show(ui_text_get(UI_TEXT_SETTINGS_AGING_FAIL_TITLE),
                                        ui_text_get(UI_TEXT_SETTINGS_AGING_FAIL_CONTENT),
                                        ui_text_get(UI_TEXT_SETTINGS_DIALOG_CONFIRM),
                                        NULL, NULL, NULL, NULL);
        }
    }
}
