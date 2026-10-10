#ifndef UI_LOCALE_READER_H
#define UI_LOCALE_READER_H
#include "ui_locale_store.h"
#include <stdio.h>
#include <string.h>
/* Shared read-only parser for normal storage and pre-switch-root startup.
 * Kept independent of UI/font libraries; never writes or repairs a file. */
static inline bool ui_locale_tag_valid(const char *tag)
{
    if (!tag || !*tag) return false;
    size_t n = 0;
    for (; tag[n]; ++n) {
        unsigned char c = (unsigned char)tag[n];
        if (n >= UI_LOCALE_TAG_CAPACITY - 1 ||
            !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (n && c >= '0' && c <= '9') || (n && c == '-'))) return false;
    }
    return tag[n - 1] != '-';
}

static inline bool ui_locale_read_file(const char *path, char tag[UI_LOCALE_TAG_CAPACITY])
{
    if (!tag || !path) return false;
    memcpy(tag, "en", 3);
    FILE *fp = fopen(path, "r");
    if (!fp) return false;
    char buffer[UI_LOCALE_TAG_CAPACITY + 2];
    size_t n = fread(buffer, 1, sizeof(buffer), fp);
    bool ok = n > 0 && n < sizeof(buffer) && !ferror(fp);
    if (ok) {
        if (n && buffer[n - 1] == '\n') --n;
        if (n && buffer[n - 1] == '\r') --n;
        /* Reject embedded NULs rather than truncating a malformed file into a tag. */
        ok = n < UI_LOCALE_TAG_CAPACITY && !memchr(buffer, 0, n);
        if (ok) { buffer[n] = 0; ok = ui_locale_tag_valid(buffer); }
    }
    if (fclose(fp) != 0) ok = false;
    if (ok) memcpy(tag, buffer, strlen(buffer) + 1);
    return ok;
}

#endif
