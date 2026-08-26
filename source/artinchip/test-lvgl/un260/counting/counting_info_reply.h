#ifndef COUNTING_INFO_REPLY_H
#define COUNTING_INFO_REPLY_H

#include <stdint.h>

#include "counting_data_types.h"
#include "counting_session_state.h"

typedef enum {
    COUNTING_INFO_REPLY_INVALID = 0,
    COUNTING_INFO_REPLY_IGNORED,
    COUNTING_INFO_REPLY_LIVE,
    COUNTING_INFO_REPLY_FINISHED,
} counting_info_reply_kind_t;

typedef struct {
    counting_info_reply_kind_t kind;
    int final_pcs;
    float final_amount;
    int final_issue;
} counting_info_reply_result_t;

counting_info_reply_result_t counting_info_reply_handle(counting_session_state_t *session,
                                                        counting_sim_t *sim_data,
                                                        const uint8_t *buf,
                                                        uint8_t len,
                                                        uint32_t history_total_notes_counted);

#endif
