#ifndef UN260_MULTI_PASS_VERIFICATION_H
#define UN260_MULTI_PASS_VERIFICATION_H

#include <stdbool.h>
#include <stdint.h>

#include "un260/counting/counting_data_types.h"
#include "un260/counting/counting_session_state.h"

#define MULTI_PASS_VERIFY_MIN_PASSES 2
#define MULTI_PASS_VERIFY_MAX_PASSES 5
#define MULTI_PASS_VERIFY_SERIAL_TEXT_MAX 31

typedef enum {
    MULTI_PASS_VERIFY_IDLE = 0,
    MULTI_PASS_VERIFY_RUNNING,
    MULTI_PASS_VERIFY_COMPLETE,
} multi_pass_verify_state_t;

typedef enum {
    MULTI_PASS_CAPTURE_IGNORED = 0,
    MULTI_PASS_CAPTURE_NEXT,
    MULTI_PASS_CAPTURE_REVIEW_BUNDLE,
    MULTI_PASS_CAPTURE_COMPLETE,
    MULTI_PASS_CAPTURE_MEMORY_ERROR,
    MULTI_PASS_CAPTURE_ADD_REQUIRED,
} multi_pass_capture_kind_t;

typedef struct {
    int denomination;
    char text[MULTI_PASS_VERIFY_SERIAL_TEXT_MAX + 1];
} multi_pass_serial_t;

typedef struct {
    bool valid;
    int accepted_pcs;
    int reject_pcs;
    int64_t amount;
    denom_t denom[COUNTING_DENOM_MAX_ITEMS];
    uint8_t denom_count;
    uint16_t reject_reason_pcs[256];
    multi_pass_serial_t *serials;
    int serial_count;
} multi_pass_snapshot_t;

typedef struct {
    bool available;
    bool exact_match;
    bool totals_match;
    bool denomination_match;
    bool reject_match;
    bool serial_comparable;
    bool serial_match;
    bool significant_mismatch;
    int accepted_delta;
    int reject_delta;
    int input_delta;
    int64_t amount_delta;
    int denomination_diff_count;
    int first_denomination;
    int first_denomination_delta;
    int serial_missing_count;
    int serial_extra_count;
    char first_missing_serial[MULTI_PASS_VERIFY_SERIAL_TEXT_MAX + 1];
    char first_extra_serial[MULTI_PASS_VERIFY_SERIAL_TEXT_MAX + 1];
} multi_pass_comparison_t;

typedef struct {
    multi_pass_capture_kind_t kind;
    uint8_t captured_passes;
    uint8_t target_passes;
    multi_pass_snapshot_t latest;
    multi_pass_comparison_t comparison;
    bool all_passes_match;
} multi_pass_capture_event_t;

typedef struct {
    multi_pass_verify_state_t state;
    uint8_t target_passes;
    uint8_t captured_passes;
    bool count_armed;
    bool awaiting_bundle_confirmation;
    const multi_pass_snapshot_t *passes[MULTI_PASS_VERIFY_MAX_PASSES];
    const multi_pass_comparison_t *comparisons[MULTI_PASS_VERIFY_MAX_PASSES];
    multi_pass_comparison_t latest_comparison;
    bool all_passes_match;
} multi_pass_verify_view_t;

bool multi_pass_verification_start(uint8_t target_passes,
                                   bool add_enabled);
void multi_pass_verification_cancel(void);
bool multi_pass_verification_on_count_start(bool add_enabled);
multi_pass_capture_kind_t multi_pass_verification_capture(
    const counting_session_state_t *session,
    const counting_sim_t *counting_data,
    multi_pass_capture_event_t *event);
multi_pass_capture_kind_t multi_pass_verification_confirm_same_bundle(void);
bool multi_pass_verification_restart_from_latest(void);
void multi_pass_verification_get_view(multi_pass_verify_view_t *view);
bool multi_pass_verification_is_active(void);
uint32_t multi_pass_verification_revision(void);

#endif
