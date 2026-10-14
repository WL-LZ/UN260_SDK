#ifndef UN260_TOUCH_SOUND_H
#define UN260_TOUCH_SOUND_H
#include "lvgl/lvgl.h"
typedef void (*touch_sound_request_t)(void);
void touch_sound_init(touch_sound_request_t request);
/* Raw press/release brackets captured input; READY marks accepted taps only. */
void touch_sound_contact(bool down);
void touch_sound_feedback(lv_indev_t *indev, lv_event_code_t code);
#endif
