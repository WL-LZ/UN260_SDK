#ifndef BOOT_WELCOME_LOCALE_H
#define BOOT_WELCOME_LOCALE_H
#include "un260/storage/ui_locale_reader.h"
#include <string.h>
#include <stdint.h>
#include "boot_welcome_registry.h"
/* Bounded read only; neither prepares general UI fonts nor changes UI language.
 * The boot welcome is a snapshot, so asynchronous preferences cannot swap it
 * halfway through its animation. Unknown/corrupt tags retain English. */
static inline unsigned boot_welcome_read_index(const char *path)
{
    char tag[UI_LOCALE_TAG_CAPACITY];
    if(!ui_locale_read_file(path,tag))return 0;
    for(unsigned i=0;i<BOOT_WELCOME_COUNT;++i)
        if(!strcmp(tag,boot_welcome_tags[i]))return i;
    return 0;
}
#ifndef BOOT_WELCOME_LOCALE_PATH
#define BOOT_WELCOME_LOCALE_PATH "/etc/ui_state/language.cfg"
#endif
static inline unsigned boot_welcome_initial_index(unsigned language_id)
{
    for(unsigned i=1;i<BOOT_WELCOME_COUNT;++i)
        if(boot_welcome_ids[i]==language_id)return i;
    return boot_welcome_read_index(BOOT_WELCOME_LOCALE_PATH);
}
#endif
