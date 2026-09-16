#ifndef APP_STANDBY_RUNTIME_H
#define APP_STANDBY_RUNTIME_H
#include <stdbool.h>
#include <stdint.h>
void app_standby_runtime_poll(uint32_t now);
void app_standby_runtime_enter(void);
/* UI-thread notification, once per newly received validated protocol frame. */
void app_standby_runtime_protocol_activity(void);
/* Called before global gestures. Capture the complete wake contact sequence. */
bool app_standby_runtime_touch(bool down);
#endif
