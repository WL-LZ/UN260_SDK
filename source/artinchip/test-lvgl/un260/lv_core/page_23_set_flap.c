#include "page_23_set_flap.h"
#include "un260/app_service/setting_service.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"

#include <stddef.h>
#include <stdint.h>

#define FLAP_ITEM_W 326
#define FLAP_ITEM_H 66
#define FLAP_ITEM_GAP_X 30
#define FLAP_OPTION_COUNT 2
#define FLAP_PREVIEW_UP_Y 86
#define FLAP_PREVIEW_DOWN_Y 190

typedef struct {
    uint8_t position;
    ui_text_id_t title_text;
    uint32_t color_hex;
} flap_option_t;

typedef struct {
    lv_obj_t* card;
    lv_obj_t* check;
} flap_item_t;

static const flap_option_t g_flap_options[FLAP_OPTION_COUNT] = {
    { FLAP_POSITION_UP, UI_TEXT_SETTINGS_FLAP_UP, 0x24B47E },
    { FLAP_POSITION_DOWN, UI_TEXT_SETTINGS_FLAP_DOWN, 0x0878C8 },
};

static lv_obj_t* flap_page = NULL;
static lv_obj_t* preview_flap = NULL;
static lv_obj_t* preview_flap_label = NULL;
static flap_item_t g_flap_items[FLAP_OPTION_COUNT] = { 0 };
static uint8_t display_position = FLAP_POSITION_UP;

static uint8_t flap_normalize_position(uint8_t position)
{
    if (position == FLAP_POSITION_DOWN) {
        return FLAP_POSITION_DOWN;
    }

    return FLAP_POSITION_UP;
}

static uint8_t flap_get_position(void)
{
    return flap_normalize_position(machine_state_flap_position());
}

static const flap_option_t* flap_find_option(uint8_t position)
{
    uint8_t normalized = flap_normalize_position(position);

    for (size_t i = 0; i < FLAP_OPTION_COUNT; i++) {
        if (g_flap_options[i].position == normalized) {
            return &g_flap_options[i];
        }
    }

    return &g_flap_options[0];
}

static int flap_option_index(uint8_t position)
{
    uint8_t normalized = flap_normalize_position(position);

    for (size_t i = 0; i < FLAP_OPTION_COUNT; i++) {
        if (g_flap_options[i].position == normalized) {
            return (int)i;
        }
    }

    return 0;
}

static lv_coord_t flap_preview_y(uint8_t position)
{
    return flap_normalize_position(position) == FLAP_POSITION_DOWN ?
           FLAP_PREVIEW_DOWN_Y : FLAP_PREVIEW_UP_Y;
}

static void flap_preview_anim_y_cb(void* obj, int32_t value)
{
    lv_obj_set_y((lv_obj_t*)obj, (lv_coord_t)value);
}

static void flap_move_preview(uint8_t position, bool animate)
{
    const flap_option_t* option = flap_find_option(position);
    lv_coord_t target_y = flap_preview_y(position);

    if (preview_flap) {
        lv_anim_del(preview_flap, flap_preview_anim_y_cb);
        lv_obj_set_style_bg_color(preview_flap, lv_color_hex(option->color_hex), 0);
        if (animate) {
            lv_anim_t anim;
            lv_anim_init(&anim);
            lv_anim_set_var(&anim, preview_flap);
            lv_anim_set_exec_cb(&anim, flap_preview_anim_y_cb);
            lv_anim_set_values(&anim, lv_obj_get_y(preview_flap), target_y);
            lv_anim_set_time(&anim, 260);
            lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
            lv_anim_start(&anim);
        } else {
            lv_obj_set_y(preview_flap, target_y);
        }
    }

    if (preview_flap_label) {
        lv_label_set_text(preview_flap_label, ui_text_get(option->title_text));
        lv_obj_center(preview_flap_label);
    }
}

