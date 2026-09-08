#ifndef LV_PIN_KEYPAD_H
#define LV_PIN_KEYPAD_H

#include "lvgl/lvgl.h"
#include "lv_pin_input.h"

#define LV_PIN_KEYPAD_WIDTH 1120
#define LV_PIN_KEYPAD_HEIGHT 320

typedef void (*lv_pin_keypad_confirm_cb_t)(const char *pin, void *user_data);
typedef void (*lv_pin_keypad_cancel_cb_t)(void *user_data);
/* Optional synchronous preference writer. Persist only the boolean, never
 * the PIN. Return false to leave the current visibility unchanged. */
typedef bool (*lv_pin_keypad_visibility_cb_t)(bool visible);

typedef struct {
    const char *eyebrow;
    const char *title;
    const char *prompt;
    lv_pin_keypad_confirm_cb_t confirm_cb;
    lv_pin_keypad_cancel_cb_t cancel_cb;
    void *user_data;
    bool digits_visible;
    lv_pin_keypad_visibility_cb_t save_visibility;
} lv_pin_keypad_config_t;

/* Zero-initialize and keep this context alive until its parent is deleted or
 * lv_pin_keypad_destroy returns. No global keypad, callbacks, or blink timer. */
typedef struct {
    lv_obj_t *root;
    lv_obj_t *eyebrow;
    lv_obj_t *title;
    lv_obj_t *prompt;
    lv_obj_t *dots[LV_PIN_DIGITS];
    lv_obj_t *digits[LV_PIN_DIGITS];
    lv_obj_t *eye;
    lv_obj_t *cursor;
    lv_obj_t *status;
    lv_obj_t *keys[12];
    lv_obj_t *cancel;
    lv_timer_t *blink;
    lv_pin_input_t input;
    lv_pin_keypad_confirm_cb_t confirm_cb;
    lv_pin_keypad_cancel_cb_t cancel_cb;
    void *user_data;
    bool cursor_on;
    bool digits_visible;
    lv_pin_keypad_visibility_cb_t save_visibility;
} lv_pin_keypad_t;

bool lv_pin_keypad_create(lv_pin_keypad_t *keypad, lv_obj_t *parent,
                          lv_coord_t x, lv_coord_t y);
bool lv_pin_keypad_show(lv_pin_keypad_t *keypad,
                        const lv_pin_keypad_config_t *config,
                        const char *initial_value);
void lv_pin_keypad_set_status(lv_pin_keypad_t *keypad, const char *text);
void lv_pin_keypad_clear(lv_pin_keypad_t *keypad);
/* Hiding discards draft PIN/callbacks and pauses its timer; show starts fresh. */
void lv_pin_keypad_hide(lv_pin_keypad_t *keypad);
void lv_pin_keypad_destroy(lv_pin_keypad_t *keypad);
bool lv_pin_keypad_is_visible(const lv_pin_keypad_t *keypad);

#endif
