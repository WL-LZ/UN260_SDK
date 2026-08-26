#include "page_25_set_serial_number.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/serial_number/serial_number.h"

#include <stddef.h>
#include <stdint.h>

#define SERIAL_ITEM_W 326
#define SERIAL_ITEM_H 66
#define SERIAL_ITEM_GAP_X 30
#define SERIAL_ITEM_GAP_Y 10
#define SERIAL_OPTION_COUNT 4

typedef struct {
    uint8_t display_no;
    uint8_t level;
    ui_text_id_t title_text;
    uint32_t color_hex;
} serial_level_option_t;

typedef struct {
    lv_obj_t* card;
    lv_obj_t* check;
} serial_level_item_t;

static const serial_level_option_t g_serial_options[SERIAL_OPTION_COUNT] = {
    { 0, SERIAL_NUMBER_LEVEL_OFF, UI_TEXT_SETTINGS_SERIAL_LEVEL_OFF, 0x9AA6B2 },
    { 1, 0x01, UI_TEXT_SETTINGS_SERIAL_LEVEL_1, 0x24B47E },
    { 2, 0x02, UI_TEXT_SETTINGS_SERIAL_LEVEL_2, 0xF59D2A },
    { 3, 0x03, UI_TEXT_SETTINGS_SERIAL_LEVEL_3, 0xF04444 },
};

static lv_obj_t* serial_page = NULL;
static lv_obj_t* preview_level_label = NULL;
static lv_obj_t* preview_desc_label = NULL;
static lv_obj_t* preview_bars[SERIAL_OPTION_COUNT] = { NULL };
static lv_obj_t* preview_bar_labels[SERIAL_OPTION_COUNT] = { NULL };
static serial_level_item_t g_serial_items[SERIAL_OPTION_COUNT] = { 0 };

static uint8_t serial_level_normalize(uint8_t level)
{
    return serial_number_state_normalize_level(level);
}

static uint8_t serial_level_get(void)
{
    return serial_level_normalize(serial_number_state_level());
}

static const serial_level_option_t* serial_level_find_option(uint8_t level)
{
    uint8_t normalized = serial_level_normalize(level);

    for (size_t i = 0; i < SERIAL_OPTION_COUNT; i++) {
        if (g_serial_options[i].level == normalized) {
            return &g_serial_options[i];
        }
    }

    return &g_serial_options[0];
}

static int serial_level_option_index(uint8_t level)
{
    uint8_t normalized = serial_level_normalize(level);

    for (size_t i = 0; i < SERIAL_OPTION_COUNT; i++) {
        if (g_serial_options[i].level == normalized) {
            return (int)i;
        }
    }

    return 0;
}

static void serial_level_refresh_view(uint8_t level)
{
    const serial_level_option_t* option = serial_level_find_option(level);
    int selected_index = serial_level_option_index(level);
    lv_color_t active_color = lv_color_hex(option->color_hex);

    for (size_t i = 0; i < SERIAL_OPTION_COUNT; i++) {
        bool selected = ((int)i == selected_index);
        lv_obj_t* card = g_serial_items[i].card;

        settings_detail_set_select_box_checked(g_serial_items[i].check, selected);
        settings_detail_set_select_box_active(g_serial_items[i].check, selected);

        if (card) {
            lv_obj_set_style_bg_color(card,
                                      selected ? lv_color_hex(0xF2FBFF) : lv_color_hex(0xFFFFFF),
                                      0);
            lv_obj_set_style_border_color(card,
                                          selected ? lv_color_hex(0x0878C8) : lv_color_hex(0xDDE6EF),
                                          0);
            lv_obj_set_style_border_width(card, 2, 0);
        }

        if (preview_bars[i]) {
            bool enabled = (g_serial_options[i].level <= level);
            lv_color_t bar_color = enabled ? active_color : lv_color_hex(0xE9EDF2);

            lv_obj_set_style_bg_color(preview_bars[i], bar_color, 0);
            lv_obj_set_style_border_color(preview_bars[i],
                                          enabled ? bar_color : lv_color_hex(0xDDE6EF),
                                          0);
        }

        if (preview_bar_labels[i]) {
            lv_obj_set_style_text_color(preview_bar_labels[i],
                                        selected ? lv_color_hex(g_serial_options[i].color_hex) :
                                        lv_color_hex(0x5686A5),
                                        0);
        }
    }

    if (preview_level_label) {
        lv_label_set_text_fmt(preview_level_label, "%u", (unsigned)option->display_no);
        lv_obj_set_style_text_color(preview_level_label, active_color, 0);
    }

    if (preview_desc_label) {
        lv_label_set_text(preview_desc_label, ui_text_get(option->title_text));
        lv_obj_set_style_text_color(preview_desc_label, active_color, 0);
    }
}