static void flap_refresh_view(bool animate_preview)
{
    uint8_t position = flap_normalize_position(display_position);
    int selected_index = flap_option_index(position);

    for (size_t i = 0; i < FLAP_OPTION_COUNT; i++) {
        bool selected = ((int)i == selected_index);
        lv_obj_t* card = g_flap_items[i].card;

        settings_detail_set_select_box_checked(g_flap_items[i].check, selected);
        settings_detail_set_select_box_active(g_flap_items[i].check, selected);

        if (card) {
            lv_obj_set_style_bg_color(card,
                                      selected ? lv_color_hex(0xF2FBFF) : lv_color_hex(0xFFFFFF),
                                      0);
            lv_obj_set_style_border_color(card,
                                          selected ? lv_color_hex(0x0878C8) : lv_color_hex(0xDDE6EF),
                                          0);
            lv_obj_set_style_border_width(card, 2, 0);
        }
    }

    flap_move_preview(position, animate_preview);
}

static void flap_request_position(uint8_t position)
{
    uint8_t target_position = flap_normalize_position(position);
    uint8_t previous_position = flap_get_position();

    if (!setting_service_request_flap_position(target_position, previous_position)) return;

    display_position = target_position;
    flap_refresh_view(true);
}

static void flap_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_pop_page();
}

static void flap_option_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    flap_request_position((uint8_t)(uintptr_t)lv_event_get_user_data(e));
}

static void flap_preview_click_cb(lv_event_t* e)
{
    uint8_t position;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    position = flap_get_position() == FLAP_POSITION_UP ? FLAP_POSITION_DOWN : FLAP_POSITION_UP;
    flap_request_position(position);
}

static lv_obj_t* flap_create_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* card = settings_detail_create_card(parent, x, y, w, h);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    return card;
}

static void flap_create_option(lv_obj_t* parent, size_t index,
                               const flap_option_t* option)
{
    lv_coord_t x = (lv_coord_t)(index * (FLAP_ITEM_W + FLAP_ITEM_GAP_X));
    lv_color_t item_color = lv_color_hex(option->color_hex);
    lv_obj_t* item = lv_obj_create(parent);

    lv_obj_remove_style_all(item);
    lv_obj_set_pos(item, x, 0);
    lv_obj_set_size(item, FLAP_ITEM_W, FLAP_ITEM_H);
    lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(item, 2, 0);
    lv_obj_set_style_border_color(item, lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_radius(item, 6, 0);
    lv_obj_set_style_translate_y(item, 0, LV_STATE_PRESSED);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item, flap_option_cb, LV_EVENT_CLICKED,
                        (void*)(uintptr_t)option->position);

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

    lv_obj_t* badge_label = settings_detail_create_label(badge,
                                                         index == 0 ? "^" : "v",
                                                         &lv_font_instrument_sans_medium_18,
                                                         item_color, 0, 0);
    lv_obj_center(badge_label);

    settings_detail_create_label(item, ui_text_get(option->title_text), &lv_font_instrument_sans_semibold_16,
                                 lv_color_hex(0x0D3440), 86, 24);

    g_flap_items[index].card = item;
    g_flap_items[index].check = settings_detail_create_select_box(item, 278, 18, 30,
                                                                  flap_option_cb,
                                                                  (void*)(uintptr_t)option->position);
    lv_obj_set_style_translate_y(g_flap_items[index].check, 0, LV_STATE_PRESSED);
}

static void flap_create_list(lv_obj_t* parent)
{
    lv_obj_t* list_area = lv_obj_create(parent);

    lv_obj_remove_style_all(list_area);
    lv_obj_set_pos(list_area, 24, 64);
    lv_obj_set_size(list_area, 682, 220);
    lv_obj_set_style_bg_opa(list_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_area, 0, 0);
    lv_obj_set_scroll_dir(list_area, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_area, LV_SCROLLBAR_MODE_OFF);

    for (size_t i = 0; i < FLAP_OPTION_COUNT; i++) {
        flap_create_option(list_area, i, &g_flap_options[i]);
    }
}

