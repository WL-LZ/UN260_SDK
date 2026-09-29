#include "ui_text.h"
#include <stddef.h>
extern const char *const g_ui_text_keys[UI_TEXT_MAX];
const char *ui_text_msgid(ui_text_id_t text_id)
{
    unsigned index = (unsigned)text_id;
    return index < UI_TEXT_MAX && g_ui_text_keys[index] ? g_ui_text_keys[index] : "";
}
const char *ui_text_get(ui_text_id_t text_id)
{
    return ui_tr(ui_text_msgid(text_id));
}