static bool serial_level_send(uint8_t level)
{
    uint8_t payload = serial_level_normalize(level);

    return settings_detail_send_command(0x32, &payload, 1);
}

static void serial_level_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_pop_page();
}

static void serial_level_option_cb(lv_event_t* e)
{
    uint8_t level;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    level = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    level = serial_level_normalize(level);
    if (!serial_number_service_request(level != SERIAL_NUMBER_LEVEL_OFF, level)) return;
    if (!serial_level_send(level)) {
        serial_number_service_cancel_request();
        return;
    }

    serial_level_refresh_view(level);
}

static lv_obj_t* serial_level_create_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                          lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* card = settings_detail_create_card(parent, x, y, w, h);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    return card;
}

static void serial_level_create_option(lv_obj_t* parent, size_t index,
                                       const serial_level_option_t* option)
{
    lv_coord_t col = (lv_coord_t)(index % 2);
    lv_coord_t row = (lv_coord_t)(index / 2);
    lv_coord_t x = (lv_coord_t)(col * (SERIAL_ITEM_W + SERIAL_ITEM_GAP_X));
    lv_coord_t y = (lv_coord_t)(row * (SERIAL_ITEM_H + SERIAL_ITEM_GAP_Y));
    lv_color_t item_color = lv_color_hex(option->color_hex);
    lv_obj_t* item = lv_obj_create(parent);

    lv_obj_remove_style_all(item);
    lv_obj_set_pos(item, x, y);
    lv_obj_set_size(item, SERIAL_ITEM_W, SERIAL_ITEM_H);
    lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(item, 2, 0);
    lv_obj_set_style_border_color(item, lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_radius(item, 6, 0);
    lv_obj_set_style_bg_color(item, lv_color_hex(0xF6FBFF), LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(item, 0, LV_STATE_PRESSED);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item, serial_level_option_cb, LV_EVENT_CLICKED,
                        (void*)(uintptr_t)option->level);

    lv_obj_t* badge = lv_obj_create(item);
    lv_obj_remove_style_all(badge);
    lv_obj_set_pos(badge, 22, 12);
    lv_obj_set_size(badge, 42, 42);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0xF6FBFF), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(badge, 1, 0);
    lv_obj_set_style_border_color(badge, item_color, 0);
    lv_obj_set_style_radius(badge, 6, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* badge_label = settings_detail_create_label(badge, "", &lv_font_instrument_sans_medium_20,
                                                         item_color, 0, 0);
    lv_label_set_text_fmt(badge_label, "%u", (unsigned)option->display_no);
    lv_obj_center(badge_label);

    settings_detail_create_label(item, ui_text_get(option->title_text), &lv_font_instrument_sans_semibold_16,
                                 lv_color_hex(0x0D3440), 86, 24);

    g_serial_items[index].card = item;
    g_serial_items[index].check = settings_detail_create_select_box(item, 278, 18, 30,
                                                                    serial_level_option_cb,
                                                                    (void*)(uintptr_t)option->level);
    lv_obj_set_style_translate_y(g_serial_items[index].check, 0, LV_STATE_PRESSED);
}

static void serial_level_create_list(lv_obj_t* parent)
{
    lv_obj_t* list_area = lv_obj_create(parent);

    lv_obj_remove_style_all(list_area);
    lv_obj_set_pos(list_area, 24, 64);
    lv_obj_set_size(list_area, 682, 220);
    lv_obj_set_style_bg_opa(list_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_area, 0, 0);
    lv_obj_set_scroll_dir(list_area, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_area, LV_SCROLLBAR_MODE_OFF);

    for (size_t i = 0; i < SERIAL_OPTION_COUNT; i++) {
        serial_level_create_option(list_area, i, &g_serial_options[i]);
    }
}

static void serial_level_create_panel(lv_obj_t* parent)
{
    lv_obj_t* card = serial_level_create_card(parent, 38, 18, 730, 306);

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_SERIAL_LEVEL_DISPLAY),
                                 &lv_font_instrument_sans_medium_16, lv_color_hex(0x0D3440), 24, 20);

    lv_obj_t* accent = lv_obj_create(card);
    lv_obj_remove_style_all(accent);
    lv_obj_set_pos(accent, 314, 26);
    lv_obj_set_size(accent, 102, 8);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x08C5D6), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(accent, 4, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);

    serial_level_create_list(card);
}

