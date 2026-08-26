#include "page_27_set_cfd_level.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/currency/currency_state.h"
#include "un260/cfd/cfd.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CFD_CELL_COUNT (CFD_SCENE_COUNT * CFD_ITEM_COUNT)

typedef struct {
    lv_obj_t* card;
    lv_obj_t* check;
} cfd_scene_item_t;

static const ui_text_id_t g_cfd_scene_texts[CFD_SCENE_COUNT] = {
    UI_TEXT_SETTINGS_CFD_LEVEL_CUSTOM1,
    UI_TEXT_SETTINGS_CFD_LEVEL_CUSTOM2,
    UI_TEXT_SETTINGS_CFD_LEVEL_CUSTOM3,
};

static lv_obj_t* cfd_level_page = NULL;
static lv_obj_t* currency_label = NULL;
static lv_obj_t* level_cells[CFD_CELL_COUNT] = { NULL };
static lv_obj_t* level_cell_labels[CFD_CELL_COUNT] = { NULL };
static lv_obj_t* table_rows[CFD_SCENE_COUNT] = { NULL };
static lv_obj_t* table_row_labels[CFD_SCENE_COUNT] = { NULL };
static cfd_scene_item_t scene_items[CFD_SCENE_COUNT] = { 0 };
static uint8_t selected_scene = 0;
static cfd_state_value_t cfd_draft;
static bool cfd_draft_valid = false;

static uint8_t cfd_normalize_level(uint8_t level)
{
    if (level < CFD_LEVEL_MIN) return CFD_LEVEL_MIN;
    if (level > CFD_LEVEL_MAX) return CFD_LEVEL_MAX;
    return level;
}

static uint8_t cfd_cell_index(uint8_t scene, uint8_t item)
{
    return (uint8_t)(scene * CFD_ITEM_COUNT + item);
}

static lv_color_t cfd_level_color(uint8_t level)
{
    static const uint32_t colors[CFD_LEVEL_MAX] = {
        0x24B47E, 0x64B95A, 0xF59D2A, 0xF06A2A, 0xF04444,
    };

    level = cfd_normalize_level(level);
    return lv_color_hex(colors[level - 1]);
}

static void cfd_refresh_view(void)
{
    const cfd_state_value_t* config;

    if (!cfd_draft_valid) {
        cfd_state_get(&cfd_draft);
        cfd_draft_valid = true;
    }
    config = &cfd_draft;
    if (currency_label) {
        lv_label_set_text(currency_label, config->currency);
    }

    for (uint8_t scene = 0; scene < CFD_SCENE_COUNT; scene++) {
        bool selected = (scene == selected_scene);

        settings_detail_set_select_box_checked(scene_items[scene].check, selected);
        settings_detail_set_select_box_active(scene_items[scene].check, selected);

        if (scene_items[scene].card) {
            lv_obj_set_style_bg_color(scene_items[scene].card,
                                      selected ? lv_color_hex(0xF2FBFF) : lv_color_hex(0xFFFFFF),
                                      0);
            lv_obj_set_style_border_color(scene_items[scene].card,
                                          selected ? lv_color_hex(0x0878C8) : lv_color_hex(0xDDE6EF),
                                          0);
        }

        if (table_rows[scene]) {
            lv_obj_set_style_bg_color(table_rows[scene],
                                      selected ? lv_color_hex(0xE3F4FF) : lv_color_hex(0xFFFFFF),
                                      0);
        }

        if (table_row_labels[scene]) {
            lv_obj_set_style_text_color(table_row_labels[scene],
                                        selected ? lv_color_hex(0x075E9C) : lv_color_hex(0x0D3440),
                                        0);
        }
    }

    for (uint8_t scene = 0; scene < CFD_SCENE_COUNT; scene++) {
        for (uint8_t item = 0; item < CFD_ITEM_COUNT; item++) {
            uint8_t index = cfd_cell_index(scene, item);
            uint8_t level = cfd_normalize_level(config->levels[scene][item]);
            bool selected = (scene == selected_scene);
            lv_color_t color = cfd_level_color(level);

            if (level_cell_labels[index]) {
                lv_label_set_text_fmt(level_cell_labels[index], "%u", (unsigned)level);
                lv_obj_set_style_text_color(level_cell_labels[index],
                                            selected ? lv_color_hex(0xFFFFFF) : color,
                                            0);
            }

            if (level_cells[index]) {
                lv_obj_set_style_bg_color(level_cells[index],
                                          selected ? color : lv_color_hex(0xF8FAFC),
                                          0);
                lv_obj_set_style_border_color(level_cells[index],
                                              selected ? color : lv_color_hex(0xDDE6EF),
                                              0);
            }
        }
    }
}

bool ui_page_27_set_cfd_level_query(void)
{
    char query_currency[4];

    currency_state_get_active_code(query_currency);
    return cfd_service_request_query(query_currency);
}

static bool cfd_send_update(void)
{
    return cfd_draft_valid &&
           cfd_service_request_update(&cfd_draft, selected_scene);
}

