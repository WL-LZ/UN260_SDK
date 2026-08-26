#include "page_30_set_factory.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_core/ui_upgrade_service.h"
#include "un260/lv_system/ui_text.h"
#include "un260/app_service/setting_service.h"

#include <stdbool.h>
#include <stdint.h>

#define FACTORY_BLOCK_COUNT 5

static lv_obj_t* factory_page = NULL;
static lv_obj_t* sweep_line = NULL;
static lv_obj_t* blocks[FACTORY_BLOCK_COUNT] = { NULL };
static lv_timer_t* factory_anim_timer = NULL;
static lv_timer_t* factory_reboot_timer = NULL;
static uint16_t anim_tick = 0;

static bool factory_send_clear_detail(void)
{
    uint8_t payload = 0x01;

    return settings_detail_send_command(0x3B, &payload, 1);
}

static void factory_confirm_start(void* user_data)
{
    (void)user_data;
    (void)setting_service_request_factory_reset();
}

static void factory_reboot_timer_cb(lv_timer_t* timer)
{
    (void)timer;
    factory_reboot_timer = NULL;
    ui_upgrade_service_reboot();
}

static void factory_confirm_reboot(void* user_data)
{
    (void)user_data;

    (void)factory_send_clear_detail();

    if (factory_reboot_timer) {
        lv_timer_del(factory_reboot_timer);
    }

    factory_reboot_timer = lv_timer_create(factory_reboot_timer_cb, 1000, NULL);
    if (factory_reboot_timer) {
        lv_timer_set_repeat_count(factory_reboot_timer, 1);
    }
}

static void factory_start_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    settings_detail_dialog_show(ui_text_get(UI_TEXT_SETTINGS_FACTORY_CONFIRM_TITLE),
                                ui_text_get(UI_TEXT_SETTINGS_FACTORY_CONFIRM_CONTENT),
                                ui_text_get(UI_TEXT_SETTINGS_DIALOG_CONFIRM),
                                ui_text_get(UI_TEXT_SETTINGS_DIALOG_CANCEL),
                                factory_confirm_start, NULL, NULL);
}

static void factory_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    settings_detail_dialog_hide();
    ui_manager_pop_page();
}

static lv_obj_t* factory_create_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                     lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* card = settings_detail_create_card(parent, x, y, w, h);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    return card;
}

static void factory_create_panel(lv_obj_t* parent)
{
    lv_obj_t* card = factory_create_card(parent, 38, 18, 730, 306);
    lv_obj_t* accent;
    lv_obj_t* hint;

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_FACTORY_PANEL),
                                 &lv_font_instrument_sans_medium_16, lv_color_hex(0x0D3440), 24, 20);

    accent = lv_obj_create(card);
    lv_obj_remove_style_all(accent);
    lv_obj_set_pos(accent, 314, 26);
    lv_obj_set_size(accent, 102, 8);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x08C5D6), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(accent, 4, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* start_btn = settings_detail_create_button(card, 250, 118, 230, 74,
                                                        ui_text_get(UI_TEXT_SETTINGS_FACTORY_START),
                                                        lv_color_hex(0xD95757),
                                                        factory_start_cb, NULL);
    lv_obj_set_style_radius(start_btn, 6, 0);
    lv_obj_set_style_shadow_width(start_btn, 14, 0);
    lv_obj_set_style_shadow_color(start_btn, lv_color_hex(0xD95757), 0);

    hint = settings_detail_create_label(card,
                                        ui_text_get(UI_TEXT_SETTINGS_FACTORY_CONFIRM_CONTENT),
                                        &lv_font_instrument_sans_medium_14,
                                        lv_color_hex(0x5686A5), 168, 220);
    lv_obj_set_width(hint, 394);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
}

