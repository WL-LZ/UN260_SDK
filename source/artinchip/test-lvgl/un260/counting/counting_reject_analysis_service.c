#include "counting_reject_analysis_service.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "un260/counting/counting_data_store.h"

#define COUNTING_REJECT_CODE_COUNT 0x100U

static uint32_t g_reject_pocket_snapshot[COUNTING_REJECT_CODE_COUNT];

static bool counting_reject_code_is_damaged(uint8_t code)
{
    switch (code) {
    case 0x18: /* Long */
    case 0x19: /* Short */
    case 0x1A: /* GAP */
    case 0x24: /* Hole */
    case 0x25: /* DogEar */
    case 0x26: /* DIRT */
    case 0x27: /* Tape */
    case 0x28: /* Tears */
    case 0x29: /* Crumples */
    case 0x2A: /* De_ink */
    case 0x2B: /* Soiling */
    case 0x2C: /* Limpness */
        return true;
    default:
        return false;
    }
}

static int counting_reject_expected_issue(counting_session_state_t *session,
                                          uint32_t delta_total)
{
    int expected = session->last_result.expected_issue;

    if (expected <= 0 && delta_total > 0U) {
        expected = delta_total > (uint32_t)INT_MAX ?
                   INT_MAX : (int)delta_total;
    }
    if (expected < 0) {
        expected = 0;
    }
    session->last_result.expected_issue = expected;
    return expected;
}

bool counting_reject_analysis_update(counting_session_state_t *session,
                                     const counting_sim_t *sim_data,
                                     counting_reject_analysis_result_t *result)
{
    uint32_t current_by_code[COUNTING_REJECT_CODE_COUNT] = {0};
    uint32_t delta_by_code[COUNTING_REJECT_CODE_COUNT] = {0};
    const uint32_t *selected_by_code;
    uint32_t current_total = 0;
    uint32_t delta_total = 0;
    uint32_t suspect = 0;
    uint32_t damaged = 0;
    uint32_t issue;
    int expected_issue;
    int detail_count;

    if (session == NULL || sim_data == NULL || result == NULL ||
        !session->last_result.valid) {
        return false;
    }

    detail_count = counting_data_error_detail_count(sim_data);
    if (detail_count == 0) {
        return false;
    }

    for (int i = 0; i < detail_count; i++) {
        current_by_code[sim_data->err_code[i]] += sim_data->err_pcs[i];
    }

    for (uint16_t code = 0; code < COUNTING_REJECT_CODE_COUNT; code++) {
        current_total += current_by_code[code];
        if (current_by_code[code] > g_reject_pocket_snapshot[code]) {
            delta_by_code[code] =
                current_by_code[code] - g_reject_pocket_snapshot[code];
            delta_total += delta_by_code[code];
        }
    }

    expected_issue = counting_reject_expected_issue(session, delta_total);
    result->source = COUNTING_REJECT_ANALYSIS_SOURCE_CURRENT;
    if (current_total == (uint32_t)expected_issue) {
        selected_by_code = current_by_code;
    } else if (delta_total == (uint32_t)expected_issue ||
               (delta_total > 0U && delta_total <= (uint32_t)expected_issue)) {
        selected_by_code = delta_by_code;
        result->source = COUNTING_REJECT_ANALYSIS_SOURCE_DELTA;
    } else {
        selected_by_code = current_by_code;
    }

    for (uint16_t code = 0; code < COUNTING_REJECT_CODE_COUNT; code++) {
        if (counting_reject_code_is_damaged((uint8_t)code)) {
            damaged += selected_by_code[code];
        } else {
            suspect += selected_by_code[code];
        }
    }

    issue = suspect + damaged;
    if (issue < (uint32_t)expected_issue) {
        suspect += (uint32_t)expected_issue - issue;
    } else if (issue > (uint32_t)expected_issue) {
        uint32_t overflow = issue - (uint32_t)expected_issue;
        uint32_t reduce = suspect < overflow ? suspect : overflow;

        suspect -= reduce;
        overflow -= reduce;
        if (overflow > 0U) {
            damaged = damaged > overflow ? damaged - overflow : 0U;
        }
    }

    session->last_result.issue_pcs = expected_issue;
    session->last_result.suspect_pcs = (int)suspect;
    session->last_result.damaged_pcs = (int)damaged;
    /* Accepted and rejected counts are independent fields in 0x0E. */
    session->last_result.valid_pcs = session->last_result.pcs;

    memcpy(g_reject_pocket_snapshot, current_by_code,
           sizeof(g_reject_pocket_snapshot));
    result->current_total = current_total;
    result->delta_total = delta_total;
    result->expected_issue = expected_issue;
    result->suspect_pcs = (int)suspect;
    result->damaged_pcs = (int)damaged;
    return true;
}
