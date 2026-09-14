#ifndef COUNTING_DETAIL_STATE_H
#define COUNTING_DETAIL_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "counting_data_types.h"

typedef struct {
    bool wait_sn_after_reject_end;
    bool query_pending;
    bool query_deferred;
    bool query_started;  /* Valid 0x0B start marker received. */
    bool query_complete; /* Valid start/end sequence completed. */
    bool query_failed;   /* Do not automatically restart an exhausted query. */
    bool query_expired;  /* Drain late query frames without treating them as a count. */
    bool query_overflow;
    bool query_wait_push; /* Wait for controller push before issuing a fallback query. */
    uint32_t query_activity_tick;
    uint32_t query_tick;
    uint8_t query_retry;
    uint32_t query_idle_retry_tick;
    denom_t query_denom[COUNTING_DENOM_MAX_ITEMS];
    uint8_t query_denom_number;
} counting_detail_state_t;

#endif
