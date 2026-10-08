/* Standalone update display only. The LVGL application keeps ui_i18n.c.
 * Its emergency renderer currently supports Latin text; this frozen English
 * subset comes from the maintained catalogue, with literal fallback for raw
 * external diagnostics. No locale state or storage is needed by this process. */
#include "ui_i18n.h"
#include <stddef.h>
#include <string.h>

static const struct { const char *key, *text; } messages[] = {
#include "ui_update_catalog.inc"
};

const char *ui_tr_for(language_t locale, const char *key)
{
    (void)locale; /* The standalone renderer always requests LANGUAGE_EN. */
    if (!key) return "";
    size_t first=0, last=sizeof(messages)/sizeof(messages[0]);
    while(first<last) {
        size_t middle=first+(last-first)/2;
        int order=strcmp(key,messages[middle].key);
        if(order==0) return messages[middle].text;
        if(order<0) last=middle; else first=middle+1;
    }
    return key;
}
