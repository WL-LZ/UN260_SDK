#ifndef UI_DEFERRED_ACTION_H
#define UI_DEFERRED_ACTION_H

#include "lvgl/lvgl.h"

/* Main/LVGL thread only. Zero-initialize the owner and cancel it before its
 * storage is released. At most one pending action is owned by each slot. */
typedef struct {
    lv_timer_t *timer;
    lv_async_cb_t callback;
    void *user_data;
} ui_deferred_action_t;

/* Replaces a pending action. On allocation failure, leaves the owner empty and
 * returns false; the caller decides whether to run its synchronous fallback. */
bool ui_deferred_action_schedule(ui_deferred_action_t *owner,
                                 lv_async_cb_t callback, void *user_data);
void ui_deferred_action_cancel(ui_deferred_action_t *owner);

#endif
