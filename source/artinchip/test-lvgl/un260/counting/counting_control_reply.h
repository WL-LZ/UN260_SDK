#ifndef COUNTING_CONTROL_REPLY_H
#define COUNTING_CONTROL_REPLY_H

#include <stdbool.h>
#include <stdint.h>

#include "counting_session_state.h"

typedef struct {
    void (*on_start_success)(const uint8_t *buf, uint8_t len);
    void (*on_error_frame)(const char *tag, const uint8_t *buf, uint8_t len);
    void (*on_start_failure)(uint8_t type, uint8_t code);
    void (*on_runtime_fault)(uint8_t code);
} counting_control_reply_hooks_t;

bool counting_control_reply_dispatch(uint8_t cmd,
                                     counting_session_state_t *session,
                                     const uint8_t *buf,
                                     uint8_t len,
                                     const counting_control_reply_hooks_t *hooks);

#endif
