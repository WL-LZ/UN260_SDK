#ifndef COUNTING_HISTORY_SERVICE_H
#define COUNTING_HISTORY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "counting_data_types.h"
#include "counting_session_state.h"

typedef enum {
    COUNTING_HISTORY_COMMIT_NOT_READY = 0,
    COUNTING_HISTORY_COMMIT_SAVED,
    COUNTING_HISTORY_COMMIT_RETRY_PENDING,
    COUNTING_HISTORY_COMMIT_FAILED,
} counting_history_commit_result_t;

void counting_history_session_start(const uint8_t *buf, uint8_t len);
void counting_history_capture_error(const char *tag,
                                    const uint8_t *buf,
                                    uint8_t len);
void counting_history_append_frame(const char *tag,
                                   const uint8_t *buf,
                                   uint8_t len);
void counting_history_capture_end(const uint8_t *buf, uint8_t len);
counting_history_commit_result_t counting_history_try_commit(
    counting_session_state_t *session,
    const counting_sim_t *sim_data,
    uint32_t now_ms);
counting_history_commit_result_t counting_history_poll_commit(
    counting_session_state_t *session,
    const counting_sim_t *sim_data,
    uint32_t now_ms);
bool counting_history_discard_pending(counting_session_state_t *session);

#endif
