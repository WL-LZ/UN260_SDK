#ifndef UN260_APP_SERVICE_APP_RUNTIME_WAKEUP_H
#define UN260_APP_SERVICE_APP_RUNTIME_WAKEUP_H

#include <stdint.h>

#define APP_RUNTIME_MAX_WAIT_MS 10U

/* Capture before draining work, not immediately before sleeping. A producer
 * notification between the drain and wait must make the wait return at once. */
uint64_t app_runtime_wakeup_snapshot(void);
void app_runtime_wakeup_notify(void);
void app_runtime_wakeup_wait_since(uint64_t sequence, uint32_t max_wait_ms);

#endif
