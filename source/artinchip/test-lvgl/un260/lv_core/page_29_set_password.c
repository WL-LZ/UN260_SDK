#include "page_29_set_password.h"
#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum {
    PASSWORD_FIELD_CURRENT = 0,
    PASSWORD_FIELD_NEW,
    PASSWORD_FIELD_CONFIRM,
    PASSWORD_FIELD_COUNT,
} password_field_t;

static lv_obj_t* password_setting_page = NULL;
static lv_obj_t* field_cards[PASSWORD_FIELD_COUNT] = { NULL };
static lv_obj_t* field_values[PASSWORD_FIELD_COUNT] = { NULL };
static char field_text[PASSWORD_FIELD_COUNT][USER_PASSWORD_MAX_LEN + 1] = { 0 };
static password_field_t active_field = PASSWORD_FIELD_CURRENT;

static const ui_text_id_t field_titles[PASSWORD_FIELD_COUNT] = {
    UI_TEXT_PASSWORD_CURRENT,
    UI_TEXT_PASSWORD_NEW,
    UI_TEXT_PASSWORD_CONFIRM_NEW,
};

static void password_setting_refresh_fields(void)
{
    for (uint8_t i = 0; i < PASSWORD_FIELD_COUNT; i++) {
        bool active = (i == active_field);
        char masked[USER_PASSWORD_MAX_LEN + 1];
        size_t len = strlen(field_text[i]);

        if (len > USER_PASSWORD_MAX_LEN) len = USER_PASSWORD_MAX_LEN;
        for (size_t j = 0; j < len; j++) {
            masked[j] = '*';
        }
        masked[len] = '\0';

        if (field_cards[i]) {
            lv_obj_set_style_bg_color(field_cards[i],
                                      active ? lv_color_hex(0xF2FBFF) : lv_color_hex(0xFFFFFF),
                                      0);
            lv_obj_set_style_border_color(field_cards[i],
                                          active ? lv_color_hex(0x0878C8) : lv_color_hex(0xDDE6EF),
                                          0);
        }
        if (field_values[i]) {
            lv_label_set_text(field_values[i],
                              len > 0 ? masked : ui_text_get(UI_TEXT_PASSWORD_PLACEHOLDER));
            lv_obj_set_style_text_color(field_values[i],
                                        len > 0 ? lv_color_hex(0x0D3440) : lv_color_hex(0x8AA8B8),
                                        0);
        }
    }
}

static void password_setting_show_toast(ui_text_id_t text_id, bool alarm)
{
    lv_print_toast_config_t cfg = lv_print_toast_get_default_config();

    cfg.w = 320;
    cfg.h = 92;
    cfg.text = ui_text_get(text_id);
    cfg.show_loader = false;
    cfg.align_center = true;
    cfg.text_font = &lv_font_instrument_sans_medium_18;
    cfg.loader_color = alarm ? lv_color_hex(0xC03A2B) : lv_color_hex(0x24B47E);
    cfg.auto_hide_ms = 1600;
    lv_print_toast_show_with_config(&cfg);
}

static void password_setting_keyboard_cb(const char* value, void* user_data)
{
    password_field_t field = (password_field_t)(uintptr_t)user_data;

    if (field >= PASSWORD_FIELD_COUNT || !value) return;

    lv_snprintf(field_text[field], sizeof(field_text[field]), "%s", value);
    password_setting_refresh_fields();
}

static void password_setting_open_keyboard(password_field_t field)
{
    active_field = field;
    password_setting_refresh_fields();
    settings_detail_keyboard_show(ui_text_get(field_titles[field]),
                                  field_text[field],
                                  USER_PASSWORD_MAX_LEN,
                                  SETTINGS_DETAIL_KEYBOARD_NUM,
                                  password_setting_keyboard_cb,
                                  (void*)(uintptr_t)field);
}

static void password_setting_field_cb(lv_event_t* e)
{
    password_field_t field;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    field = (password_field_t)(uintptr_t)lv_event_get_user_data(e);
    if (field >= PASSWORD_FIELD_COUNT) return;
    password_setting_open_keyboard(field);
}

