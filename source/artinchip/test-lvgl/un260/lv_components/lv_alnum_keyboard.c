#include "lv_alnum_keyboard.h"
#include "lv_damped_button.h"
#include "lv_nav_button.h"

#include <string.h>

#define KEYBOARD_WIDTH 1280
#define KEYBOARD_HEIGHT 400
#define PANEL_WIDTH 1120
#define PANEL_HEIGHT 320
#define BUTTON_COUNT 41
#define CHARACTER_COUNT 36
#define KEY_COLOR 0xFFFFFF
#define ACTION_COLOR 0x607888
#define CANCEL_COLOR 0xF6F7F8
#define TEXT_COLOR 0x4C606E
#define MUTED_COLOR 0x7A8D9B
#define LINE_COLOR 0xDFE6EB

struct lv_alnum_keyboard {
    lv_obj_t *root, *value_label, *placeholder, *cursor;
    lv_indev_t *contact;
    lv_obj_t *buttons[BUTTON_COUNT];
    uint32_t button_colors[BUTTON_COUNT];
    uint8_t button_count, max_length;
    bool symbol_page;
    char text[LV_ALNUM_KEYBOARD_MAX_TEXT + 1];
    lv_alnum_keyboard_submit_cb_t submit;
    lv_alnum_keyboard_cancel_cb_t cancel;
    void *context;
};

static void set_visible(lv_obj_t *object, bool visible)
{
    if (visible) lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *plain(lv_obj_t *parent, int x, int y, int width, int height,
                       uint32_t color, int radius)
{
    lv_obj_t *object = lv_obj_create(parent);
    if (!object) return NULL;
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(object, radius, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

static lv_obj_t *label_create(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, uint32_t color,
                              int x, int y, int width, int height)
{
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return NULL;
    lv_obj_remove_style_all(label);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(label, text ? text : "");
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

static void refresh_text(lv_alnum_keyboard_t *keyboard)
{
    lv_point_t size;
    lv_label_set_text(keyboard->value_label, keyboard->text);
    set_visible(keyboard->placeholder, keyboard->text[0] == '\0');
    lv_txt_get_size(&size, keyboard->text, &lv_font_instrument_sans_medium_22,
                    0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_x(keyboard->cursor, 14 + size.x + 2);
}

static void stop_button(lv_alnum_keyboard_t *keyboard, unsigned index)
{
    lv_obj_t *button = keyboard->buttons[index];
    lv_obj_clear_state(button, LV_STATE_PRESSED);
    lv_color_t color = lv_color_hex(keyboard->button_colors[index]);
    lv_damped_button_set_palette(button, color, color);
    lv_anim_del(button, NULL);
    for (uint32_t child = 0; child < lv_obj_get_child_cnt(button); ++child)
        lv_obj_set_style_translate_y(lv_obj_get_child(button, (int32_t)child), 0, 0);
}

static void input_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_RELEASED &&
        code != LV_EVENT_PRESS_LOST) return;
    lv_alnum_keyboard_t *keyboard = lv_event_get_user_data(event);
    bool visible = lv_alnum_keyboard_is_visible(keyboard);
    if (code == LV_EVENT_PRESSED && visible) {
        lv_indev_t *indev = lv_event_get_indev(event);
        if (indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER)
            keyboard->contact = indev;
    } else keyboard->contact = NULL;
    /* A late PRESS_LOST after an owner hide must not restart hidden feedback. */
    if (!visible) {
        for (unsigned i = 0; i < keyboard->button_count; ++i)
            if (keyboard->buttons[i] == lv_event_get_target(event)) stop_button(keyboard, i);
    }
}

static void root_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_DELETE) { input_event(event); return; }
    if (lv_event_get_target(event) != lv_event_get_current_target(event)) return;
    /* Children own their damped-button animations and free them on DELETE.
     * No global keyboard pointers, cursor timers or deferred callbacks exist. */
    lv_mem_free(lv_event_get_user_data(event));
}

static void cancel_event(lv_event_t *event)
{
    lv_alnum_keyboard_t *keyboard = lv_event_get_user_data(event);
    lv_alnum_keyboard_cancel_cb_t callback = keyboard->cancel;
    void *context = keyboard->context;
    if (!lv_alnum_keyboard_is_visible(keyboard)) return;
    lv_alnum_keyboard_hide(keyboard);
    if (callback) callback(context);
}

static void apply_event(lv_event_t *event)
{
    lv_alnum_keyboard_t *keyboard = lv_event_get_user_data(event);
    lv_alnum_keyboard_submit_cb_t callback = keyboard->submit;
    void *context = keyboard->context;
    char text[LV_ALNUM_KEYBOARD_MAX_TEXT + 1];
    if (!lv_alnum_keyboard_is_visible(keyboard)) return;
    memcpy(text, keyboard->text, sizeof(text));
    lv_alnum_keyboard_hide(keyboard);
    /* The callback may destroy keyboard or its parent. */
    callback(text, context);
}

static void clear_event(lv_event_t *event)
{
    lv_alnum_keyboard_t *keyboard = lv_event_get_user_data(event);
    if (!lv_alnum_keyboard_is_visible(keyboard)) return;
    keyboard->text[0] = '\0';
    refresh_text(keyboard);
}

static void backspace_event(lv_event_t *event)
{
    lv_alnum_keyboard_t *keyboard = lv_event_get_user_data(event);
    if (!lv_alnum_keyboard_is_visible(keyboard)) return;
    size_t length = strlen(keyboard->text);
    if (!length) return;
    keyboard->text[length - 1] = '\0';
    refresh_text(keyboard);
}

static void key_event(lv_event_t *event)
{
    lv_alnum_keyboard_t *keyboard = lv_event_get_user_data(event);
    if (!lv_alnum_keyboard_is_visible(keyboard)) return;
    lv_obj_t *label = lv_damped_button_get_label(lv_event_get_target(event));
    if (!label) return;
    const char *key = lv_label_get_text(label);
    size_t length = strlen(keyboard->text);
    if (!key || !key[0] || key[1] || length >= keyboard->max_length) return;
    keyboard->text[length] = key[0];
    keyboard->text[length + 1] = '\0';
    refresh_text(keyboard);
}

static void symbols_event(lv_event_t *event)
{
    static const char letters[] = "1234567890QWERTYUIOPASDFGHJKLZXCVBNM";
    static const char symbols[] = "1234567890.:-<>=/\\?!@#$%&*()_+,;\"'[]";
    _Static_assert(sizeof(letters) == CHARACTER_COUNT + 1, "letter page size");
    _Static_assert(sizeof(symbols) == CHARACTER_COUNT + 1, "symbol page size");
    lv_alnum_keyboard_t *keyboard = lv_event_get_user_data(event);
    if (!lv_alnum_keyboard_is_visible(keyboard)) return;
    keyboard->symbol_page = !keyboard->symbol_page;
    const char *page = keyboard->symbol_page ? symbols : letters;
    for (unsigned i = 0; i < CHARACTER_COUNT; ++i) {
        stop_button(keyboard, i);
        char key[2] = { page[i], '\0' };
        lv_damped_button_set_text(keyboard->buttons[i], key);
    }
    lv_damped_button_set_text(lv_event_get_target(event),
                              keyboard->symbol_page ? "ABC" : "#+=");
}

static lv_obj_t *button_create(lv_alnum_keyboard_t *keyboard, lv_obj_t *parent,
                               const char *text, const lv_font_t *font,
                               int x, int y, int width, int height,
                               uint32_t color, lv_event_cb_t callback)
{
    if (keyboard->button_count >= BUTTON_COUNT) return NULL;
    const lv_damped_button_style_t style = {
        color, color, 0xEFF2F4, color == ACTION_COLOR ? 0xFFFFFF : TEXT_COLOR,
        MUTED_COLOR, 7
    };
    lv_obj_t *button = lv_damped_button_create(parent, &style, text, font);
    if (!button || !lv_damped_button_get_label(button)) {
        if (button) lv_obj_del(button);
        return NULL;
    }
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(LINE_COLOR), 0);
    lv_obj_set_style_border_opa(button, LV_OPA_COVER, 0);
    if (!lv_obj_add_event_cb(button, input_event, LV_EVENT_ALL, keyboard) ||
        !lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, keyboard)) {
        lv_obj_del(button);
        return NULL;
    }
    keyboard->buttons[keyboard->button_count] = button;
    keyboard->button_colors[keyboard->button_count++] = color;
    return button;
}

