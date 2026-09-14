#ifndef UN260_STARTUP_VISUAL_H
#define UN260_STARTUP_VISUAL_H
#include <stdint.h>
/* Before the first LVGL render: -1 unsafe to draw, 0 legacy, 1 adopted. */
int startup_visual_acquire(uint32_t *elapsed_ms);
#endif
