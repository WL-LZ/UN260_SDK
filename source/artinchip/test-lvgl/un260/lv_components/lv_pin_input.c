#include "lv_pin_input.h"

#include <string.h>

void lv_pin_input_clear(lv_pin_input_t *input)
{
    if (input) memset(input, 0, sizeof(*input));
}

bool lv_pin_input_set(lv_pin_input_t *input, const char *value)
{
    size_t length = 0;
    if (!input) return false;
    lv_pin_input_clear(input);
    if (!value) return true;
    while (value[length] && length < LV_PIN_DIGITS) {
        if (value[length] < '0' || value[length] > '9') return false;
        ++length;
    }
    if (value[length]) return false;
    memcpy(input->value, value, length);
    input->length = length;
    return true;
}

bool lv_pin_input_push(lv_pin_input_t *input, unsigned digit)
{
    if (!input || digit > 9 || input->length >= LV_PIN_DIGITS) return false;
    input->value[input->length++] = (char)('0' + digit);
    input->value[input->length] = '\0';
    return true;
}

bool lv_pin_input_backspace(lv_pin_input_t *input)
{
    if (!input || !input->length) return false;
    input->value[--input->length] = '\0';
    return true;
}

bool lv_pin_input_is_complete(const char *value)
{
    if (!value) return false;
    for (size_t i = 0; i < LV_PIN_DIGITS; ++i) {
        if (value[i] < '0' || value[i] > '9') return false;
    }
    return value[LV_PIN_DIGITS] == '\0';
}
