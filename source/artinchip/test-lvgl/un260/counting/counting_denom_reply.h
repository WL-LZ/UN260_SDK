#ifndef COUNTING_DENOM_REPLY_H
#define COUNTING_DENOM_REPLY_H

#include <stdint.h>

#include "counting_data_types.h"
#include "counting_detail_state.h"
#include "counting_session_state.h"

typedef enum {
    COUNTING_DENOM_REPLY_INVALID = 0,
    COUNTING_DENOM_REPLY_IGNORED,
    COUNTING_DENOM_REPLY_START,
    COUNTING_DENOM_REPLY_DATA,
    COUNTING_DENOM_REPLY_QUERY_END,
    COUNTING_DENOM_REPLY_SESSION_END,
} counting_denom_reply_result_t;

typedef struct {
    void (*on_history_frame)(const char *tag, const uint8_t *buf, uint8_t len);
    void (*on_main_data_changed)(void);
} counting_denom_reply_hooks_t;

counting_denom_reply_result_t counting_denom_reply_handle(
    counting_detail_state_t *detail,
    const counting_session_state_t *session,
    counting_sim_t *sim_data,
    const uint8_t *buf,
    uint8_t len,
    const counting_denom_reply_hooks_t *hooks);

#endif
