#ifndef LV_ALNUM_KEYBOARD_H
#define LV_ALNUM_KEYBOARD_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#define LV_ALNUM_KEYBOARD_MAX_TEXT 32

typedef struct lv_alnum_keyboard lv_alnum_keyboard_t;
typedef void (*lv_alnum_keyboard_submit_cb_t)(const char *text, void *context);
typedef void (*lv_alnum_keyboard_cancel_cb_t)(void *context);

typedef struct {
    const char *title;
    const char *placeholder;
    const char *apply_text;
    const char *clear_text;
    const char *cancel_text;
    uint8_t max_length;
    lv_alnum_keyboard_submit_cb_t submit;
    lv_alnum_keyboard_cancel_cb_t cancel;
    void *context;
    /* Optional printable-symbol page; false preserves the alphanumeric layout. */
    bool symbols;
    /* Opt-in search sheet; existing device-entry layouts remain unchanged. */
    bool modern;
} lv_alnum_keyboard_config_t;

/* Creates a hidden 1280x400 modal owned by parent. Labels are copied by LVGL;
 * config may be temporary. A valid max_length is 1..32. The callback text is
 * valid only during submit, which also runs when the text has not changed.
 * User apply/cancel hides before calling back; callbacks may delete the owner.
 * Programmatic hide/destroy never submit or call cancel. Deleting parent frees
 * this keyboard automatically and invalidates the caller's handle. */
lv_alnum_keyboard_t *lv_alnum_keyboard_create(lv_obj_t *parent,
    const lv_alnum_keyboard_config_t *config);

/* ASCII printable text only; too-long or non-ASCII input is rejected without
 * changing the current value. Character keys append; they never replace it. */
bool lv_alnum_keyboard_set_text(lv_alnum_keyboard_t *keyboard, const char *text);
const char *lv_alnum_keyboard_get_text(const lv_alnum_keyboard_t *keyboard);
void lv_alnum_keyboard_show(lv_alnum_keyboard_t *keyboard);
void lv_alnum_keyboard_hide(lv_alnum_keyboard_t *keyboard);
bool lv_alnum_keyboard_is_visible(const lv_alnum_keyboard_t *keyboard);
void lv_alnum_keyboard_destroy(lv_alnum_keyboard_t *keyboard);

#endif
