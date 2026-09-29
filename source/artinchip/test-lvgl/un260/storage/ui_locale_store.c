#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "ui_locale_store.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef UI_STATE_DIR
#define UI_STATE_DIR "/etc/ui_state"
#endif
#define LOCALE_PATH UI_STATE_DIR "/language.cfg"
#define LOCALE_TEMP UI_STATE_DIR "/language.cfg.tmp"

static bool valid_tag(const char *tag)
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

bool ui_locale_store_read(char tag[UI_LOCALE_TAG_CAPACITY])
{
    if (!tag) return false;
    memcpy(tag, "en", 3);
    FILE *fp = fopen(LOCALE_PATH, "r");
    if (!fp) return false;
    char buffer[UI_LOCALE_TAG_CAPACITY + 2];
    size_t n = fread(buffer, 1, sizeof(buffer), fp);
    bool ok = n > 0 && n < sizeof(buffer) && !ferror(fp);
    if (ok) {
        if (n && buffer[n - 1] == '\n') --n;
        if (n && buffer[n - 1] == '\r') --n;
        /* Reject embedded NULs rather than truncating a malformed file into a tag. */
        ok = n < UI_LOCALE_TAG_CAPACITY && !memchr(buffer, 0, n);
        if (ok) { buffer[n] = 0; ok = valid_tag(buffer); }
    }
    if (fclose(fp) != 0) ok = false;
    if (ok) memcpy(tag, buffer, strlen(buffer) + 1);
    return ok;
}

bool ui_locale_store_write(const char *tag)
{
    if (!valid_tag(tag)) return false;
    if (mkdir(UI_STATE_DIR, 0755) != 0 && errno != EEXIST) return false;
    int dir = open(UI_STATE_DIR, O_RDONLY | O_DIRECTORY);
    if (dir < 0) return false;
    FILE *fp = fopen(LOCALE_TEMP, "w");
    if (!fp) { close(dir); return false; }
    bool ok = fprintf(fp, "%s\n", tag) >= 0 && fflush(fp) == 0;
    if (ok && fsync(fileno(fp)) != 0) ok = false;
    if (fclose(fp) != 0) ok = false;
    if (!ok) { unlink(LOCALE_TEMP); close(dir); return false; }
    if (rename(LOCALE_TEMP, LOCALE_PATH) != 0) { unlink(LOCALE_TEMP); close(dir); return false; }
    /* A failed directory flush leaves durability uncertain. Report failure and
     * keep the active UI locale, even though the new file may exist on disk. */
    ok = fsync(dir) == 0;
    close(dir);
    return ok;
}
