#include "app_touch_sound.h"
#include "buzzer_service.h"
#include "un260/gesture/touch_sound.h"
#include "un260/protocol/protocol_send.h"
#include "lv_port_indev.h"
#include "un260/lv_system/user_cfg.h"

/* A quick local preset save can produce a wire request on the next UI tick.
 * A slow storage operation must still give its tap audible feedback. */
#define TOUCH_BUSINESS_GRACE_MS 60U
static bool pending;
static uint32_t requested_at;
static void request_click(void)
{
    if (!pending) requested_at = lv_tick_get();
    pending = true;
}

/* One accepted touch has the same feedback policy on every page. Machine
 * setting requests and their replies do not revoke an already accepted tap. */
void app_touch_sound_poll(bool business_preparing)
{
    if(!user_cfg_touch_sound_enabled()) {
        pending = false;
        buzzer_service_cancel();
        return;
    }
    if(!pending) return;
    if (business_preparing && protocol_send_is_ready() &&
        (uint32_t)(lv_tick_get() - requested_at) < TOUCH_BUSINESS_GRACE_MS) return;
    pending = false;
    if(!protocol_send_is_ready()) return;
    buzzer_service_click();
}
void app_touch_sound_init(void)
{
    pending = false;
    user_cfg_touch_sound_load();
    touch_sound_init(request_click);
    lv_port_indev_set_contact_observer(touch_sound_contact);
    lv_port_indev_set_feedback_observer(touch_sound_feedback);
}