lv_alnum_keyboard_t *lv_alnum_keyboard_create(lv_obj_t *parent,
    const lv_alnum_keyboard_config_t *config)
{
    if (!parent || !config || !config->submit || !config->max_length ||
        config->max_length > LV_ALNUM_KEYBOARD_MAX_TEXT) return NULL;
    lv_alnum_keyboard_t *keyboard = lv_mem_alloc(sizeof(*keyboard));
    if (!keyboard) return NULL;
    memset(keyboard, 0, sizeof(*keyboard));
    keyboard->max_length = config->max_length;
    keyboard->submit = config->submit;
    keyboard->cancel = config->cancel;
    keyboard->context = config->context;
    keyboard->root = plain(parent, 0, 0, KEYBOARD_WIDTH, KEYBOARD_HEIGHT, 0x17212A, 0);
    if (!keyboard->root) { lv_mem_free(keyboard); return NULL; }
    lv_obj_set_style_bg_opa(keyboard->root, LV_OPA_40, 0);
    lv_obj_add_flag(keyboard->root, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(keyboard->root, LV_OBJ_FLAG_GESTURE_BUBBLE);
    if (!lv_obj_add_event_cb(keyboard->root, root_event, LV_EVENT_ALL, keyboard)) {
        lv_obj_del(keyboard->root);
        lv_mem_free(keyboard);
        return NULL;
    }

    lv_obj_t *panel = plain(keyboard->root, 80, 40, PANEL_WIDTH, PANEL_HEIGHT, 0xF6F8F9, 16);
    if (!panel) goto failed;
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(LINE_COLOR), 0);
    if (!label_create(panel, config->title, &lv_font_instrument_sans_semibold_20,
                       0x17212A, 24, 14, 834, 29)) goto failed;

    lv_obj_t *input = plain(panel, 24, 56, 1072, 44, 0xFFFFFF, 7);
    if (!input) goto failed;
    lv_obj_set_style_border_width(input, 1, 0);
    lv_obj_set_style_border_color(input, lv_color_hex(0xC7D3DB), 0);
    keyboard->placeholder = label_create(input, config->placeholder,
        &lv_font_instrument_sans_medium_18, MUTED_COLOR, 17, 11, 1036, 26);
    keyboard->value_label = label_create(input, "", &lv_font_instrument_sans_medium_22,
        TEXT_COLOR, 14, 8, 1038, 30);
    keyboard->cursor = plain(input, 16, 10, 1, 24, MUTED_COLOR, 0);
    if (!keyboard->placeholder || !keyboard->value_label || !keyboard->cursor) goto failed;

    static const char *const rows[] = { "1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
    for (unsigned row = 0; row < sizeof(rows) / sizeof(rows[0]); ++row) {
        size_t count = strlen(rows[row]);
        int x = 24 + (10 - (int)count) * 42;
        for (size_t col = 0; col < count; ++col) {
            char key[2] = { rows[row][col], '\0' };
            if (!button_create(keyboard, panel, key, &lv_font_instrument_sans_medium_20,
                               x + (int)col * 84, 112 + row * 45, 78, 39,
                               KEY_COLOR, key_event)) goto failed;
        }
    }
    if (!button_create(keyboard, panel, LV_SYMBOL_BACKSPACE, &lv_font_montserrat_20,
                       880, 112, 216, 39, KEY_COLOR, backspace_event) ||
        !button_create(keyboard, panel, config->clear_text, &lv_font_instrument_sans_medium_18,
                       880, 157, 216, 39, KEY_COLOR, clear_event) ||
        !button_create(keyboard, panel, config->apply_text, &lv_font_instrument_sans_semibold_20,
                       880, 202, 216, 84, ACTION_COLOR, apply_event)) goto failed;
    if (config->symbols && !button_create(keyboard, panel, "#+=",
            &lv_font_instrument_sans_medium_18, 24, 247, 112, 39,
            KEY_COLOR, symbols_event)) goto failed;

    lv_obj_t *cancel = lv_nav_button_create(panel, 880, 9, 216, 36, cancel_event, keyboard);
    if (!cancel || !lv_damped_button_get_label(cancel) ||
        !lv_obj_add_event_cb(cancel, input_event, LV_EVENT_ALL, keyboard)) goto failed;
    lv_damped_button_set_text(cancel, config->cancel_text);
    keyboard->buttons[keyboard->button_count] = cancel;
    keyboard->button_colors[keyboard->button_count++] = CANCEL_COLOR;
    refresh_text(keyboard);
    return keyboard;

failed:
    lv_obj_del(keyboard->root);
    return NULL;
}

bool lv_alnum_keyboard_set_text(lv_alnum_keyboard_t *keyboard, const char *text)
{
    if (!keyboard || !text) return false;
    size_t length = 0;
    while (text[length]) {
        unsigned char ch = (unsigned char)text[length];
        if (ch < 32 || ch > 126 || length >= keyboard->max_length) return false;
        ++length;
    }
    memmove(keyboard->text, text, length + 1);
    refresh_text(keyboard);
    return true;
}

const char *lv_alnum_keyboard_get_text(const lv_alnum_keyboard_t *keyboard)
{ return keyboard ? keyboard->text : ""; }

void lv_alnum_keyboard_show(lv_alnum_keyboard_t *keyboard)
{
    if (!keyboard) return;
    lv_obj_move_foreground(keyboard->root);
    lv_obj_clear_flag(keyboard->root, LV_OBJ_FLAG_HIDDEN);
}

void lv_alnum_keyboard_hide(lv_alnum_keyboard_t *keyboard)
{
    if (!keyboard) return;
    lv_indev_t *contact = keyboard->contact;
    keyboard->contact = NULL;
    if (contact) {
        lv_indev_reset(contact, NULL);
        lv_indev_wait_release(contact);
    }
    lv_obj_add_flag(keyboard->root, LV_OBJ_FLAG_HIDDEN);
    /* Damped color animations use a private context, not the button as var.
     * The palette API cancels those; translation animations use the object. */
    for (unsigned i = 0; i < keyboard->button_count; ++i) stop_button(keyboard, i);
}

bool lv_alnum_keyboard_is_visible(const lv_alnum_keyboard_t *keyboard)
{ return keyboard && lv_obj_is_visible(keyboard->root); }

void lv_alnum_keyboard_destroy(lv_alnum_keyboard_t *keyboard)
{ if (keyboard) lv_obj_del(keyboard->root); }