static void cfd_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (cfd_service_busy()) return;
    ui_manager_pop_page();
}

static void cfd_scene_cb(lv_event_t* e)
{
    uint8_t scene;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    scene = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (scene >= CFD_SCENE_COUNT) return;

    selected_scene = scene;
    cfd_refresh_view();
}

static void cfd_level_cell_cb(lv_event_t* e)
{
    uint8_t index;
    uint8_t scene;
    uint8_t item;
    uint8_t level;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (cfd_service_busy()) return;

    index = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    scene = (uint8_t)(index / CFD_ITEM_COUNT);
    item = (uint8_t)(index % CFD_ITEM_COUNT);
    if (scene >= CFD_SCENE_COUNT || item >= CFD_ITEM_COUNT) return;

    if (scene != selected_scene) {
        selected_scene = scene;
        cfd_refresh_view();
        return;
    }

    if (!cfd_draft_valid) return;
    level = cfd_normalize_level(cfd_draft.levels[scene][item]);
    level = (uint8_t)(level >= CFD_LEVEL_MAX ? CFD_LEVEL_MIN : level + 1);
    cfd_draft.levels[scene][item] = level;
    cfd_refresh_view();
}

static void cfd_update_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    cfd_send_update();
}

static lv_obj_t* cfd_create_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                 lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* card = settings_detail_create_card(parent, x, y, w, h);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    return card;
}

static void cfd_create_scene_item(lv_obj_t* parent, uint8_t scene,
                                  lv_coord_t x, lv_coord_t y)
{
    lv_obj_t* item = lv_obj_create(parent);

    lv_obj_remove_style_all(item);
    lv_obj_set_pos(item, x, y);
    lv_obj_set_size(item, 286, 54);
    lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(item, 2, 0);
    lv_obj_set_style_border_color(item, lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_radius(item, 6, 0);
    lv_obj_set_style_translate_y(item, 0, LV_STATE_PRESSED);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item, cfd_scene_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)scene);

    scene_items[scene].check = settings_detail_create_select_box(item, 20, 12, 30,
                                                                 cfd_scene_cb,
                                                                 (void*)(uintptr_t)scene);
    lv_obj_set_style_translate_y(scene_items[scene].check, 0, LV_STATE_PRESSED);

    settings_detail_create_label(item, ui_text_get(g_cfd_scene_texts[scene]),
                                 &lv_font_instrument_sans_medium_18, lv_color_hex(0x0D3440), 70, 16);

    scene_items[scene].card = item;
}

static void cfd_create_left_panel(lv_obj_t* parent)
{
    lv_obj_t* card = cfd_create_card(parent, 38, 18, 340, 306);

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_CURRENCY),
                                 &lv_font_instrument_sans_medium_18, lv_color_hex(0x5686A5), 26, 28);
    currency_label = settings_detail_create_label(card, "", &lv_font_instrument_sans_medium_22,
                                                  lv_color_hex(0x0D3440), 128, 27);

    lv_obj_t* accent = lv_obj_create(card);
    lv_obj_remove_style_all(accent);
    lv_obj_set_pos(accent, 26, 66);
    lv_obj_set_size(accent, 286, 7);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x08C5D6), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(accent, 4, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t scene = 0; scene < CFD_SCENE_COUNT; scene++) {
        cfd_create_scene_item(card, scene, 26, (lv_coord_t)(98 + scene * 58));
    }
}

