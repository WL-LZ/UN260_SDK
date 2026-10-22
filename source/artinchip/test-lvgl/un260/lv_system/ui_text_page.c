#include "ui_text.h"
#include "un260/counting/counting_reject_reason.h"

const char *const g_ui_text_keys[UI_TEXT_MAX] = {
#include "i18n/generated/ui_text_keys.inc"
};

const char* ui_text_counting_reject_reason(uint8_t code)
{
    return counting_reject_reason_get(code);
}
