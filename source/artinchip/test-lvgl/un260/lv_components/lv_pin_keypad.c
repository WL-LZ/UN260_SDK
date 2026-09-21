#include "lv_pin_keypad.h"
#include "lv_damped_button.h"
#include "lv_settings_palette.h"

#include <string.h>

#define PIN_IDLE_STATUS ""

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
        lv_obj_set_style_bg_color(keypad->dots[i], lv_color_hex(keypad->compact ?
            (keypad->error ? 0xF8E9E9 : i < n ? 0x20313B : 0xFFFFFF) :
            (i < n ? 0x1D2B34 : 0xB9C5CD)), 0);
        lv_obj_set_style_border_width(keypad->dots[i],keypad->compact?2:0,0);
        lv_obj_set_style_border_color(keypad->dots[i],lv_color_hex(
            keypad->error ? 0xBE6C6D : i < n ? 0x20313B : 0xCAD6DF),0);
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
        keypad->digits_visible ? "Hide" : "Show");
    keypad->cursor_on = true;
    lv_obj_set_style_bg_opa(keypad->cursor, LV_OPA_COVER, 0);
    if (keypad->compact || n == LV_PIN_DIGITS) {
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
    if (!keypad || !keypad->root || !lv_obj_is_valid(keypad->root)) return;
    bool was_error = keypad->error;
    keypad->error = false;
    lv_label_set_text(keypad->status, text ? text :
        keypad->idle_status ? keypad->idle_status : PIN_IDLE_STATUS);
    lv_obj_set_style_text_color(keypad->status,
        lv_color_hex(keypad->compact ? 0x647B89 : 0x586B78), 0);
    if (was_error) pin_refresh(keypad);
}

