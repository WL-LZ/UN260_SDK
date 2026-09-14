#ifndef UN260_APP_STARTUP_TRACE_H
#define UN260_APP_STARTUP_TRACE_H

/* UI thread, startup milestones only. Opt in with UN260_BOOT_TRACE=1. */
void app_startup_trace_mark(const char *stage);

#endif
