#ifndef UI_LOCALE_STORE_H
#define UI_LOCALE_STORE_H
#include <stdbool.h>
#define UI_LOCALE_TAG_CAPACITY 32
/* Storage contains a BCP 47 tag, independent of catalogue ordering. No UI calls. */
bool ui_locale_store_read(char tag[UI_LOCALE_TAG_CAPACITY]);
/* Success includes directory durability. On failure after rename the tag can
 * already exist, but durability was not confirmed; the UI must not claim success. */
bool ui_locale_store_write(const char *tag);
#endif
