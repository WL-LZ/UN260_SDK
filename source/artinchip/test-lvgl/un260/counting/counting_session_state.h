#ifndef COUNTING_SESSION_STATE_H
#define COUNTING_SESSION_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool valid;
    int pcs;
    float amount;
    int valid_pcs;
    int issue_pcs;
    int suspect_pcs;
    int damaged_pcs;
    int expected_issue;
} counting_pending_result_t;

typedef struct {
    bool valid;
    bool end_seen;
    uint8_t save_attempts;
    uint32_t retry_tick;
    uint32_t pcs;
    uint32_t total_after;
    float amount;
} counting_pending_history_record_t;

typedef enum {
    COUNTING_SESSION_IDLE = 0,
    COUNTING_SESSION_ACTIVE,
    COUNTING_SESSION_FINISHED_WAIT_START,
} counting_session_phase_t;

typedef struct {
    counting_session_phase_t phase;
    bool end_anim_wait_detail;
    bool auto_wave_pending;
    counting_pending_result_t last_result;
    int analysis_valid_pcs;
    int expected_issue;
    counting_pending_history_record_t history_record;
} counting_session_state_t;

#endif
