#ifndef APP_STANDBY_RUNTIME_H
#define APP_STANDBY_RUNTIME_H
#include <stdbool.h>
#include <stdint.h>
void app_standby_runtime_poll(uint32_t now);
void app_standby_runtime_enter(void);
/* UI-thread policy shared by explicit entry and the inactivity timer.
 * NULL means entry is allowed; the returned message is an untranslated key. */
const char *app_standby_runtime_blocker(void);
/* An explicit request can present unread reports; the idle timer never does. */
bool app_standby_runtime_needs_fault_review(void);
bool app_standby_runtime_request(void);
/* UI-thread notification, once per newly received validated protocol frame. */
void app_standby_runtime_protocol_activity(void);
/* Called before global gestures. Capture the complete wake contact sequence. */
bool app_standby_runtime_touch(bool down);
#endif
