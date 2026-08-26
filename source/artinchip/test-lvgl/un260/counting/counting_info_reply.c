#include "counting_info_reply.h"

#include <stddef.h>


static counting_info_reply_result_t counting_info_reply_result(counting_info_reply_kind_t kind)
{
    counting_info_reply_result_t result = {
        .kind = kind,
        .final_pcs = 0,
        .final_amount = 0.0f,
        .final_issue = 0,
    };
    return result;
}

static uint32_t counting_info_saturating_add_u32(uint32_t value,
                                                 uint32_t increment)
{
    if (increment > UINT32_MAX - value) {
        return UINT32_MAX;
    }
    return value + increment;
}

static void counting_info_commit_previous_result(counting_session_state_t *session,
                                                 counting_sim_t *sim_data)
{
    if (!session->last_result.valid) {
        return;
    }

    sim_data->last_total_pcs = session->last_result.pcs;
    sim_data->last_total_amount = session->last_result.amount;
    sim_data->last_valid_pcs = session->last_result.valid_pcs;
    sim_data->last_issue_pcs = session->last_result.issue_pcs;
    sim_data->last_suspect_pcs = session->last_result.suspect_pcs;
    sim_data->last_damaged_pcs = session->last_result.damaged_pcs;
    session->last_result.valid = false;
}

static counting_info_reply_result_t counting_info_handle_live(counting_session_state_t *session,
                                                              counting_sim_t *sim_data,
                                                              uint32_t amount,
                                                              uint16_t qty,
                                                              uint8_t issue)
{
    if (session->phase == COUNTING_SESSION_FINISHED_WAIT_START) {
        return counting_info_reply_result(COUNTING_INFO_REPLY_IGNORED);
    }

    if (session->phase != COUNTING_SESSION_ACTIVE) {
        counting_info_commit_previous_result(session, sim_data);
        session->auto_wave_pending = false;
        session->phase = COUNTING_SESSION_ACTIVE;
        session->expected_issue = 0;
    }

    sim_data->total_amount = amount;
    sim_data->total_pcs = qty;
    if ((int)issue > session->expected_issue) {
        session->expected_issue = (int)issue;
    }
    sim_data->err_expected = session->expected_issue;

    return counting_info_reply_result(COUNTING_INFO_REPLY_LIVE);
}

static counting_info_reply_result_t counting_info_handle_finished(counting_session_state_t *session,
                                                                  counting_sim_t *sim_data,
                                                                  uint32_t amount,
                                                                  uint16_t qty,
                                                                  uint8_t issue,
                                                                  uint32_t history_total_notes_counted)
{
    counting_info_reply_result_t result = counting_info_reply_result(COUNTING_INFO_REPLY_FINISHED);

    if (session->phase == COUNTING_SESSION_FINISHED_WAIT_START) {
        return counting_info_reply_result(COUNTING_INFO_REPLY_IGNORED);
    }

    session->phase = COUNTING_SESSION_FINISHED_WAIT_START;
    session->end_anim_wait_detail = true;
    session->last_result.valid = true;

    if (sim_data->total_pcs > 0 || sim_data->total_amount > 0.0f) {
        result.final_pcs = sim_data->total_pcs;
        result.final_amount = sim_data->total_amount;
    } else {
        result.final_pcs = (int)qty;
        result.final_amount = (float)amount;
    }
    result.final_issue = (int)issue > session->expected_issue
        ? (int)issue : session->expected_issue;
    sim_data->err_expected = result.final_issue;

    session->last_result.pcs = result.final_pcs;
    session->last_result.amount = result.final_amount;
    session->last_result.issue_pcs = result.final_issue;
    session->last_result.suspect_pcs = result.final_issue;
    session->last_result.damaged_pcs = 0;
    /* 0x0E qty already represents accepted notes, not accepted + rejected. */
    session->last_result.valid_pcs = result.final_pcs;
    session->last_result.expected_issue = result.final_issue;
    session->analysis_valid_pcs = (int)qty;

    session->history_record.valid = (result.final_pcs > 0);
    session->history_record.end_seen = false;
    session->history_record.save_attempts = 0;
    session->history_record.retry_tick = 0;
    session->history_record.pcs = (uint32_t)result.final_pcs;
    session->history_record.total_after = counting_info_saturating_add_u32(
        history_total_notes_counted, (uint32_t)result.final_pcs);
    session->history_record.amount = result.final_amount;

    return result;
}

counting_info_reply_result_t counting_info_reply_handle(counting_session_state_t *session,
                                                        counting_sim_t *sim_data,
                                                        const uint8_t *buf,
                                                        uint8_t len,
                                                        uint32_t history_total_notes_counted)
{
    const uint8_t *payload;
    uint32_t amount;
    uint16_t qty;
    uint8_t issue;
    uint8_t status;

    if (session == NULL || sim_data == NULL || buf == NULL || len < 12) {
        return counting_info_reply_result(COUNTING_INFO_REPLY_INVALID);
    }

    payload = &buf[4];
    amount = ((uint32_t)payload[0] << 24) |
             ((uint32_t)payload[1] << 16) |
             ((uint32_t)payload[2] << 8) |
             (uint32_t)payload[3];
    qty = (uint16_t)(((uint16_t)payload[4] << 8) | payload[5]);
    issue = payload[6];
    status = payload[7];

    if (status <= 0x01) {
        return counting_info_handle_live(session, sim_data, amount, qty, issue);
    }
    if (status == 0x02) {
        return counting_info_handle_finished(session, sim_data, amount, qty, issue,
                                             history_total_notes_counted);
    }
    return counting_info_reply_result(COUNTING_INFO_REPLY_IGNORED);
}