static void flap_create_panel(lv_obj_t* parent)
{
    lv_obj_t* card = flap_create_card(parent, 38, 18, 730, 306);

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_FLAP_POSITION),
                                 &lv_font_instrument_sans_medium_16, lv_color_hex(0x0D3440), 24, 20);

    lv_obj_t* accent = lv_obj_create(card);
    lv_obj_remove_style_all(accent);
    lv_obj_set_pos(accent, 314, 26);
    lv_obj_set_size(accent, 102, 8);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x08C5D6), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(accent, 4, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);

    flap_create_list(card);
}

static void flap_create_preview(lv_obj_t* parent)
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

    settings_detail_create_label(header, ui_text_get(UI_TEXT_SETTINGS_FLAP_PREVIEW),
                                 &lv_font_instrument_sans_medium_18, lv_color_hex(0xFFFFFF), 150, 12);

    lv_obj_t* chamber = lv_obj_create(card);
    lv_obj_remove_style_all(chamber);
    lv_obj_set_pos(chamber, 56, 68);
    lv_obj_set_size(chamber, 258, 172);
    lv_obj_set_style_bg_color(chamber, lv_color_hex(0xF6FBFF), 0);
    lv_obj_set_style_bg_opa(chamber, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chamber, 2, 0);
    lv_obj_set_style_border_color(chamber, lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_radius(chamber, 8, 0);
    lv_obj_set_style_translate_y(chamber, 0, LV_STATE_PRESSED);
    lv_obj_clear_flag(chamber, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chamber, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(chamber, flap_preview_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* guide = lv_obj_create(card);
    lv_obj_remove_style_all(guide);
    lv_obj_set_pos(guide, 179, 84);
    lv_obj_set_size(guide, 12, 140);
    lv_obj_set_style_bg_color(guide, lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_bg_opa(guide, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(guide, 6, 0);
    lv_obj_clear_flag(guide, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(guide, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(guide, flap_preview_click_cb, LV_EVENT_CLICKED, NULL);

    preview_flap = lv_obj_create(card);
    lv_obj_remove_style_all(preview_flap);
    lv_obj_set_pos(preview_flap, 92, FLAP_PREVIEW_UP_Y);
    lv_obj_set_size(preview_flap, 186, 28);
    lv_obj_set_style_bg_color(preview_flap, lv_color_hex(0x24B47E), 0);
    lv_obj_set_style_bg_opa(preview_flap, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(preview_flap, 0, 0);
    lv_obj_set_style_radius(preview_flap, 6, 0);
    lv_obj_set_style_shadow_width(preview_flap, 14, 0);
    lv_obj_set_style_shadow_opa(preview_flap, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(preview_flap, 4, 0);
    lv_obj_clear_flag(preview_flap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(preview_flap, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(preview_flap, flap_preview_click_cb, LV_EVENT_CLICKED, NULL);

    preview_flap_label = settings_detail_create_label(preview_flap, "", &lv_font_instrument_sans_medium_16,
                                                      lv_color_hex(0xFFFFFF), 0, 0);
    lv_obj_center(preview_flap_label);
}

void ui_page_23_set_flap_create(lv_obj_t* parent)
{
    lv_obj_t* content = NULL;

    if (flap_page) return;

    display_position = flap_get_position();

    flap_page = settings_detail_create_page(parent,
                                            ui_text_get(UI_TEXT_SETTINGS_FLAP_TITLE),
                                            flap_esc_cb, &content);

    flap_create_panel(content);
    flap_create_preview(content);
    flap_refresh_view(false);
}

void ui_page_23_set_flap_destroy(void)
{
    if (flap_page && lv_obj_is_valid(flap_page)) {
        lv_obj_del(flap_page);
    }

    flap_page = NULL;
    preview_flap = NULL;
    preview_flap_label = NULL;

    for (size_t i = 0; i < FLAP_OPTION_COUNT; i++) {
        g_flap_items[i].card = NULL;
        g_flap_items[i].check = NULL;
    }
}

void ui_page_23_set_flap_on_reply(const setting_value_result_t* result)
{
    if (!result) return;

    if (!result->success) {
        display_position = flap_get_position();
    }

    if (!result->success && flap_page && lv_obj_is_valid(flap_page)) {
        flap_refresh_view(true);
    }
}
