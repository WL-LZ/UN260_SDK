#ifndef LV_PIN_INPUT_H
#define LV_PIN_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#define LV_PIN_DIGITS 4

/* A PIN is always an ASCII digit string, including leading zeroes. */
typedef struct {
    char value[LV_PIN_DIGITS + 1];
    size_t length;
} lv_pin_input_t;

void lv_pin_input_clear(lv_pin_input_t *input);
bool lv_pin_input_set(lv_pin_input_t *input, const char *value);
bool lv_pin_input_push(lv_pin_input_t *input, unsigned digit);
bool lv_pin_input_backspace(lv_pin_input_t *input);
bool lv_pin_input_is_complete(const char *value);

#endif
