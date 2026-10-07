#ifndef APP_LOCAL_SETTING_H
#define APP_LOCAL_SETTING_H
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    APP_LOCAL_LAYOUT, APP_LOCAL_GESTURES, APP_LOCAL_FAULT_POPUP,
    APP_LOCAL_TOUCH, APP_LOCAL_TOP_POPUP, APP_LOCAL_SHORTCUT_ORDER,
    APP_LOCAL_SETTING_COUNT
} app_local_setting_t;
/* A user commit, not startup restoration. a is the desired boolean value;
 * shortcut order uses a/b as positions. Completion feedback belongs here. */
bool app_local_setting_apply(app_local_setting_t setting, uint8_t a, uint8_t b);
#endif