static lv_obj_t* factory_create_scene_obj(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
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

static void factory_anim_timer_cb(lv_timer_t* timer)
{
    uint16_t phase;

    (void)timer;

    anim_tick = (uint16_t)(anim_tick + 1);
    phase = (uint16_t)(anim_tick % 120);

    if (sweep_line && lv_obj_is_valid(sweep_line)) {
        lv_obj_set_x(sweep_line, (lv_coord_t)(58 + phase * 210 / 120));
        lv_obj_set_style_shadow_opa(sweep_line, phase < 60 ? LV_OPA_50 : LV_OPA_30, 0);
    }

    for (uint8_t i = 0; i < FACTORY_BLOCK_COUNT; i++) {
        uint16_t local = (uint16_t)((phase + i * 18) % 120);
        if (!blocks[i] || !lv_obj_is_valid(blocks[i])) continue;

        lv_obj_set_style_bg_color(blocks[i],
                                  local < 56 ? lv_color_hex(0x08C5D6) : lv_color_hex(0xF4A24C),
                                  0);
        lv_obj_set_style_bg_opa(blocks[i], local < 56 ? LV_OPA_50 : LV_OPA_20, 0);
    }
}

static void factory_anim_start(void)
{
    if (factory_anim_timer) return;
    factory_anim_timer = lv_timer_create(factory_anim_timer_cb, 42, NULL);
}

static void factory_anim_stop(void)
{
    if (!factory_anim_timer) return;
    lv_timer_del(factory_anim_timer);
    factory_anim_timer = NULL;
}

static void factory_create_preview(lv_obj_t* parent)
{
    lv_obj_t* card = settings_detail_create_card(parent, 820, 18, 370, 306);
    lv_obj_t* header;
    lv_obj_t* scene;

    lv_obj_set_style_shadow_width(card, 8, 0);

    header = lv_obj_create(card);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 370, 42);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x08C5D6), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    settings_detail_create_label(header, ui_text_get(UI_TEXT_SETTINGS_FACTORY_PREVIEW),
                                 &lv_font_instrument_sans_medium_18, lv_color_hex(0xFFFFFF), 150, 12);

    scene = lv_obj_create(card);
    lv_obj_remove_style_all(scene);
    lv_obj_set_pos(scene, 35, 58);
    lv_obj_set_size(scene, 300, 208);
    lv_obj_set_style_bg_color(scene, lv_color_hex(0x142332), 0);
    lv_obj_set_style_bg_opa(scene, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scene, 1, 0);
    lv_obj_set_style_border_color(scene, lv_color_hex(0x24465C), 0);
    lv_obj_set_style_radius(scene, 8, 0);
    lv_obj_clear_flag(scene, LV_OBJ_FLAG_SCROLLABLE);

    factory_create_scene_obj(scene, 46, 54, 208, 118, 0x12C8DD, LV_OPA_10);
    factory_create_scene_obj(scene, 66, 72, 168, 20, 0x12C8DD, LV_OPA_20);
    factory_create_scene_obj(scene, 66, 104, 168, 20, 0x12C8DD, LV_OPA_20);
    factory_create_scene_obj(scene, 66, 136, 168, 20, 0x12C8DD, LV_OPA_20);

    for (uint8_t i = 0; i < FACTORY_BLOCK_COUNT; i++) {
        blocks[i] = factory_create_scene_obj(scene,
                                             (lv_coord_t)(66 + i * 34), 176, 24, 8,
                                             0x08C5D6, LV_OPA_40);
        lv_obj_set_style_radius(blocks[i], 4, 0);
    }

    settings_detail_create_label(scene, LV_SYMBOL_REFRESH,
                                 &lv_font_montserrat_24, lv_color_hex(0xFFFFFF), 138, 100);

    sweep_line = lv_obj_create(scene);
    lv_obj_remove_style_all(sweep_line);
    lv_obj_set_pos(sweep_line, 58, 48);
    lv_obj_set_size(sweep_line, 3, 128);
    lv_obj_set_style_bg_color(sweep_line, lv_color_hex(0x62E6FF), 0);
    lv_obj_set_style_bg_opa(sweep_line, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(sweep_line, 18, 0);
    lv_obj_set_style_shadow_color(sweep_line, lv_color_hex(0x62E6FF), 0);
    lv_obj_set_style_shadow_opa(sweep_line, LV_OPA_30, 0);
    lv_obj_clear_flag(sweep_line, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* label = settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_FACTORY_IDLE),
                                                   &lv_font_instrument_sans_medium_16,
                                                   lv_color_hex(0x5686A5), 0, 276);
    lv_obj_set_width(label, 370);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

void ui_page_30_set_factory_create(lv_obj_t* parent)
{
    lv_obj_t* content = NULL;

    if (factory_page) return;

    factory_page = settings_detail_create_page(parent,
                                               ui_text_get(UI_TEXT_SETTINGS_FACTORY_TITLE),
                                               factory_esc_cb, &content);

    factory_create_panel(content);
    factory_create_preview(content);
    factory_anim_start();
}

void ui_page_30_set_factory_destroy(void)
{
    settings_detail_dialog_hide();
    factory_anim_stop();

    if (factory_page && lv_obj_is_valid(factory_page)) {
        lv_obj_del(factory_page);
    }

    factory_page = NULL;
    sweep_line = NULL;
    anim_tick = 0;
    for (uint8_t i = 0; i < FACTORY_BLOCK_COUNT; i++) {
        blocks[i] = NULL;
    }
}

void ui_page_30_set_factory_on_reply(uint8_t res)
{
    if (!factory_page || !lv_obj_is_valid(factory_page)) {
        return;
    }

    if (res == 0x01) {
        settings_detail_dialog_show(ui_text_get(UI_TEXT_SETTINGS_FACTORY_SUCCESS_TITLE),
                                    ui_text_get(UI_TEXT_SETTINGS_FACTORY_SUCCESS_CONTENT),
                                    ui_text_get(UI_TEXT_SETTINGS_DIALOG_CONFIRM),
                                    NULL, factory_confirm_reboot, NULL, NULL);
        return;
    }

    settings_detail_dialog_show(ui_text_get(UI_TEXT_SETTINGS_FACTORY_FAIL_TITLE),
                                ui_text_get(UI_TEXT_SETTINGS_FACTORY_FAIL_CONTENT),
                                ui_text_get(UI_TEXT_SETTINGS_DIALOG_CONFIRM),
                                NULL, NULL, NULL, NULL);
}
