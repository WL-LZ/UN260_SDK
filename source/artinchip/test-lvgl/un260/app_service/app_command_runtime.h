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
bool app_command_runtime_clear_counting_data(const char *reason);
void app_command_runtime_process_frames(void);
/* A frame is indivisible. Zero budget dispatches at most one ready frame.
 * History backpressure retains one transition frame and stops later dequeues. */
uint32_t app_command_runtime_process_frames_budget(uint32_t budget_us);
/* Runnable work: false while a retained frame is storage-blocked (10ms retry). */
bool app_command_runtime_frames_pending(void);
void app_command_runtime_poll(uint32_t now_ms);

#endif
