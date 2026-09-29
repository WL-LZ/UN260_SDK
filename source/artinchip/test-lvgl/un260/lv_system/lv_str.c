#include "lv_str.h"
#include "ui_text.h"
#include <string.h>
const char *get_str_by_name(const char *name)
{
    if (name && strcmp(name, "REJECT:") == 0)
        return ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_REJECT);
    return NULL;
}
