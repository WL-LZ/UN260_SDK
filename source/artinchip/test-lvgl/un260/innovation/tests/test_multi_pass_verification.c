#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "un260/counting/counting_data_store.h"
#include "un260/innovation/multi_pass_verification.h"

static void fill_result(counting_session_state_t *session,
                        counting_sim_t *data,
                        int accepted,
                        int reject,
                        int denom_value,
                        int denom_pcs,
                        const char *serial_a,
                        const char *serial_b)
{
    memset(session, 0, sizeof(*session));
    counting_data_clear_errors(data);
    counting_data_clear_serials(data);
    memset(data, 0, sizeof(*data));

    session->last_result.valid = true;
    session->last_result.pcs = accepted;
    session->last_result.amount = (float)(denom_value * denom_pcs);
    session->last_result.expected_issue = reject;
    data->denom_number = 1;
    data->denom[0].value = denom_value;
    data->denom[0].pcs = (uint16_t)denom_pcs;
    data->denom[0].amount = (float)(denom_value * denom_pcs);
    data->err_expected = (uint16_t)reject;
    if (reject > 0) {
        assert(counting_data_ensure_error_capacity(data, 1));
        data->err_num = 1;
        data->err_code[0] = 0x1c;
        data->err_pcs[0] = (uint8_t)reject;
    }

    if (serial_a != NULL) {
        int count = serial_b != NULL ? 2 : 1;
        assert(counting_data_ensure_serial_capacity(data, count));
        data->sn_str[0] = strdup(serial_a);
        data->denom_mix[0] = denom_value;
        if (serial_b != NULL) {
            data->sn_str[1] = strdup(serial_b);
            data->denom_mix[1] = denom_value;
        }
    }
}

static multi_pass_capture_kind_t capture(counting_session_state_t *session,
                                         counting_sim_t *data,
                                         multi_pass_capture_event_t *event)
{
    assert(multi_pass_verification_on_count_start(false));
    return multi_pass_verification_capture(session, data, event);
}

int main(void)
{
    counting_session_state_t session;
    counting_sim_t data = { 0 };
    multi_pass_capture_event_t event;
    multi_pass_verify_view_t view;

    assert(!multi_pass_verification_start(2, true));
    assert(!multi_pass_verification_start(1, false));

    assert(multi_pass_verification_start(2, false));
    assert(!multi_pass_verification_on_count_start(true));
    fill_result(&session, &data, 2, 0, 20, 2, "SN001", "SN002");
    assert(capture(&session, &data, &event) == MULTI_PASS_CAPTURE_NEXT);
    fill_result(&session, &data, 2, 0, 20, 2, "SN002", "SN001");
    assert(capture(&session, &data, &event) == MULTI_PASS_CAPTURE_COMPLETE);
    assert(event.all_passes_match);
    assert(event.comparison.exact_match);

    assert(multi_pass_verification_start(2, false));
    fill_result(&session, &data, 2, 0, 20, 2, "SN001", "SN002");
    assert(capture(&session, &data, &event) == MULTI_PASS_CAPTURE_NEXT);
    fill_result(&session, &data, 2, 0, 20, 2, "SN001", NULL);
    assert(capture(&session, &data, &event) == MULTI_PASS_CAPTURE_COMPLETE);
    assert(!event.all_passes_match);
    assert(event.comparison.serial_missing_count == 1);

    assert(multi_pass_verification_start(3, false));
    fill_result(&session, &data, 10, 0, 20, 10, NULL, NULL);
    assert(capture(&session, &data, &event) == MULTI_PASS_CAPTURE_NEXT);
    fill_result(&session, &data, 9, 0, 20, 9, NULL, NULL);
    assert(capture(&session, &data, &event) == MULTI_PASS_CAPTURE_REVIEW_BUNDLE);
    assert(multi_pass_verification_confirm_same_bundle() ==
           MULTI_PASS_CAPTURE_NEXT);
    fill_result(&session, &data, 10, 0, 20, 10, NULL, NULL);
    assert(capture(&session, &data, &event) == MULTI_PASS_CAPTURE_COMPLETE);
    assert(!event.all_passes_match);
    multi_pass_verification_get_view(&view);
    assert(view.captured_passes == 3);
    assert(!view.comparisons[1]->exact_match);
    assert(view.comparisons[2]->exact_match);

    assert(multi_pass_verification_start(2, false));
    fill_result(&session, &data, 10, 0, 20, 10, NULL, NULL);
    assert(capture(&session, &data, &event) == MULTI_PASS_CAPTURE_NEXT);
    fill_result(&session, &data, 3, 1, 100, 3, NULL, NULL);
    assert(capture(&session, &data, &event) == MULTI_PASS_CAPTURE_REVIEW_BUNDLE);
    assert(event.comparison.reject_delta == 1);
    assert(!event.comparison.reject_match);
    assert(multi_pass_verification_restart_from_latest());
    multi_pass_verification_get_view(&view);
    assert(view.captured_passes == 1);
    assert(view.passes[0]->accepted_pcs == 3);
    assert(view.passes[0]->reject_pcs == 1);

    multi_pass_verification_cancel();
    counting_data_clear_errors(&data);
    counting_data_clear_serials(&data);
    puts("multi-pass verification tests passed");
    return 0;
}
