#include "un260/storage/ui_locale_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef UI_STATE_DIR
#error Run with an isolated temporary UI_STATE_DIR
#endif
static void raw(const char *value)
{
    FILE *file = fopen(UI_STATE_DIR "/language.cfg", "w"); assert(file);
    assert(fputs(value, file) >= 0); assert(fclose(file) == 0);
}
int main(void)
{
    char tag[UI_LOCALE_TAG_CAPACITY];
    assert(!ui_locale_store_read(tag) && !strcmp(tag, "en"));
    assert(ui_locale_store_write("en"));
    assert(ui_locale_store_read(tag) && !strcmp(tag, "en"));
    assert(ui_locale_store_write("zh-Hans"));
    assert(ui_locale_store_read(tag) && !strcmp(tag, "zh-Hans"));
    assert(mkdir(UI_STATE_DIR "/language.cfg.tmp", 0755) == 0);
    assert(!ui_locale_store_write("en"));
    assert(ui_locale_store_read(tag) && !strcmp(tag, "zh-Hans"));
    assert(rmdir(UI_STATE_DIR "/language.cfg.tmp") == 0);
    assert(!ui_locale_store_write("../en"));
    assert(!ui_locale_store_write("en\nko"));
    assert(!ui_locale_store_write(""));
    assert(!ui_locale_store_write(NULL));
    assert(ui_locale_store_read(tag) && !strcmp(tag, "zh-Hans"));
    raw("unknown-LOCALE\n"); assert(ui_locale_store_read(tag)); /* Selection policy is not storage's job. */
    raw("en\nko\n"); assert(!ui_locale_store_read(tag) && !strcmp(tag, "en"));
    raw("\n"); assert(!ui_locale_store_read(tag));
    raw("en "); assert(!ui_locale_store_read(tag));
    FILE *binary = fopen(UI_STATE_DIR "/language.cfg", "wb"); assert(binary);
    const char malformed[] = "en\0junk\n";
    assert(fwrite(malformed, 1, sizeof(malformed) - 1, binary) == sizeof(malformed) - 1);
    assert(fclose(binary) == 0);
    assert(!ui_locale_store_read(tag) && !strcmp(tag, "en"));
    raw("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"); assert(!ui_locale_store_read(tag));
    unlink(UI_STATE_DIR "/language.cfg"); rmdir(UI_STATE_DIR);
    puts("Locale storage PASS: atomic tag roundtrip and malformed/failure preservation");
    return 0;
}
