#ifndef UN260_STARTUP_VISUAL_H
#define UN260_STARTUP_VISUAL_H
#include <stdbool.h>
#include <stdint.h>
#include "boot_light_protocol.h"
/* Probe a native first frame without stopping its renderer.
 * Returns 1 while a root player owns scanout, 0 for legacy/free fallback,
 * -1 if the lease cannot be checked. A slow probe is completed by poll;
 * snapshot is written only after receiving a supported immutable status. */
int startup_visual_begin(boot_light_status_t *snapshot);
/* While the root player owns scanout, poll its immutable boot snapshot.
 * ready requests normal handoff; error bypasses the brand hold for diagnosis.
 * Returns 0 while it draws, 1 after acquiring the lease, -1 if unsafe to draw.
 * Player failure can also return 1, but only after the lease is really free
 * and the caller has prepared a ready or error surface. */
int startup_visual_poll(bool ready, bool error);
/* Before the first LVGL render: -1 unsafe to draw, 0 legacy, 1 adopted. */
int startup_visual_acquire(uint32_t *elapsed_ms);
/* One RAM PID marker after an actual UI scanout; failure never blocks paint. */
void startup_visual_note_ui_presented(void);
#endif