void lv_pin_keypad_set_error(lv_pin_keypad_t *keypad, const char *text)
{
    lv_pin_keypad_set_status(keypad, text);
    if (!keypad || !keypad->root || !lv_obj_is_valid(keypad->root)) return;
    keypad->error = true;
    lv_obj_set_style_text_color(keypad->status, lv_color_hex(0xB23E40), 0);
    pin_refresh(keypad);
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
        if(keypad->auto_confirm){lv_pin_keypad_clear(keypad);lv_pin_keypad_set_status(keypad,NULL);return;}
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
    if(keypad->auto_confirm&&keypad->input.length==LV_PIN_DIGITS&&keypad->confirm_cb){
        char value[LV_PIN_DIGITS+1];
        lv_pin_keypad_confirm_cb_t callback=keypad->confirm_cb;
        void *user_data=keypad->user_data;
        memcpy(value,keypad->input.value,sizeof(value));
        callback(value,user_data);
        memset(value,0,sizeof(value));
        /* Do not access keypad after callback: it may have been destroyed. */
    }
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

static void pin_apply_layout(lv_pin_keypad_t *keypad, bool compact)
{
    keypad->compact=compact;
    lv_obj_set_size(keypad->root,compact?850:LV_PIN_KEYPAD_WIDTH,
                    compact?364:LV_PIN_KEYPAD_HEIGHT);
    lv_obj_set_style_bg_color(keypad->root,lv_color_hex(compact?0xFBFCFD:0xFFFFFF),0);
    lv_obj_set_style_radius(keypad->root,compact?22:16,0);
    lv_obj_set_style_border_color(keypad->root,lv_color_hex(compact?0xFFFFFF:0xE3E9ED),0);
    lv_obj_set_style_shadow_width(keypad->root,compact?50:0,0);
    lv_obj_set_style_shadow_color(keypad->root,lv_color_hex(0x263F4F),0);
    lv_obj_set_style_shadow_opa(keypad->root,32,0);
    lv_obj_set_style_shadow_ofs_y(keypad->root,compact?20:0,0);
    if(compact){
        lv_obj_add_flag(keypad->eyebrow,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(keypad->eye,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(keypad->close_icon,LV_OBJ_FLAG_HIDDEN);
    }else{
        lv_obj_clear_flag(keypad->eyebrow,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(keypad->eye,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(keypad->close_icon,LV_OBJ_FLAG_HIDDEN);
    }
    /* Content coordinates exclude the root's 1px border. Rounded from the
     * original 850x364 CSS grid, rather than scaling the legacy wide keypad. */
    lv_obj_set_pos(keypad->title,compact?34:24,compact?72:48);
    lv_obj_set_style_text_letter_space(keypad->title,compact?-1:0,0);
    lv_obj_set_style_text_color(keypad->title,lv_color_hex(compact?0x20313B:0x1D2B34),0);
    lv_obj_set_pos(keypad->prompt,compact?34:24,compact?114:92);
    lv_obj_set_style_text_color(keypad->prompt,lv_color_hex(compact?0x647B89:0x586B78),0);
    for(unsigned i=0;i<LV_PIN_DIGITS;i++){
        lv_obj_set_pos(keypad->dots[i],compact?34+i*34:48+i*64,compact?166:166);
        lv_obj_set_size(keypad->dots[i],compact?19:12,compact?19:12);
        lv_obj_set_pos(keypad->digits[i],compact?25+i*35:36+i*64,compact?151:152);
    }
    lv_obj_set_pos(keypad->eye,compact?210:288,compact?149:148);
    lv_obj_set_size(keypad->eye,compact?90:104,48);
    lv_obj_set_pos(keypad->status,compact?34:24,compact?204:220);
    lv_obj_set_width(keypad->status,compact?340:368);
    lv_obj_set_style_text_font(keypad->status,compact?
        &lv_font_instrument_sans_medium_13:&lv_font_instrument_sans_medium_14,0);
    lv_obj_set_pos(keypad->cancel,compact?791:24,compact?14:260);
    lv_obj_set_size(keypad->cancel,compact?40:368,compact?36:44);
    lv_damped_button_set_text(keypad->cancel,compact?"":"Cancel");
    lv_damped_button_set_exact_palette(keypad->cancel,
        lv_color_hex(compact?0xFBFCFD:LV_SETTINGS_CONTROL_SURFACE),
        lv_color_hex(compact?0xE2E9EE:LV_SETTINGS_CONTROL_PRESSED));
    lv_obj_center(keypad->close_icon);
    lv_img_set_src(keypad->backspace_icon,compact?
        LVGL_DIR "pin_icons/erase.png":LVGL_DIR "popup_icons/backspace.png");
    static const lv_coord_t col_x[3]={486,601,715};
    static const lv_coord_t col_w[3]={107,107,107};
    static const lv_coord_t row_y[4]={55,128,201,273};
    static const lv_coord_t row_h[4]={65,65,64,65};
    for(unsigned i=0;i<12;i++){
        /* Keep key identity/callbacks stable; the side panel orders Clear,0,erase. */
        unsigned slot=compact&&keypad->auto_confirm?(i==9?11:i==11?9:i):i;
        lv_coord_t col=(lv_coord_t)(slot%3),row=(lv_coord_t)(slot/3);
        bool utility=compact&&(i==9||i==11);
        lv_obj_t *button=keypad->keys[i];
        lv_obj_set_pos(button,compact?col_x[col]:424+col*228,
                       compact?row_y[row]:16+row*76);
        lv_obj_set_size(button,compact?col_w[col]:216,compact?row_h[row]:64);
        lv_damped_button_set_exact_palette(button,lv_color_hex(compact?
            (utility?0xFBFCFD:0xF0F3F5):i==11&&!keypad->auto_confirm?
            LV_SETTINGS_PRIMARY:LV_SETTINGS_CONTROL_SURFACE),lv_color_hex(compact?
            0xE2E9EE:i==11&&!keypad->auto_confirm?
            LV_SETTINGS_PRIMARY_PRESSED:LV_SETTINGS_CONTROL_PRESSED));
        lv_obj_set_style_border_width(button,compact&&!utility?1:0,0);
        lv_obj_set_style_border_color(button,lv_color_hex(0xE9EDF0),0);
        lv_obj_t *label=lv_damped_button_get_label(button);
        lv_obj_set_style_text_font(label,compact?(utility?
            &lv_font_instrument_sans_medium_14:&lv_font_instrument_sans_medium_26):
            i==11?&lv_font_instrument_sans_medium_16:&lv_font_instrument_sans_medium_28,0);
        lv_obj_set_style_text_color(label,lv_color_hex(compact?(utility?0x647B89:0x20313B):
            i==11&&!keypad->auto_confirm?0xFFFFFF:LV_SETTINGS_ACTION_TEXT),0);
    }
    lv_obj_center(keypad->backspace_icon);
}

bool lv_pin_keypad_create(lv_pin_keypad_t *keypad, lv_obj_t *parent,
                          lv_coord_t x, lv_coord_t y)
{
    if (!keypad || !parent) return false;
    if (keypad->root && lv_obj_is_valid(keypad->root)) return true;
    memset(keypad, 0, sizeof(*keypad));
    lv_obj_t *card = pin_shape(parent, x, y, LV_PIN_KEYPAD_WIDTH,
                                LV_PIN_KEYPAD_HEIGHT, 0xFFFFFF, 16);
    keypad->root = card;
    /* Blank areas inside the keyboard are not outside-dismiss targets. */
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xE3E9ED), 0);
    lv_obj_add_event_cb(card, pin_delete_event, LV_EVENT_DELETE, keypad);
    lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
    keypad->eyebrow = pin_label(card, "", &lv_font_instrument_sans_medium_14,
                                 0x586B78, 24, 20);
    keypad->title = pin_label(card, "", &lv_font_instrument_sans_semibold_28,
                               0x1D2B34, 24, 48);
    lv_obj_set_width(keypad->title, 376);
    keypad->prompt = pin_label(card, "", &lv_font_instrument_sans_medium_14,
                                0x586B78, 24, 92);
    lv_obj_set_width(keypad->prompt, 376);
    keypad->leading_icon=lv_img_create(card);
    lv_obj_set_pos(keypad->leading_icon,34,30);
    lv_obj_clear_flag(keypad->leading_icon,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    keypad->footnote_icon=lv_img_create(card);
    lv_obj_set_pos(keypad->footnote_icon,34,314);
    lv_obj_clear_flag(keypad->footnote_icon,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    keypad->footnote=pin_label(card,"",&lv_font_instrument_sans_medium_12,0x647B89,59,314);
    lv_obj_set_width(keypad->footnote,355);
    for (unsigned i = 0; i < LV_PIN_DIGITS; ++i) {
        keypad->dots[i] = pin_shape(card, 48 + i * 64, 166, 12, 12,
                                     0xB9C5CD, LV_RADIUS_CIRCLE);
        keypad->digits[i] = pin_label(card, "", &lv_font_instrument_sans_medium_28,
                                       0x1D2B34, 36 + i * 64, 152);
        lv_obj_set_size(keypad->digits[i], 36, 36);
        lv_obj_set_style_text_align(keypad->digits[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_add_flag(keypad->digits[i], LV_OBJ_FLAG_HIDDEN);
    }
    const lv_damped_button_style_t eye_style = {
        .normal_color = LV_SETTINGS_CONTROL_SURFACE, .pressed_color = LV_SETTINGS_CONTROL_PRESSED,
        .text_color = LV_SETTINGS_ACTION_TEXT, .disabled_color = LV_SETTINGS_DISABLED_SURFACE,
        .disabled_text_color = LV_SETTINGS_DISABLED_TEXT, .radius = 10
    };
    keypad->eye = lv_damped_button_create(card, &eye_style, "Show",
                                          &lv_font_instrument_sans_medium_16);
    lv_damped_button_set_exact_palette(keypad->eye, lv_color_hex(eye_style.normal_color), lv_color_hex(eye_style.pressed_color));
    lv_obj_set_pos(keypad->eye, 288, 148);
    lv_obj_set_size(keypad->eye, 104, 48);
    lv_obj_set_style_shadow_width(keypad->eye, 0, 0);
    lv_obj_add_event_cb(keypad->eye, pin_visibility_event, LV_EVENT_CLICKED, keypad);
    keypad->cursor = pin_shape(card, 44, 196, 20, 2, 0x1462CC, 0);
    keypad->status = pin_label(card, PIN_IDLE_STATUS,
        &lv_font_instrument_sans_medium_14, 0x586B78, 24, 220);
    lv_obj_set_width(keypad->status, 368);
    const lv_damped_button_style_t cancel_style = {
        .normal_color = LV_SETTINGS_CONTROL_SURFACE, .pressed_color = LV_SETTINGS_CONTROL_PRESSED,
        .text_color = LV_SETTINGS_ACTION_TEXT, .disabled_color = LV_SETTINGS_DISABLED_SURFACE,
        .disabled_text_color = LV_SETTINGS_DISABLED_TEXT, .radius = 10
    };
    keypad->cancel = lv_damped_button_create(card, &cancel_style, "Cancel",
                                              &lv_font_instrument_sans_medium_16);
    lv_damped_button_set_exact_palette(keypad->cancel, lv_color_hex(cancel_style.normal_color), lv_color_hex(cancel_style.pressed_color));
    lv_obj_set_pos(keypad->cancel, 24, 260);
    lv_obj_set_size(keypad->cancel, 368, 44);
    lv_obj_set_style_shadow_width(keypad->cancel, 0, 0);
    lv_obj_add_event_cb(keypad->cancel, pin_cancel_event, LV_EVENT_CLICKED, keypad);
    keypad->close_icon=lv_img_create(keypad->cancel);
    lv_img_set_src(keypad->close_icon,LVGL_DIR "pin_icons/close.png");
    lv_obj_clear_flag(keypad->close_icon,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(keypad->close_icon,LV_OBJ_FLAG_HIDDEN);
    for (unsigned i = 0; i < 12; ++i) {
        unsigned key = pin_keys[i];
        char digit[2] = {(char)('0' + key), 0};
        lv_damped_button_style_t style = {
            .normal_color = key == 11 ? LV_SETTINGS_PRIMARY : LV_SETTINGS_CONTROL_SURFACE,
            .pressed_color = key == 11 ? LV_SETTINGS_PRIMARY_PRESSED : LV_SETTINGS_CONTROL_PRESSED,
            .text_color = key == 11 ? 0xFFFFFF : LV_SETTINGS_ACTION_TEXT,
            .disabled_color = LV_SETTINGS_DISABLED_SURFACE, .disabled_text_color = LV_SETTINGS_DISABLED_TEXT, .radius = 12
        };
        lv_obj_t *button = lv_damped_button_create(card, &style,
            key == 10 ? "" : key == 11 ? "Confirm" : digit,
            key == 11 ? &lv_font_instrument_sans_medium_16 : &lv_font_instrument_sans_medium_28);
        lv_damped_button_set_exact_palette(button, lv_color_hex(style.normal_color), lv_color_hex(style.pressed_color));
        keypad->keys[i] = button;
        lv_obj_set_pos(button, 424 + (i % 3) * 228, 16 + (i / 3) * 76);
        lv_obj_set_size(button, 216, 64);
        lv_obj_set_style_border_width(button, 0, 0);
        lv_obj_set_style_shadow_width(button, 0, 0);
        if (key == 10) {
            lv_obj_t *icon = lv_img_create(button);
            keypad->backspace_icon=icon;
            lv_img_set_src(icon, LVGL_DIR "popup_icons/backspace.png");
            lv_obj_center(icon);
            lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        }
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
    keypad->digits_visible = !config->compact && config->digits_visible;
    keypad->auto_confirm = config->auto_confirm;
    keypad->idle_status = config->idle_status;
    if(config->compact&&config->leading_icon){
        lv_img_set_src(keypad->leading_icon,config->leading_icon);
        lv_obj_clear_flag(keypad->leading_icon,LV_OBJ_FLAG_HIDDEN);
    }else lv_obj_add_flag(keypad->leading_icon,LV_OBJ_FLAG_HIDDEN);
    if(config->compact&&config->footnote_icon){
        lv_img_set_src(keypad->footnote_icon,config->footnote_icon);
        lv_obj_clear_flag(keypad->footnote_icon,LV_OBJ_FLAG_HIDDEN);
    }else lv_obj_add_flag(keypad->footnote_icon,LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(keypad->footnote,config->compact&&config->footnote?config->footnote:"");
    pin_apply_layout(keypad,config->compact);
    lv_damped_button_set_text(keypad->keys[11],config->auto_confirm?"Clear":"Confirm");
    keypad->save_visibility = config->save_visibility;
    lv_obj_clear_flag(keypad->root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(keypad->root);
    /* A freshly-created modal on layer_top has not been laid out yet. Resolve
     * it before visibility-based input/blink checks, not on the next frame. */
    lv_obj_update_layout(keypad->root);
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
    keypad->idle_status = NULL;
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
