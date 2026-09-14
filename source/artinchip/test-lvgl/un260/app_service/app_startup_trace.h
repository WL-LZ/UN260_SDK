#ifndef UN260_APP_STARTUP_TRACE_H
#define UN260_APP_STARTUP_TRACE_H

#include <stdbool.h>
#include <stdint.h>

/* UI thread, startup milestones only. Opt in with UN260_BOOT_TRACE=1. */
void app_startup_trace_mark(const char *stage);

/* Separate, default-off frame tracing; the caller attaches the observer only
 * during the intro and detaches before finishing or entering normal UI. */
bool app_startup_frames_begin(void);
void app_startup_frames_present(uint64_t present_us);
void app_startup_frames_finish(void);

#endif
