#include "app_touch_sound.h"
#include "buzzer_service.h"
#include "un260/gesture/touch_sound.h"
#include "un260/protocol/protocol_send.h"
#include "lv_port_indev.h"
#include "un260/lv_system/user_cfg.h"

static bool pending;
static void request_click(void) { pending = true; }

/* One accepted touch has the same feedback policy on every page. Machine
 * setting requests and their replies do not revoke an already accepted tap. */
void app_touch_sound_poll(void)
{
    if(!user_cfg_touch_sound_enabled()) {
        pending = false;
        buzzer_service_cancel();
        return;
    }
    if(!pending) return;
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
