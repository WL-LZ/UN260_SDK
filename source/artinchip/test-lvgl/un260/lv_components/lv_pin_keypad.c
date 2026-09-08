#include "lv_pin_keypad.h"
#include "lv_damped_button.h"

#include <string.h>

#define PIN_IDLE_STATUS "When finished, press CONFIRM."

static const unsigned pin_keys[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 11};

static lv_obj_t *pin_shape(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                           lv_coord_t w, lv_coord_t h, uint32_t color,
                           lv_coord_t radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *pin_label(lv_obj_t *parent, const char *text,
                           const lv_font_t *font, uint32_t color,
                           lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_pos(label, x, y);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

bool lv_pin_keypad_is_visible(const lv_pin_keypad_t *keypad)
{
    return keypad && keypad->root && lv_obj_is_valid(keypad->root) &&
           lv_obj_is_visible(keypad->root);
}

static void pin_refresh(lv_pin_keypad_t *keypad)
{
    size_t n = keypad->input.length;
    for (unsigned i = 0; i < LV_PIN_DIGITS; ++i) {
        bool visible_digit = keypad->digits_visible && i < n;
        lv_obj_set_style_bg_color(keypad->dots[i],
            lv_color_hex(i < n ? 0x000000 : 0xD9E0E3), 0);
        if (visible_digit) {
            char digit[2] = {keypad->input.value[i], '\0'};
            lv_label_set_text(keypad->digits[i], digit);
            lv_obj_clear_flag(keypad->digits[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(keypad->dots[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            /* Erase label text as well as hiding it, including on suspend. */
            lv_label_set_text(keypad->digits[i], "");
            lv_obj_add_flag(keypad->digits[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(keypad->dots[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_damped_button_set_text(keypad->eye,
        keypad->digits_visible ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
    keypad->cursor_on = true;
    lv_obj_set_style_bg_opa(keypad->cursor, LV_OPA_COVER, 0);
    if (n == LV_PIN_DIGITS) {
        lv_obj_add_flag(keypad->cursor, LV_OBJ_FLAG_HIDDEN);
        if (keypad->blink) lv_timer_pause(keypad->blink);
    } else {
        lv_obj_set_x(keypad->cursor, 44 + (lv_coord_t)n * 64);
        lv_obj_clear_flag(keypad->cursor, LV_OBJ_FLAG_HIDDEN);
        if (keypad->blink) {
            lv_timer_reset(keypad->blink);
            if (lv_pin_keypad_is_visible(keypad)) lv_timer_resume(keypad->blink);
            else lv_timer_pause(keypad->blink);
        }
    }
}

static void pin_blink(lv_timer_t *timer)
{
    lv_pin_keypad_t *keypad = timer->user_data;
    if (!lv_pin_keypad_is_visible(keypad) || keypad->input.length == LV_PIN_DIGITS) {
        lv_timer_pause(timer);
        return;
    }
    keypad->cursor_on = !keypad->cursor_on;
    lv_obj_set_style_bg_opa(keypad->cursor,
        keypad->cursor_on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

void lv_pin_keypad_set_status(lv_pin_keypad_t *keypad, const char *text)
{
    if (keypad && keypad->root && lv_obj_is_valid(keypad->root))
        lv_label_set_text(keypad->status, text ? text : PIN_IDLE_STATUS);
}

void lv_pin_keypad_clear(lv_pin_keypad_t *keypad)
{
    if (!keypad) return;
    lv_pin_input_clear(&keypad->input);
    if (keypad->root && lv_obj_is_valid(keypad->root)) pin_refresh(keypad);
}

static void pin_visibility_event(lv_event_t *event)
{
    lv_pin_keypad_t *keypad = lv_event_get_user_data(event);
    if (lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !lv_pin_keypad_is_visible(keypad)) return;
    bool visible = !keypad->digits_visible;
    if (keypad->save_visibility && !keypad->save_visibility(visible)) {
        lv_pin_keypad_set_status(keypad, "Could not save display preference.");
        return;
    }
    keypad->digits_visible = visible;
    lv_pin_keypad_set_status(keypad, NULL);
    pin_refresh(keypad);
}

static void pin_key_event(lv_event_t *event)
{
    lv_pin_keypad_t *keypad = lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target(event);
    unsigned key = 12;
    if (lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !lv_pin_keypad_is_visible(keypad)) return;
    for (unsigned i = 0; i < 12; ++i) {
        if (target == keypad->keys[i]) {
            key = pin_keys[i];
            break;
        }
    }
    if (key == 11) {
        if (!lv_pin_input_is_complete(keypad->input.value)) {
            lv_pin_keypad_set_status(keypad, "Enter exactly 4 digits.");
            return;
        }
        /* The callback may hide/destroy the keypad (or its parent page). */
        if (keypad->confirm_cb) {
            char value[LV_PIN_DIGITS + 1];
            lv_pin_keypad_confirm_cb_t callback = keypad->confirm_cb;
            void *user_data = keypad->user_data;
            memcpy(value, keypad->input.value, sizeof(value));
            callback(value, user_data);
            memset(value, 0, sizeof(value));
        }
        return;
    }
    if (key == 10) lv_pin_input_backspace(&keypad->input);
    else if (key < 10) lv_pin_input_push(&keypad->input, key);
    else return;
    lv_pin_keypad_set_status(keypad, NULL);
    pin_refresh(keypad);
}

static void pin_cancel_event(lv_event_t *event)
{
    lv_pin_keypad_t *keypad = lv_event_get_user_data(event);
    if (lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !lv_pin_keypad_is_visible(keypad)) return;
    if (keypad->cancel_cb) keypad->cancel_cb(keypad->user_data);
}

static void pin_delete_event(lv_event_t *event)
{
    lv_pin_keypad_t *keypad = lv_event_get_user_data(event);
    if (lv_event_get_code(event) != LV_EVENT_DELETE ||
        lv_event_get_target(event) != keypad->root) return;
    if (keypad->blink) lv_timer_del(keypad->blink);
    memset(keypad, 0, sizeof(*keypad));
}

bool lv_pin_keypad_create(lv_pin_keypad_t *keypad, lv_obj_t *parent,
                          lv_coord_t x, lv_coord_t y)
{
    if (!keypad || !parent) return false;
    if (keypad->root && lv_obj_is_valid(keypad->root)) return true;
    memset(keypad, 0, sizeof(*keypad));
    lv_obj_t *card = pin_shape(parent, x, y, LV_PIN_KEYPAD_WIDTH,
                                LV_PIN_KEYPAD_HEIGHT, 0xFFFFFF, 5);
    keypad->root = card;
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xE4E8EA), 0);
    lv_obj_add_event_cb(card, pin_delete_event, LV_EVENT_DELETE, keypad);
    lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
    keypad->eyebrow = pin_label(card, "", &lv_font_instrument_sans_medium_12,
                                 0x87969C, 32, 30);
    lv_obj_set_style_text_letter_space(keypad->eyebrow, 2, 0);
    keypad->title = pin_label(card, "", &lv_font_instrument_sans_bold_24,
                               0x30464F, 32, 67);
    lv_obj_set_width(keypad->title, 360);
    keypad->prompt = pin_label(card, "", &lv_font_instrument_sans_medium_14,
                                0x87969C, 32, 110);
    lv_obj_set_width(keypad->prompt, 360);
    for (unsigned i = 0; i < LV_PIN_DIGITS; ++i) {
        keypad->dots[i] = pin_shape(card, 48 + i * 64, 176, 12, 12,
                                     0xD9E0E3, LV_RADIUS_CIRCLE);
        keypad->digits[i] = pin_label(card, "", &lv_font_instrument_sans_medium_24,
                                       0x000000, 36 + i * 64, 165);
        lv_obj_set_size(keypad->digits[i], 36, 32);
        lv_obj_set_style_text_align(keypad->digits[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_add_flag(keypad->digits[i], LV_OBJ_FLAG_HIDDEN);
    }
    const lv_damped_button_style_t eye_style = {
        .normal_color = 0xFFFFFF, .text_color = 0x8A959D, .radius = 8
    };
    keypad->eye = lv_damped_button_create(card, &eye_style, LV_SYMBOL_EYE_CLOSE,
                                          &lv_font_montserrat_20);
    /* Small gray icon above/right of the fourth slot, with a larger hit box. */
    lv_obj_set_pos(keypad->eye, 266, 140);
    lv_obj_set_size(keypad->eye, 44, 36);
    lv_obj_set_style_shadow_width(keypad->eye, 0, 0);
    lv_obj_add_event_cb(keypad->eye, pin_visibility_event, LV_EVENT_CLICKED, keypad);
    keypad->cursor = pin_shape(card, 44, 205, 20, 2, 0xA9B8BF, 0);
    keypad->status = pin_label(card, PIN_IDLE_STATUS,
        &lv_font_instrument_sans_medium_14, 0x87969C, 32, 235);
    lv_obj_set_width(keypad->status, 360);
    pin_shape(card, 32, 273, 328, 1, 0xE6EAEC, 0);
    pin_label(card, "PIN VERIFICATION", &lv_font_instrument_sans_medium_12,
               0x87969C, 32, 288);
    const lv_damped_button_style_t cancel_style = {
        .normal_color = 0xFFFFFF, .text_color = 0x87969C, .radius = 0
    };
    keypad->cancel = lv_damped_button_create(card, &cancel_style, "ESC Cancel",
                                              &lv_font_instrument_sans_medium_12);
    lv_obj_set_pos(keypad->cancel, 278, 277);
    lv_obj_set_size(keypad->cancel, 102, 36);
    lv_obj_set_style_shadow_width(keypad->cancel, 0, 0);
    lv_obj_add_event_cb(keypad->cancel, pin_cancel_event, LV_EVENT_CLICKED, keypad);
    for (unsigned i = 0; i < 12; ++i) {
        unsigned key = pin_keys[i];
        char digit[2] = {(char)('0' + key), 0};
        lv_damped_button_style_t style = {
            .normal_color = key == 11 ? 0x088DA7 : 0xF7F8F8,
            .text_color = key == 11 ? 0xFFFFFF : 0x30464F,
            .disabled_color = 0xE6EAEC, .disabled_text_color = 0x87969C, .radius = 0
        };
        lv_obj_t *button = lv_damped_button_create(card, &style,
            key == 10 ? LV_SYMBOL_BACKSPACE : key == 11 ? "CONFIRM" : digit,
            key == 10 ? &lv_font_montserrat_20 : key == 11 ?
                &lv_font_instrument_sans_medium_16 : &lv_font_instrument_sans_medium_24);
        keypad->keys[i] = button;
        lv_obj_set_pos(button, 416 + (i % 3) * 226, 24 + (i / 3) * 68);
        lv_obj_set_size(button, 226, 68);
        lv_obj_set_style_border_width(button, 1, 0);
        lv_obj_set_style_border_color(button, lv_color_hex(0xE8ECEE), 0);
        lv_obj_set_style_shadow_width(button, 0, 0);
        lv_obj_add_event_cb(button, pin_key_event, LV_EVENT_CLICKED, keypad);
    }
    keypad->blink = lv_timer_create(pin_blink, 500, keypad);
    if (keypad->blink) lv_timer_pause(keypad->blink);
    return true;
}

bool lv_pin_keypad_show(lv_pin_keypad_t *keypad,
                        const lv_pin_keypad_config_t *config,
                        const char *initial_value)
{
    if (!keypad || !config || !keypad->root || !lv_obj_is_valid(keypad->root))
        return false;
    lv_pin_input_set(&keypad->input, initial_value);
    lv_label_set_text(keypad->eyebrow, config->eyebrow ? config->eyebrow : "SECURE ACCESS");
    lv_label_set_text(keypad->title, config->title ? config->title : "Enter access PIN");
    lv_label_set_text(keypad->prompt, config->prompt ? config->prompt : "Please enter your 4-digit PIN");
    keypad->confirm_cb = config->confirm_cb;
    keypad->cancel_cb = config->cancel_cb;
    keypad->user_data = config->user_data;
    keypad->digits_visible = config->digits_visible;
    keypad->save_visibility = config->save_visibility;
    lv_obj_clear_flag(keypad->root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(keypad->root);
    if (config->cancel_cb) lv_obj_clear_flag(keypad->cancel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(keypad->cancel, LV_OBJ_FLAG_HIDDEN);
    lv_pin_keypad_set_status(keypad, NULL);
    pin_refresh(keypad);
    return true;
}

void lv_pin_keypad_hide(lv_pin_keypad_t *keypad)
{
    if (!keypad) return;
    if (keypad->root && lv_obj_is_valid(keypad->root))
        lv_obj_add_flag(keypad->root, LV_OBJ_FLAG_HIDDEN);
    if (keypad->blink) lv_timer_pause(keypad->blink);
    lv_pin_keypad_clear(keypad);
    keypad->confirm_cb = NULL;
    keypad->cancel_cb = NULL;
    keypad->user_data = NULL;
    keypad->save_visibility = NULL;
}

void lv_pin_keypad_destroy(lv_pin_keypad_t *keypad)
{
    if (!keypad) return;
    if (keypad->root && lv_obj_is_valid(keypad->root)) lv_obj_del(keypad->root);
    else {
        if (keypad->blink) lv_timer_del(keypad->blink);
        memset(keypad, 0, sizeof(*keypad));
    }
}
