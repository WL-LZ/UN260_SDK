#include "page_24_set_reject_pocket.h"
#include "un260/app_service/setting_service.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define REJECT_LEVEL_COUNT 10
#define REJECT_BAR_W 232
#define REJECT_BAR_H 11
#define REJECT_BAR_GAP 5

static lv_obj_t* reject_page = NULL;
static lv_obj_t* value_box = NULL;
static lv_obj_t* value_label = NULL;
static lv_obj_t* preview_value_label = NULL;
static lv_obj_t* preview_unit_label = NULL;
static lv_obj_t* preview_level_label = NULL;
static lv_obj_t* level_bars[REJECT_LEVEL_COUNT] = { NULL };
static uint8_t display_capacity = REJECT_POCKET_MIN_CAPACITY;

static uint8_t reject_normalize_capacity(uint8_t capacity)
{
    if (capacity < REJECT_POCKET_MIN_CAPACITY) {
        return REJECT_POCKET_MIN_CAPACITY;
    }
    if (capacity > REJECT_POCKET_MAX_CAPACITY) {
        return REJECT_POCKET_MAX_CAPACITY;
    }

    return capacity;
}

static uint8_t reject_get_capacity(void)
{
    return reject_normalize_capacity(machine_state_reject_pocket_max());
}

static uint8_t reject_level_from_capacity(uint8_t capacity)
{
    uint8_t normalized = reject_normalize_capacity(capacity);
    uint8_t level = normalized / 10;

    if (level < 1) level = 1;
    if (level > REJECT_LEVEL_COUNT) level = REJECT_LEVEL_COUNT;
    return level;
}

static uint8_t reject_capacity_from_level(uint8_t level)
{
    uint16_t capacity;

    if (level < 1) level = 1;
    if (level > REJECT_LEVEL_COUNT) level = REJECT_LEVEL_COUNT;

    capacity = (uint16_t)level * 10U;
    if (capacity < REJECT_POCKET_MIN_CAPACITY) {
        capacity = REJECT_POCKET_MIN_CAPACITY;
    }

    return (uint8_t)capacity;
}

static lv_color_t reject_level_color(uint8_t level)
{
    static const uint32_t colors[REJECT_LEVEL_COUNT] = {
        0x23B26D, 0x48BA5F, 0x72C451, 0xA7CA3E, 0xD7C736,
        0xF0AD2F, 0xF58B28, 0xF36B23, 0xE74D25, 0xD73333,
    };

    if (level < 1) level = 1;
    if (level > REJECT_LEVEL_COUNT) level = REJECT_LEVEL_COUNT;
    return lv_color_hex(colors[level - 1]);
}

static void reject_refresh_view(void)
{
    uint8_t capacity = reject_normalize_capacity(display_capacity);
    uint8_t level = reject_level_from_capacity(capacity);

    if (value_label) {
        lv_label_set_text_fmt(value_label, "%u", (unsigned)capacity);
        lv_obj_center(value_label);
    }

    if (preview_value_label) {
        lv_label_set_text_fmt(preview_value_label, "%u", (unsigned)capacity);
    }

    if (preview_unit_label) {
        lv_label_set_text(preview_unit_label, ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_PCS));
    }

    if (preview_level_label) {
        lv_label_set_text_fmt(preview_level_label,
                              ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_LEVEL_FMT),
                              (int)level);
        lv_obj_set_style_text_color(preview_level_label, reject_level_color(level), 0);
    }

    for (uint8_t i = 0; i < REJECT_LEVEL_COUNT; i++) {
        bool active = (i < level);

        if (!level_bars[i]) continue;

        lv_obj_set_style_bg_color(level_bars[i],
                                  active ? reject_level_color((uint8_t)(i + 1)) :
                                  lv_color_hex(0xEDF2F6),
                                  0);
        lv_obj_set_style_border_color(level_bars[i],
                                      active ? reject_level_color((uint8_t)(i + 1)) :
                                      lv_color_hex(0xDDE6EF),
                                      0);
    }
}

static void reject_request_capacity(uint8_t capacity)
{
    uint8_t normalized = reject_normalize_capacity(capacity);
    uint8_t previous = reject_get_capacity();

    if (!setting_service_request_reject_pocket_max(normalized, previous)) return;

    display_capacity = normalized;
    reject_refresh_view();
}

static void reject_keyboard_done(const char* value, void* user_data)
{
    long input_value;

    (void)user_data;

    if (!value || value[0] == '\0') {
        return;
    }

    input_value = strtol(value, NULL, 10);
    if (input_value < 0) {
        input_value = 0;
    }
    if (input_value > 255) {
        input_value = 255;
    }

    reject_request_capacity((uint8_t)input_value);
}

static void reject_value_cb(lv_event_t* e)
{
    char value[8];

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    lv_snprintf(value, sizeof(value), "%u", (unsigned)reject_get_capacity());
    settings_detail_keyboard_show(ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_CAPACITY),
                                  value, 3, SETTINGS_DETAIL_KEYBOARD_NUM,
                                  reject_keyboard_done, NULL);
}

static void reject_bar_cb(lv_event_t* e)
{
    uint8_t level;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    level = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    reject_request_capacity(reject_capacity_from_level(level));
}

static void reject_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    settings_detail_keyboard_hide();
    ui_manager_pop_page();
}

static lv_obj_t* reject_create_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                    lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* card = settings_detail_create_card(parent, x, y, w, h);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    return card;
}

