#ifndef UN260_APP_SERVICE_APP_COMMAND_RUNTIME_H
#define UN260_APP_SERVICE_APP_COMMAND_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

bool app_command_runtime_request_count_start(void);
bool app_command_runtime_clear_counting_data(const char *reason);
void app_command_runtime_process_frames(void);
void app_command_runtime_poll(uint32_t now_ms);

#endif
