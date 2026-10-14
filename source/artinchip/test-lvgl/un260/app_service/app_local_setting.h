#ifndef APP_LOCAL_SETTING_H
#define APP_LOCAL_SETTING_H
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    APP_LOCAL_LAYOUT, APP_LOCAL_GESTURES, APP_LOCAL_FAULT_POPUP,
    APP_LOCAL_TOUCH, APP_LOCAL_TOP_POPUP, APP_LOCAL_SHORTCUT_VISIBILITY, APP_LOCAL_MAIN_BATCH_ACTION,
    APP_LOCAL_TOUCH_SOUND,
    APP_LOCAL_SETTING_COUNT
} app_local_setting_t;
/* A user commit, not startup restoration. a carries the setting value;
 * visibility uses a as action, b as shown.
 * Completion feedback belongs here. */
bool app_local_setting_apply(app_local_setting_t setting, uint8_t a, uint8_t b);
#endif
