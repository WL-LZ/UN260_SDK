#include "ui_deferred_action.h"

void ui_deferred_action_cancel(ui_deferred_action_t *owner)
{
    lv_timer_t *timer;

    if (owner == NULL) return;
    timer = owner->timer;
    owner->timer = NULL;
    owner->callback = NULL;
    owner->user_data = NULL;
    if (timer != NULL) lv_timer_del(timer);
}

static void ui_deferred_action_dispatch(lv_timer_t *timer)
{
    ui_deferred_action_t *owner = timer->user_data;
    lv_async_cb_t callback = owner->callback;
    void *user_data = owner->user_data;

    /* LVGL 8.3.2's lv_async trampoline frees its callback data AFTER calling
     * user code, so cancelling that running async action double-frees it.
     * Detach and delete our one-shot timer BEFORE calling user code: callbacks
     * may cancel/reuse/destroy their owner, including during page suspension. */
    owner->timer = NULL;
    owner->callback = NULL;
    owner->user_data = NULL;
    lv_timer_del(timer);
    callback(user_data);
    /* Do not access owner or timer here: user code may have released either. */
}

bool ui_deferred_action_schedule(ui_deferred_action_t *owner,
                                 lv_async_cb_t callback, void *user_data)
{
    if (owner == NULL || callback == NULL) return false;
    ui_deferred_action_cancel(owner);
    owner->timer = lv_timer_create(ui_deferred_action_dispatch, 0, owner);
    if (owner->timer == NULL) return false;
    owner->callback = callback;
    owner->user_data = user_data;
    return true;
}