static void password_setting_save_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (field_text[PASSWORD_FIELD_NEW][0] == '\0') {
        password_setting_show_toast(UI_TEXT_PASSWORD_EMPTY, true);
        return;
    }

    if (strcmp(field_text[PASSWORD_FIELD_CURRENT], user_cfg_password_get()) != 0) {
        password_setting_show_toast(UI_TEXT_PASSWORD_ERROR, true);
        return;
    }

    if (strcmp(field_text[PASSWORD_FIELD_NEW], field_text[PASSWORD_FIELD_CONFIRM]) != 0) {
        password_setting_show_toast(UI_TEXT_PASSWORD_MISMATCH, true);
        return;
    }

    if (!user_cfg_password_save(field_text[PASSWORD_FIELD_NEW])) {
        password_setting_show_toast(UI_TEXT_PASSWORD_SAVE_FAILED, true);
        return;
    }

    memset(field_text, 0, sizeof(field_text));
    active_field = PASSWORD_FIELD_CURRENT;
    password_setting_refresh_fields();
    password_setting_show_toast(UI_TEXT_PASSWORD_SAVED, false);
}

static void password_setting_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    settings_detail_keyboard_hide();
    ui_manager_pop_page();
}

static lv_obj_t* password_setting_create_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                              lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* card = settings_detail_create_card(parent, x, y, w, h);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    return card;
}

static void password_setting_create_field(lv_obj_t* parent, password_field_t field, lv_coord_t y)
{
    lv_obj_t* item = lv_obj_create(parent);

    lv_obj_remove_style_all(item);
    lv_obj_set_pos(item, 34, y);
    lv_obj_set_size(item, 662, 62);
    lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(item, 2, 0);
    lv_obj_set_style_border_color(item, lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_radius(item, 8, 0);
    lv_obj_set_style_translate_y(item, 0, LV_STATE_PRESSED);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item, password_setting_field_cb, LV_EVENT_CLICKED,
                        (void*)(uintptr_t)field);

    settings_detail_create_label(item, ui_text_get(field_titles[field]),
                                 &lv_font_instrument_sans_medium_16, lv_color_hex(0x0D3440), 22, 22);
    field_values[field] = settings_detail_create_label(item,
                                                       ui_text_get(UI_TEXT_PASSWORD_PLACEHOLDER),
                                                       &lv_font_instrument_sans_medium_16,
                                                       lv_color_hex(0x8AA8B8), 430, 22);

    field_cards[field] = item;
}

void ui_page_29_set_password_create(lv_obj_t* parent)
{
    lv_obj_t* content = NULL;
    lv_obj_t* card;
    lv_obj_t* accent;

    if (password_setting_page) return;

    password_setting_page = settings_detail_create_page(parent,
                                                        ui_text_get(UI_TEXT_SETTINGS_PASSWORD),
                                                        password_setting_esc_cb,
                                                        &content);

    card = password_setting_create_card(content, 276, 18, 730, 306);
    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_PASSWORD),
                                 &lv_font_instrument_sans_medium_18, lv_color_hex(0x0D3440), 34, 20);

    accent = lv_obj_create(card);
    lv_obj_remove_style_all(accent);
    lv_obj_set_pos(accent, 274, 28);
    lv_obj_set_size(accent, 182, 7);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x08C5D6), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(accent, 4, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);

    password_setting_create_field(card, PASSWORD_FIELD_CURRENT, 68);
    password_setting_create_field(card, PASSWORD_FIELD_NEW, 138);
    password_setting_create_field(card, PASSWORD_FIELD_CONFIRM, 208);

    settings_detail_create_button(card, 560, 20, 136, 34,
                                  ui_text_get(UI_TEXT_PASSWORD_SAVE),
                                  lv_color_hex(0x0878C8),
                                  password_setting_save_cb, NULL);

    active_field = PASSWORD_FIELD_CURRENT;
    memset(field_text, 0, sizeof(field_text));
    password_setting_refresh_fields();
}

void ui_page_29_set_password_destroy(void)
{
    settings_detail_keyboard_hide();

    if (password_setting_page && lv_obj_is_valid(password_setting_page)) {
        lv_obj_del(password_setting_page);
    }

    password_setting_page = NULL;
    memset(field_cards, 0, sizeof(field_cards));
    memset(field_values, 0, sizeof(field_values));
    memset(field_text, 0, sizeof(field_text));
    active_field = PASSWORD_FIELD_CURRENT;
}
