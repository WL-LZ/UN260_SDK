#ifndef UN260_COUNTING_COUNTING_ACTION_SERVICE_H
#define UN260_COUNTING_COUNTING_ACTION_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    COUNTING_ACTION_TIMEOUT_NONE = 0,
    COUNTING_ACTION_TIMEOUT_START = 1U << 0,
    COUNTING_ACTION_TIMEOUT_CLEAR = 1U << 1,
} counting_action_timeout_t;

bool counting_action_request_start(void);
bool counting_action_request_clear(void);
void counting_action_handle_reply(uint8_t cmd, const uint8_t *frame,
                                  uint8_t frame_len);
uint32_t counting_action_take_timeouts(void);
bool counting_action_clear_pending(void);
void counting_action_cancel_all(void);

#endif