static lv_obj_t* cfd_create_table_label(lv_obj_t* parent, const char* text,
                                        lv_coord_t x, lv_coord_t y,
                                        lv_coord_t w)
{
    lv_obj_t* label = settings_detail_create_label(parent, text, &lv_font_instrument_sans_medium_14,
                                                   lv_color_hex(0x5686A5), x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static void cfd_create_level_cell(lv_obj_t* parent, uint8_t scene, uint8_t item,
                                  lv_coord_t x, lv_coord_t y)
{
    uint8_t index = cfd_cell_index(scene, item);
    lv_obj_t* cell = lv_obj_create(parent);

    lv_obj_remove_style_all(cell);
    lv_obj_set_pos(cell, x, y);
    lv_obj_set_size(cell, 64, 38);
    lv_obj_set_style_bg_color(cell, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_border_color(cell, lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_radius(cell, 6, 0);
    lv_obj_set_style_translate_y(cell, 0, LV_STATE_PRESSED);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cell, cfd_level_cell_cb, LV_EVENT_CLICKED,
                        (void*)(uintptr_t)index);

    level_cell_labels[index] = settings_detail_create_label(cell, "", &lv_font_instrument_sans_medium_20,
                                                            lv_color_hex(0x0878C8), 0, 0);
    lv_obj_center(level_cell_labels[index]);
    level_cells[index] = cell;
}

static void cfd_create_detail_panel(lv_obj_t* parent)
{
    lv_obj_t* card = cfd_create_card(parent, 406, 18, 784, 306);
    lv_obj_t* line;

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_DETAIL),
                                 &lv_font_instrument_sans_medium_18, lv_color_hex(0x0D3440), 30, 20);

    line = lv_obj_create(card);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 30, 50);
    lv_obj_set_size(line, 520, 2);
    lv_obj_set_style_bg_color(line, lv_color_hex(0xE9EDF2), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);

    cfd_create_table_label(card, ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_UV), 206, 72, 64);
    cfd_create_table_label(card, ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_MG), 328, 72, 64);
    cfd_create_table_label(card, ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_MT), 450, 72, 64);
    cfd_create_table_label(card, ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_IR), 572, 72, 64);

    for (uint8_t scene = 0; scene < CFD_SCENE_COUNT; scene++) {
        lv_coord_t row_top = (lv_coord_t)(98 + scene * 58);
        table_rows[scene] = lv_obj_create(card);
        lv_obj_remove_style_all(table_rows[scene]);
        lv_obj_set_pos(table_rows[scene], 24, row_top);
        lv_obj_set_size(table_rows[scene], 710, 54);
        lv_obj_set_style_bg_color(table_rows[scene], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(table_rows[scene], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(table_rows[scene], 6, 0);
        lv_obj_clear_flag(table_rows[scene], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(table_rows[scene], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(table_rows[scene], cfd_scene_cb, LV_EVENT_CLICKED,
                            (void*)(uintptr_t)scene);

        table_row_labels[scene] = settings_detail_create_label(card,
                                                               ui_text_get(g_cfd_scene_texts[scene]),
                                                               &lv_font_instrument_sans_medium_16,
                                                               lv_color_hex(0x0D3440),
                                                               44, (lv_coord_t)(row_top + 17));
        lv_obj_add_flag(table_row_labels[scene], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(table_row_labels[scene], cfd_scene_cb, LV_EVENT_CLICKED,
                            (void*)(uintptr_t)scene);

        for (uint8_t item = 0; item < CFD_ITEM_COUNT; item++) {
            cfd_create_level_cell(card, scene, item,
                                  (lv_coord_t)(206 + item * 122),
                                  (lv_coord_t)(row_top + 8));
        }
    }

    settings_detail_create_button(card, 624, 14, 128, 34,
                                  ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_UPDATE),
                                  lv_color_hex(0x0878C8), cfd_update_cb, NULL);
}

void ui_page_27_set_cfd_level_create(lv_obj_t* parent)
{
    lv_obj_t* content = NULL;

    if (cfd_level_page) return;

    selected_scene = 0;
    cfd_state_get(&cfd_draft);
    cfd_draft_valid = true;

    cfd_level_page = settings_detail_create_page(parent,
                                                 ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_TITLE),
                                                 cfd_esc_cb, &content);

    cfd_create_left_panel(content);
    cfd_create_detail_panel(content);
    cfd_refresh_view();
}

void ui_page_27_set_cfd_level_destroy(void)
{
    if (cfd_level_page && lv_obj_is_valid(cfd_level_page)) {
        lv_obj_del(cfd_level_page);
    }

    cfd_level_page = NULL;
    currency_label = NULL;
    selected_scene = 0;
    cfd_draft_valid = false;
    memset(&cfd_draft, 0, sizeof(cfd_draft));

    for (uint8_t i = 0; i < CFD_CELL_COUNT; i++) {
        level_cells[i] = NULL;
        level_cell_labels[i] = NULL;
    }
    for (uint8_t scene = 0; scene < CFD_SCENE_COUNT; scene++) {
        table_rows[scene] = NULL;
        table_row_labels[scene] = NULL;
        scene_items[scene].card = NULL;
        scene_items[scene].check = NULL;
    }
}

void ui_page_27_set_cfd_level_on_info(const uint8_t* data, uint16_t len)
{
    uint16_t pos = 4;
    cfd_state_value_t config;
    char response_currency[4];

    if (!data || len < 16) return;
    response_currency[0] = (char)data[0];
    response_currency[1] = (char)data[1];
    response_currency[2] = (char)data[2];
    response_currency[3] = '\0';
    memset(&config, 0, sizeof(config));
    memcpy(config.currency, response_currency, sizeof(config.currency));

    for (uint8_t scene = 0; scene < CFD_SCENE_COUNT; scene++) {
        for (uint8_t item = 0; item < CFD_ITEM_COUNT; item++) {
            config.levels[scene][item] = cfd_normalize_level(data[pos++]);
        }
    }

    if (!cfd_service_take_query_result(response_currency) &&
        !cfd_service_take_update_result(&config)) {
        return;
    }

    cfd_state_confirm(&config);

    if (cfd_level_page) {
        cfd_draft = config;
        cfd_draft_valid = true;
        if (data[3] >= 1 && data[3] <= CFD_SCENE_COUNT) {
            selected_scene = (uint8_t)(data[3] - 1);
        }
        cfd_refresh_view();
    }
}

void ui_page_27_set_cfd_level_on_request_failed(void)
{
    if (!cfd_level_page || !lv_obj_is_valid(cfd_level_page)) return;
    cfd_state_get(&cfd_draft);
    cfd_draft_valid = true;
    cfd_refresh_view();
}