static void reject_create_panel(lv_obj_t* parent)
{
    lv_obj_t* card = reject_create_card(parent, 38, 18, 730, 306);

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_CAPACITY),
                                 &lv_font_instrument_sans_medium_16, lv_color_hex(0x0D3440), 24, 20);

    lv_obj_t* accent = lv_obj_create(card);
    lv_obj_remove_style_all(accent);
    lv_obj_set_pos(accent, 314, 26);
    lv_obj_set_size(accent, 102, 8);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x08C5D6), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(accent, 4, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);

    value_box = lv_obj_create(card);
    lv_obj_remove_style_all(value_box);
    lv_obj_set_pos(value_box, 232, 116);
    lv_obj_set_size(value_box, 266, 76);
    lv_obj_set_style_bg_color(value_box, lv_color_hex(0xF6FBFF), 0);
    lv_obj_set_style_bg_opa(value_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(value_box, 2, 0);
    lv_obj_set_style_border_color(value_box, lv_color_hex(0x0878C8), 0);
    lv_obj_set_style_radius(value_box, 8, 0);
    lv_obj_set_style_translate_y(value_box, 0, LV_STATE_PRESSED);
    lv_obj_clear_flag(value_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(value_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(value_box, reject_value_cb, LV_EVENT_CLICKED, NULL);

    value_label = settings_detail_create_label(value_box, "", &lv_font_manrope_bold_40,
                                               lv_color_hex(0x0D3440), 0, 0);
    lv_obj_t* unit = settings_detail_create_label(value_box,
                                                  ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_PCS),
                                                  &lv_font_instrument_sans_medium_18,
                                                  lv_color_hex(0x5686A5), 196, 27);
    lv_obj_clear_flag(unit, LV_OBJ_FLAG_CLICKABLE);

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_RANGE_HINT),
                                 &lv_font_instrument_sans_medium_14, lv_color_hex(0x5686A5), 232, 210);
}

static void reject_create_preview(lv_obj_t* parent)
{
    lv_obj_t* card = settings_detail_create_card(parent, 820, 18, 370, 306);
    lv_obj_set_style_shadow_width(card, 8, 0);

    lv_obj_t* header = lv_obj_create(card);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 370, 42);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x08C5D6), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    settings_detail_create_label(header, ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_PREVIEW),
                                 &lv_font_instrument_sans_medium_18, lv_color_hex(0xFFFFFF), 150, 12);

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_CAPACITY),
                                 &lv_font_instrument_sans_medium_14, lv_color_hex(0x5686A5), 30, 62);

    preview_level_label = settings_detail_create_label(card, "", &lv_font_instrument_sans_medium_14,
                                                       lv_color_hex(0xF36B23), 248, 62);

    lv_obj_t* stack = lv_obj_create(card);
    lv_obj_remove_style_all(stack);
    lv_obj_set_pos(stack, 70, 88);
    lv_obj_set_size(stack, 232, 160);
    lv_obj_set_style_bg_opa(stack, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(stack, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0; i < REJECT_LEVEL_COUNT; i++) {
        uint8_t level = (uint8_t)(REJECT_LEVEL_COUNT - i);
        lv_coord_t y = (lv_coord_t)(i * (REJECT_BAR_H + REJECT_BAR_GAP));
        lv_obj_t* bar = lv_obj_create(stack);

        lv_obj_remove_style_all(bar);
        lv_obj_set_pos(bar, 0, y);
        lv_obj_set_size(bar, REJECT_BAR_W, REJECT_BAR_H);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0xEDF2F6), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bar, 1, 0);
        lv_obj_set_style_border_color(bar, lv_color_hex(0xDDE6EF), 0);
        lv_obj_set_style_radius(bar, 4, 0);
        lv_obj_set_style_translate_y(bar, 0, LV_STATE_PRESSED);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(bar, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(bar, reject_bar_cb, LV_EVENT_CLICKED,
                            (void*)(uintptr_t)level);

        level_bars[level - 1] = bar;
    }

    preview_value_label = settings_detail_create_label(card, "", &lv_font_manrope_bold_42,
                                                       lv_color_hex(0x0D3440), 30, 252);
    preview_unit_label = settings_detail_create_label(card, "", &lv_font_instrument_sans_medium_18,
                                                      lv_color_hex(0x5686A5), 112, 276);
}

void ui_page_24_set_reject_pocket_create(lv_obj_t* parent)
{
    lv_obj_t* content = NULL;

    if (reject_page) return;

    display_capacity = reject_get_capacity();

    reject_page = settings_detail_create_page(parent,
                                              ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_TITLE),
                                              reject_esc_cb, &content);

    reject_create_panel(content);
    reject_create_preview(content);
    reject_refresh_view();
}

void ui_page_24_set_reject_pocket_destroy(void)
{
    settings_detail_keyboard_hide();

    if (reject_page && lv_obj_is_valid(reject_page)) {
        lv_obj_del(reject_page);
    }

    reject_page = NULL;
    value_box = NULL;
    value_label = NULL;
    preview_value_label = NULL;
    preview_unit_label = NULL;
    preview_level_label = NULL;

    for (uint8_t i = 0; i < REJECT_LEVEL_COUNT; i++) {
        level_bars[i] = NULL;
    }
}

void ui_page_24_set_reject_pocket_on_boot_setting(void)
{
    setting_service_clear_reject_pocket_max_request();
    display_capacity = reject_get_capacity();

    if (reject_page && lv_obj_is_valid(reject_page)) {
        reject_refresh_view();
    }
}

void ui_page_24_set_reject_pocket_on_reply(const setting_value_result_t* result)
{
    if (!result) return;

    if (!result->success) {
        display_capacity = reject_get_capacity();
    }

    if (!result->success && reject_page && lv_obj_is_valid(reject_page)) {
        reject_refresh_view();
    }
}
