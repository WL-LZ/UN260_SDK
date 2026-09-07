#include "page_05_set_password.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include <string.h>

#define PIN_DIGITS 4
typedef struct {
    lv_obj_t *page, *dots[PIN_DIGITS], *cursor, *status;
    lv_timer_t *blink;
    char input[PIN_DIGITS + 1];
    bool cursor_on;
} password_page_context_t;
static password_page_context_t g_password_page;

static void pin_refresh(void)
{
    size_t n = strlen(g_password_page.input);
    for(unsigned i = 0; i < PIN_DIGITS; ++i)
        lv_obj_set_style_bg_color(g_password_page.dots[i],
            lv_color_hex(i < n ? 0x000000 : 0xD9E0E3), 0);
    g_password_page.cursor_on = true;
    if(n == PIN_DIGITS) {
        lv_obj_add_flag(g_password_page.cursor, LV_OBJ_FLAG_HIDDEN);
        if(g_password_page.blink) lv_timer_pause(g_password_page.blink);
    } else {
        lv_obj_set_x(g_password_page.cursor, 44 + (lv_coord_t)n * 64);
        lv_obj_clear_flag(g_password_page.cursor, LV_OBJ_FLAG_HIDDEN);
        if(g_password_page.blink) {
            lv_timer_reset(g_password_page.blink);
            lv_timer_resume(g_password_page.blink);
        }
    }
}
static void pin_blink(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if(!g_password_page.page || lv_obj_has_flag(g_password_page.page, LV_OBJ_FLAG_HIDDEN) ||
       strlen(g_password_page.input) == PIN_DIGITS) return;
    g_password_page.cursor_on = !g_password_page.cursor_on;
    lv_obj_set_style_bg_opa(g_password_page.cursor,
        g_password_page.cursor_on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}
static void pin_key(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    unsigned key = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    size_t n = strlen(g_password_page.input);
    if(key == 11) {
        if(n != PIN_DIGITS) {
            lv_label_set_text(g_password_page.status, "Enter exactly 4 digits.");
            return;
        }
        if(strcmp(user_cfg_password_get(), g_password_page.input) == 0) {
            memset(g_password_page.input, 0, sizeof(g_password_page.input));
            ui_manager_switch(UI_PAGE_SETTING);
            return;
        }
        memset(g_password_page.input, 0, sizeof(g_password_page.input));
        lv_label_set_text(g_password_page.status, "Incorrect PIN. Please try again.");
    } else {
        if(key == 10) { if(n) g_password_page.input[n - 1] = 0; }
        else if(key < 10 && n < PIN_DIGITS) {
            g_password_page.input[n] = (char)('0' + key);
            g_password_page.input[n + 1] = 0;
        }
        lv_label_set_text(g_password_page.status, "When finished, press CONFIRM.");
    }
    lv_obj_set_style_bg_opa(g_password_page.cursor, LV_OPA_COVER, 0);
    pin_refresh();
}
static void password_back_cb(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) ui_manager_switch(UI_PAGE_MAIN);
}
static lv_obj_t *pin_shape(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
void ui_page_05_set_password_create(lv_obj_t *parent)
{
    if(g_password_page.page && lv_obj_is_valid(g_password_page.page)) return;
    lv_obj_t *content = NULL;
    g_password_page.page = settings_detail_create_page(parent,
        ui_text_get(UI_TEXT_PASSWORD_LOGIN_TITLE), password_back_cb, &content);
    /* 1120 x 320, centred inside the 1280 x 345 content below the existing header. */
    lv_obj_t *card = settings_detail_create_card(content, 80, 12, 1120, 320);
    lv_obj_set_style_radius(card, 5, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xE4E8EA), 0);
    lv_obj_t *label = settings_detail_create_label(card, "SECURE ACCESS",
        &lv_font_instrument_sans_medium_12, lv_color_hex(0x87969C), 32, 30);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    settings_detail_create_label(card, "Enter access PIN",
        &lv_font_instrument_sans_bold_24, lv_color_hex(0x30464F), 32, 67);
    settings_detail_create_label(card, "Please enter your 4-digit PIN",
        &lv_font_instrument_sans_medium_14, lv_color_hex(0x87969C), 32, 110);
    for(unsigned i=0; i<PIN_DIGITS; ++i)
        g_password_page.dots[i] = pin_shape(card, 48 + i*64, 176, 12, 12, 0xD9E0E3, LV_RADIUS_CIRCLE);
    g_password_page.cursor = pin_shape(card, 44, 205, 20, 2, 0xA9B8BF, 0);
    g_password_page.status = settings_detail_create_label(card,
        "When finished, press CONFIRM.", &lv_font_instrument_sans_medium_14,
        lv_color_hex(0x87969C), 32, 235);
    lv_obj_set_width(g_password_page.status, 360);
    pin_shape(card, 32, 273, 328, 1, 0xE6EAEC, 0);
    settings_detail_create_label(card, "PIN VERIFICATION",
        &lv_font_instrument_sans_medium_12, lv_color_hex(0x87969C), 32, 288);
    settings_detail_create_label(card, "ESC Cancel",
        &lv_font_instrument_sans_medium_12, lv_color_hex(0x87969C), 292, 288);
    static const unsigned keys[12] = {1,2,3,4,5,6,7,8,9,10,0,11};
    for(unsigned i=0; i<12; ++i) {
        unsigned key = keys[i]; char digit[2] = {(char)('0'+key),0};
        lv_damped_button_style_t style = {
            .normal_color=key == 11 ? 0x088DA7 : 0xF7F8F8,
            .text_color=key == 11 ? 0xFFFFFF : 0x30464F,
            .disabled_color=0xE6EAEC, .disabled_text_color=0x87969C, .radius=0
        };
        lv_obj_t *btn = lv_damped_button_create(card, &style,
            key == 10 ? LV_SYMBOL_BACKSPACE : key == 11 ? "CONFIRM" : digit,
            key == 10 ? &lv_font_montserrat_20 :
            key == 11 ? &lv_font_instrument_sans_medium_16 : &lv_font_instrument_sans_medium_24);
        lv_obj_set_pos(btn, 416 + (i%3)*226, 24 + (i/3)*68);
        lv_obj_set_size(btn, 226, 68);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0xE8ECEE), 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_add_event_cb(btn, pin_key, LV_EVENT_CLICKED, (void *)(uintptr_t)key);
    }
    g_password_page.blink = lv_timer_create(pin_blink, 500, NULL);
    pin_refresh();
}
void ui_page_05_set_password_destroy(void)
{
    if(g_password_page.blink) lv_timer_del(g_password_page.blink);
    if(g_password_page.page && lv_obj_is_valid(g_password_page.page))
        lv_obj_del(g_password_page.page);
    memset(&g_password_page, 0, sizeof(g_password_page));
}
bool ui_page_05_set_password_resume(void)
{
    if(!g_password_page.page || !lv_obj_is_valid(g_password_page.page)) return false;
    memset(g_password_page.input, 0, sizeof(g_password_page.input));
    lv_label_set_text(g_password_page.status, "When finished, press CONFIRM.");
    lv_obj_clear_flag(g_password_page.page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_password_page.page);
    lv_obj_set_style_bg_opa(g_password_page.cursor, LV_OPA_COVER, 0);
    pin_refresh();
    return true;
}
void ui_page_05_set_password_suspend(void)
{
    if(!g_password_page.page || !lv_obj_is_valid(g_password_page.page)) return;
    if(g_password_page.blink) lv_timer_pause(g_password_page.blink);
    memset(g_password_page.input, 0, sizeof(g_password_page.input));
    lv_obj_add_flag(g_password_page.page, LV_OBJ_FLAG_HIDDEN);
}
