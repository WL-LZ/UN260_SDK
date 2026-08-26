#ifndef COUNTING_REJECT_ANALYSIS_SERVICE_H
#define COUNTING_REJECT_ANALYSIS_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "counting_data_types.h"
#include "counting_session_state.h"

typedef enum {
    COUNTING_REJECT_ANALYSIS_SOURCE_CURRENT = 0,
    COUNTING_REJECT_ANALYSIS_SOURCE_DELTA,
} counting_reject_analysis_source_t;

typedef struct {
    uint32_t current_total;
    uint32_t delta_total;
    int expected_issue;
    int suspect_pcs;
    int damaged_pcs;
    counting_reject_analysis_source_t source;
} counting_reject_analysis_result_t;

bool counting_reject_analysis_update(counting_session_state_t *session,
                                     const counting_sim_t *sim_data,
                                     counting_reject_analysis_result_t *result);

#endif
