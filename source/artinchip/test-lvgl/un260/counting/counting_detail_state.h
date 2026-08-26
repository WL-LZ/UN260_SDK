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
    uint32_t query_tick;
    uint8_t query_retry;
    uint32_t query_idle_retry_tick;
    denom_t query_denom[COUNTING_DENOM_MAX_ITEMS];
    uint8_t query_denom_number;
} counting_detail_state_t;

#endif
