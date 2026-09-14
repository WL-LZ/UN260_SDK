#ifndef APP_STARTUP_RUNTIME_H
#define APP_STARTUP_RUNTIME_H
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_STARTUP_WAITING = 0,
    APP_STARTUP_READY,
    APP_STARTUP_FAILED,
    APP_STARTUP_TIMED_OUT
} app_startup_status_t;

/* One-shot owner of startup I/O and serial creation. Until can_process(), main
 * renders only: no protocol or settings consumers may run. READY additionally
 * requires history completion. poll publishes preferences and reaps the worker;
 * no LVGL API is used by that worker. */
bool app_startup_runtime_begin(uint32_t now_ms);
app_startup_status_t app_startup_runtime_poll(uint32_t now_ms);
bool app_startup_runtime_is_settled(void);
/* History may still be loading. The existing protocol preflight owns record
 * backpressure; do not hold unrelated replies behind a slow history read. */
bool app_startup_runtime_can_process(void);
#endif
