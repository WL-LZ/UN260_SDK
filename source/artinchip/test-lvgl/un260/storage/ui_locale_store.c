#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "ui_locale_store.h"
#include "ui_locale_reader.h"
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

bool ui_locale_store_read(char tag[UI_LOCALE_TAG_CAPACITY])
{
    return ui_locale_read_file(LOCALE_PATH, tag);
}

bool ui_locale_store_write(const char *tag)
{
    if (!ui_locale_tag_valid(tag)) return false;
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
