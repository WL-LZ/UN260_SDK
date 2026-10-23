#ifndef UN260_APP_SERVICE_APP_ADD_RUNTIME_H
#define UN260_APP_SERVICE_APP_ADD_RUNTIME_H

#include <stdbool.h>

/* UI-thread owner; retained across page visibility and reconstruction. */
bool app_add_runtime_request(bool target);
bool app_add_runtime_busy(void);
bool app_add_runtime_waiting_setting(void);
/* A retry only clears; it never resends or reverses an already-confirmed ADD. */
bool app_add_runtime_retry_clear(void);
void app_add_runtime_setting_result(bool target, bool success);
void app_add_runtime_setting_timeout(void);
/* Real controller START is preserved. False only defers it while a confirmed
 * CLEAR still needs its local history/result commit, retaining RX order. */
bool app_add_runtime_before_count_start(void);
void app_add_runtime_poll(void);

#endif
