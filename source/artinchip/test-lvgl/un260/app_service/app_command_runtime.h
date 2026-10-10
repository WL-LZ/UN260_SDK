#ifndef UN260_APP_SERVICE_APP_COMMAND_RUNTIME_H
#define UN260_APP_SERVICE_APP_COMMAND_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

bool app_command_runtime_request_count_start(void);
/* NULL means ordinary manual RUN is currently safe in a diagnostic scope. */
const char *app_command_runtime_diagnostic_run_blocker(void);
const char *app_command_runtime_calibration_blocker(void);
bool app_command_runtime_request_diagnostic_run(void);
/* Pending command or a controller-confirmed counting session. */
bool app_command_runtime_count_start_busy(void);
/* UI-consumer-owned accepted run completion counter, retained across resets.
 * An entry snapshot and inequality detect even a start/end in one dispatch tick.
 * Boot, duplicate, unknown or otherwise ignored end frames do not advance it. */
uint32_t app_command_runtime_count_end_generation(void);
bool app_command_runtime_result_pending(void);
typedef enum {
    APP_UI_UPGRADE_READY = 0,
    APP_UI_UPGRADE_WAIT,
    APP_UI_UPGRADE_BUSY,
    APP_UI_UPGRADE_FAILED,
} app_ui_upgrade_readiness_t;
/* UI-thread, nonblocking: freeze current results into their owning history
 * queue, then wait for local writes. Does not clear controller/counting data. */
app_ui_upgrade_readiness_t app_command_runtime_prepare_ui_upgrade(void);
bool app_command_runtime_clear_counting_data(const char *reason);
void app_command_runtime_process_frames(void);
/* A frame is indivisible. Zero budget dispatches at most one ready frame.
 * History backpressure retains one transition frame and stops later dequeues. */
uint32_t app_command_runtime_process_frames_budget(uint32_t budget_us);
/* Runnable work: false while a retained frame is storage-blocked (10ms retry). */
bool app_command_runtime_frames_pending(void);
/* UI consumer thread only: observe its retained frame without consuming or
 * reordering it. Includes replies waiting for history preflight to recover. */
bool app_command_runtime_has_command_between(uint8_t command,
                                              uint64_t started_ms,
                                              uint64_t deadline_ms);
void app_command_runtime_poll(uint32_t now_ms);

#endif
