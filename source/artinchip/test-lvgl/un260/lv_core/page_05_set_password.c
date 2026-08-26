#include "page_05_set_password.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define PASSWORD_DOT_COUNT 4

typedef struct {
    lv_obj_t *page;
    lv_obj_t *display;
    lv_obj_t *field;
    lv_obj_t *dots[PASSWORD_DOT_COUNT];
    lv_obj_t *error_label;
    lv_timer_t *error_timer;
    char input[USER_PASSWORD_MAX_LEN + 1];
} password_page_context_t;

static password_page_context_t g_password_page;

static void password_open_keyboard(void);

static void password_error_timer_cb(lv_timer_t* timer)
{
    (void)timer;

    if (g_password_page.error_label &&
        lv_obj_is_valid(g_password_page.error_label)) {
        lv_obj_add_flag(g_password_page.error_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_password_page.error_timer) {
        lv_timer_del(g_password_page.error_timer);
        g_password_page.error_timer = NULL;
    }
}

static void password_show_error(void)
{
    if (!g_password_page.error_label ||
        !lv_obj_is_valid(g_password_page.error_label)) return;

    lv_label_set_text(g_password_page.error_label,
                      ui_text_get(UI_TEXT_PASSWORD_ERROR));
    lv_obj_clear_flag(g_password_page.error_label, LV_OBJ_FLAG_HIDDEN);

    if (g_password_page.error_timer) {
        lv_timer_del(g_password_page.error_timer);
    }
    g_password_page.error_timer = lv_timer_create(password_error_timer_cb,
                                                  1600, NULL);
}

static void password_set_display_text(const char* value)
{
    size_t len;

    len = value ? strlen(value) : 0;
    if (len > USER_PASSWORD_MAX_LEN) len = USER_PASSWORD_MAX_LEN;

    for (uint8_t i = 0; i < PASSWORD_DOT_COUNT; i++) {
        bool filled = (i < len);

        if (!g_password_page.dots[i] ||
            !lv_obj_is_valid(g_password_page.dots[i])) continue;
        lv_obj_set_style_bg_color(g_password_page.dots[i],
                                  filled ? lv_color_hex(0x0878C8) : lv_color_hex(0xECF4FA),
                                  0);
        lv_obj_set_style_border_color(g_password_page.dots[i],
                                      filled ? lv_color_hex(0x0466AD) : lv_color_hex(0xCFE0EE),
                                      0);
        lv_obj_set_style_shadow_opa(g_password_page.dots[i],
                                    filled ? LV_OPA_30 : LV_OPA_TRANSP, 0);
    }

    if (g_password_page.display && lv_obj_is_valid(g_password_page.display)) {
        lv_label_set_text(g_password_page.display,
                          len > 0 ? "" : ui_text_get(UI_TEXT_PASSWORD_PLACEHOLDER));
        lv_obj_align(g_password_page.display, LV_ALIGN_BOTTOM_MID, 0, -6);
    }
}

static void password_confirm_cb(const char* value, void* user_data)
{
    (void)user_data;

    if (!value || value[0] == '\0') return;

    lv_snprintf(g_password_page.input, sizeof(g_password_page.input), "%s", value);
    password_set_display_text(g_password_page.input);

    if (strcmp(user_cfg_password_get(), g_password_page.input) == 0) {
        memset(g_password_page.input, 0, sizeof(g_password_page.input));
        ui_manager_switch(UI_PAGE_SETTING);
        return;
    }

    memset(g_password_page.input, 0, sizeof(g_password_page.input));
    password_set_display_text("");
    password_show_error();
}

static void password_field_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    password_open_keyboard();
}

static void password_open_keyboard(void)
{
    settings_detail_keyboard_show(ui_text_get(UI_TEXT_PASSWORD_TITLE),
                                  "",
                                  USER_PASSWORD_MAX_LEN,
                                  SETTINGS_DETAIL_KEYBOARD_NUM,
                                  password_confirm_cb,
                                  NULL);
}

static void password_back_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    settings_detail_keyboard_hide();
    ui_manager_switch(UI_PAGE_MAIN);
}