static void serial_level_create_preview(lv_obj_t* parent)
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

    settings_detail_create_label(header, ui_text_get(UI_TEXT_SETTINGS_SERIAL_LEVEL_PREVIEW),
                                 &lv_font_instrument_sans_medium_18, lv_color_hex(0xFFFFFF), 150, 12);

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_SERIAL_LEVEL_DISPLAY),
                                 &lv_font_instrument_sans_medium_16, lv_color_hex(0x5686A5), 34, 62);

    preview_level_label = settings_detail_create_label(card, "", &lv_font_instrument_sans_medium_48,
                                                       lv_color_hex(0x0878C8), 34, 90);

    for (size_t i = 0; i < SERIAL_OPTION_COUNT; i++) {
        lv_coord_t h = (lv_coord_t)(44 + i * 20);
        lv_coord_t x = (lv_coord_t)(50 + i * 70);
        lv_coord_t y = (lv_coord_t)(224 - h);

        preview_bars[i] = lv_obj_create(card);
        lv_obj_remove_style_all(preview_bars[i]);
        lv_obj_set_pos(preview_bars[i], x, y);
        lv_obj_set_size(preview_bars[i], 48, h);
        lv_obj_set_style_bg_color(preview_bars[i], lv_color_hex(0xE9EDF2), 0);
        lv_obj_set_style_bg_opa(preview_bars[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(preview_bars[i], 1, 0);
        lv_obj_set_style_border_color(preview_bars[i], lv_color_hex(0xDDE6EF), 0);
        lv_obj_set_style_radius(preview_bars[i], 6, 0);
        lv_obj_set_style_translate_y(preview_bars[i], 0, LV_STATE_PRESSED);
        lv_obj_clear_flag(preview_bars[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(preview_bars[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(preview_bars[i], serial_level_option_cb, LV_EVENT_CLICKED,
                            (void*)(uintptr_t)g_serial_options[i].level);

        preview_bar_labels[i] = settings_detail_create_label(card, "", &lv_font_instrument_sans_medium_16,
                                                             lv_color_hex(0x5686A5),
                                                             (lv_coord_t)(x + 18), 238);
        lv_label_set_text_fmt(preview_bar_labels[i], "%u",
                              (unsigned)g_serial_options[i].display_no);
    }

    preview_desc_label = settings_detail_create_label(card, "", &lv_font_instrument_sans_bold_16,
                                                      lv_color_hex(0x0878C8), 0, 276);
    lv_obj_set_width(preview_desc_label, 370);
    lv_obj_set_style_text_align(preview_desc_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(preview_desc_label, LV_LABEL_LONG_CLIP);
}

void ui_page_25_set_serial_number_create(lv_obj_t* parent)
{
    lv_obj_t* content = NULL;

    if (serial_page) return;

    serial_page = settings_detail_create_page(parent,
                                              ui_text_get(UI_TEXT_SETTINGS_SERIAL_LEVEL_TITLE),
                                              serial_level_esc_cb, &content);

    serial_level_create_panel(content);
    serial_level_create_preview(content);

    serial_level_refresh_view(serial_level_get());
}

void ui_page_25_set_serial_number_destroy(void)
{
    if (serial_page && lv_obj_is_valid(serial_page)) {
        lv_obj_del(serial_page);
    }

    serial_page = NULL;
    preview_level_label = NULL;
    preview_desc_label = NULL;

    for (size_t i = 0; i < SERIAL_OPTION_COUNT; i++) {
        g_serial_items[i].card = NULL;
        g_serial_items[i].check = NULL;
        preview_bars[i] = NULL;
        preview_bar_labels[i] = NULL;
    }
}

void ui_page_25_set_serial_number_on_boot_setting(uint8_t level)
{
    (void)level;

    if (serial_page) {
        serial_level_refresh_view(serial_level_get());
    }
}

void ui_page_25_set_serial_number_on_reply(uint8_t level, uint8_t res)
{
    (void)level;
    (void)res;

    if (serial_page) {
        serial_level_refresh_view(serial_level_get());
    }
}
