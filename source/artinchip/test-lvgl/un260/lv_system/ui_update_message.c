#include "ui_update_message.h"
#include "ui_i18n.h"
#include <stdio.h>
#include <string.h>

typedef struct { unsigned args; const char *key, *prefix, *middle, *suffix; } update_template_t;
static const update_template_t templates[] = {
#include "ui_update_templates.inc"
};
static void complete_utf8(char *out)
{
    size_t length = strlen(out), start = length;
    while (start && ((unsigned char)out[start-1] & 0xc0) == 0x80) --start;
    if (!start) return;
    unsigned char c = (unsigned char)out[start-1];
    size_t need = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc0 ? 2 : 1;
    if (length-(start-1) < need) out[start-1] = 0;
}

void ui_update_message_render(language_t locale, const char *raw, char *out, size_t capacity)
{
    if (!out || !capacity) return;
    if (!raw) raw = "";
    /* Match complete known templates; caller filenames and numbers are data.
     * Never feed an external diagnostic to printf as a format string. */
    for (size_t i = 0; i < sizeof(templates)/sizeof(templates[0]); ++i) {
        const update_template_t *t = &templates[i];
        size_t prefix = strlen(t->prefix), length = strlen(raw);
        if (length < prefix || strncmp(raw, t->prefix, prefix)) continue;
        const char *first = raw + prefix;
        size_t suffix = strlen(t->suffix);
        if (length-prefix < suffix || (suffix && strcmp(raw+length-suffix, t->suffix))) continue;
        const char *separator = t->args == 2 ? strstr(first, t->middle) : raw + length-suffix;
        if (!separator) continue;
        const char *second = t->args == 2 ? separator + strlen(t->middle) : "";
        size_t remaining = strlen(second);
        if (t->args == 2 && suffix > remaining) continue;
        char a[192], b[96];
        size_t a_len = (size_t)(separator-first), b_len = t->args == 2 ? remaining-suffix : 0;
        if (a_len >= sizeof(a) || b_len >= sizeof(b)) continue;
        memcpy(a, first, a_len); a[a_len] = 0;
        memcpy(b, second, b_len); b[b_len] = 0;
        const char *format = ui_tr_for(locale, t->key);
        if (t->args == 2) snprintf(out, capacity, format, a, b);
        else snprintf(out, capacity, format, a);
        complete_utf8(out);
        return;
    }
    snprintf(out, capacity, "%s", ui_tr_for(locale, raw));
    complete_utf8(out);
}
