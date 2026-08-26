#ifndef SCREEN_RECORDING_SERVICE_H
#define SCREEN_RECORDING_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SCREEN_RECORDING_IDLE = 0,
    SCREEN_RECORDING_STARTING,
    SCREEN_RECORDING_ACTIVE,
    SCREEN_RECORDING_STOPPING,
} screen_recording_state_t;

typedef enum {
    SCREEN_RECORDING_START_OK = 0,
    SCREEN_RECORDING_START_BUSY,
    SCREEN_RECORDING_START_USB_NOT_READY,
    SCREEN_RECORDING_START_FAILED,
} screen_recording_start_result_t;

typedef enum {
    SCREEN_RECORDING_COMPLETION_NONE = 0,
    SCREEN_RECORDING_COMPLETION_SAVED,
    SCREEN_RECORDING_COMPLETION_FAILED,
} screen_recording_completion_t;

typedef struct {
    screen_recording_completion_t result;
    char path[256];
    uint32_t frames;
} screen_recording_completion_info_t;

screen_recording_start_result_t screen_recording_service_start(uint32_t fps);
void screen_recording_service_request_stop(void);
screen_recording_state_t screen_recording_service_state(void);
bool screen_recording_service_poll_completion(
    screen_recording_completion_info_t *completion);

#endif