static void password_create_login_card(lv_obj_t* parent)
{
    lv_obj_t* card = settings_detail_create_card(parent, 330, 34, 620, 278);
    lv_obj_t* accent;
    lv_obj_t* halo;
    lv_obj_t* lock_body;
    lv_obj_t* lock_hook;
    lv_obj_t* pin_box;
    lv_obj_t* password_hit_area;

    lv_obj_set_style_shadow_width(card, 22, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);

    accent = lv_obj_create(card);
    lv_obj_remove_style_all(accent);
    lv_obj_set_pos(accent, 220, 22);
    lv_obj_set_size(accent, 180, 6);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x08C5D6), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(accent, 3, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);

    halo = lv_obj_create(card);
    lv_obj_remove_style_all(halo);
    lv_obj_set_pos(halo, 270, 48);
    lv_obj_set_size(halo, 80, 80);
    lv_obj_set_style_radius(halo, 40, 0);
    lv_obj_set_style_bg_color(halo, lv_color_hex(0xEAF8FF), 0);
    lv_obj_set_style_bg_opa(halo, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(halo, 2, 0);
    lv_obj_set_style_border_color(halo, lv_color_hex(0xBCE9F7), 0);
    lv_obj_clear_flag(halo, LV_OBJ_FLAG_SCROLLABLE);

    lock_hook = lv_obj_create(halo);
    lv_obj_remove_style_all(lock_hook);
    lv_obj_set_pos(lock_hook, 24, 16);
    lv_obj_set_size(lock_hook, 32, 34);
    lv_obj_set_style_bg_opa(lock_hook, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lock_hook, 4, 0);
    lv_obj_set_style_border_color(lock_hook, lv_color_hex(0x0878C8), 0);
    lv_obj_set_style_radius(lock_hook, 16, 0);
    lv_obj_clear_flag(lock_hook, LV_OBJ_FLAG_SCROLLABLE);

    lock_body = lv_obj_create(halo);
    lv_obj_remove_style_all(lock_body);
    lv_obj_set_pos(lock_body, 20, 38);
    lv_obj_set_size(lock_body, 40, 28);
    lv_obj_set_style_radius(lock_body, 7, 0);
    lv_obj_set_style_bg_color(lock_body, lv_color_hex(0x0878C8), 0);
    lv_obj_set_style_bg_opa(lock_body, LV_OPA_COVER, 0);
    lv_obj_clear_flag(lock_body, LV_OBJ_FLAG_SCROLLABLE);

    settings_detail_create_label(card, ui_text_get(UI_TEXT_PASSWORD_LOGIN_TITLE),
                                 &lv_font_instrument_sans_medium_24, lv_color_hex(0x0D3440), 206, 134);

    g_password_page.field = lv_obj_create(card);
    lv_obj_remove_style_all(g_password_page.field);
    lv_obj_set_pos(g_password_page.field, 88, 168);
    lv_obj_set_size(g_password_page.field, 444, 80);
    lv_obj_set_style_bg_color(g_password_page.field, lv_color_hex(0xF6FBFF), 0);
    lv_obj_set_style_bg_opa(g_password_page.field, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_password_page.field, 2, 0);
    lv_obj_set_style_border_color(g_password_page.field, lv_color_hex(0x0878C8), 0);
    lv_obj_set_style_radius(g_password_page.field, 8, 0);
    lv_obj_add_flag(g_password_page.field, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_password_page.field, password_field_cb,
                        LV_EVENT_CLICKED, NULL);

    pin_box = lv_obj_create(g_password_page.field);
    lv_obj_remove_style_all(pin_box);
    lv_obj_set_pos(pin_box, 86, 12);
    lv_obj_set_size(pin_box, 272, 34);
    lv_obj_clear_flag(pin_box, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0; i < PASSWORD_DOT_COUNT; i++) {
        g_password_page.dots[i] = lv_obj_create(pin_box);
        lv_obj_remove_style_all(g_password_page.dots[i]);
        lv_obj_set_pos(g_password_page.dots[i], (lv_coord_t)(i * 72), 0);
        lv_obj_set_size(g_password_page.dots[i], 34, 34);
        lv_obj_set_style_radius(g_password_page.dots[i], 17, 0);
        lv_obj_set_style_bg_color(g_password_page.dots[i], lv_color_hex(0xECF4FA), 0);
        lv_obj_set_style_bg_opa(g_password_page.dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(g_password_page.dots[i], 2, 0);
        lv_obj_set_style_border_color(g_password_page.dots[i], lv_color_hex(0xCFE0EE), 0);
        lv_obj_set_style_shadow_width(g_password_page.dots[i], 10, 0);
        lv_obj_set_style_shadow_color(g_password_page.dots[i], lv_color_hex(0x0878C8), 0);
        lv_obj_set_style_shadow_opa(g_password_page.dots[i], LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(g_password_page.dots[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    g_password_page.display = settings_detail_create_label(
        g_password_page.field, ui_text_get(UI_TEXT_PASSWORD_PLACEHOLDER),
        &lv_font_instrument_sans_medium_16, lv_color_hex(0x5686A5), 0, 0);
    lv_obj_align(g_password_page.display, LV_ALIGN_BOTTOM_MID, 0, -6);

    password_hit_area = lv_obj_create(g_password_page.field);
    lv_obj_remove_style_all(password_hit_area);
    lv_obj_set_pos(password_hit_area, 0, 0);
    lv_obj_set_size(password_hit_area, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(password_hit_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(password_hit_area, 0, 0);
    lv_obj_add_flag(password_hit_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(password_hit_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(password_hit_area, password_field_cb, LV_EVENT_CLICKED, NULL);

    g_password_page.error_label = settings_detail_create_label(
        card, "", &lv_font_instrument_sans_medium_16,
        lv_color_hex(0xC03A2B), 258, 252);
    lv_obj_add_flag(g_password_page.error_label, LV_OBJ_FLAG_HIDDEN);

    password_set_display_text("");
}

void ui_page_05_set_password_create(lv_obj_t* parent)
{
    lv_obj_t* content = NULL;

    if (g_password_page.page && lv_obj_is_valid(g_password_page.page)) return;

    ui_page_05_set_password_destroy();

    if (parent == NULL) {
        parent = lv_scr_act();
    }

    g_password_page.page = settings_detail_create_page(
        parent, ui_text_get(UI_TEXT_PASSWORD_LOGIN_TITLE),
        password_back_cb, &content);
    password_create_login_card(content);
    password_open_keyboard();
}

void ui_page_05_set_password_destroy(void)
{
    settings_detail_keyboard_hide();

    if (g_password_page.error_timer) {
        lv_timer_del(g_password_page.error_timer);
        g_password_page.error_timer = NULL;
    }

    if (g_password_page.page && lv_obj_is_valid(g_password_page.page)) {
        lv_obj_del(g_password_page.page);
    }

    memset(&g_password_page, 0, sizeof(g_password_page));
}
