#include "multi_pass_verification.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "un260/counting/counting_data_store.h"

typedef struct {
    multi_pass_verify_state_t state;
    uint8_t target_passes;
    uint8_t captured_passes;
    bool count_armed;
    bool awaiting_bundle_confirmation;
    multi_pass_snapshot_t passes[MULTI_PASS_VERIFY_MAX_PASSES];
    multi_pass_comparison_t comparisons[MULTI_PASS_VERIFY_MAX_PASSES];
    multi_pass_comparison_t latest_comparison;
} multi_pass_verify_context_t;

static multi_pass_verify_context_t g_verify;
static uint32_t g_verify_revision = 1;

static void multi_pass_revision_bump(void)
{
    g_verify_revision++;
    if (g_verify_revision == 0) g_verify_revision = 1;
}

static int multi_pass_abs_int(int value)
{
    if (value == INT_MIN) return INT_MAX;
    return value < 0 ? -value : value;
}

static int64_t multi_pass_abs_i64(int64_t value)
{
    if (value == INT64_MIN) return INT64_MAX;
    return value < 0 ? -value : value;
}

static int64_t multi_pass_amount_to_integer(float amount)
{
    return amount >= 0.0f ? (int64_t)(amount + 0.5f)
                          : (int64_t)(amount - 0.5f);
}

static void multi_pass_snapshot_release(multi_pass_snapshot_t *snapshot)
{
    if (snapshot == NULL) return;
    free(snapshot->serials);
    memset(snapshot, 0, sizeof(*snapshot));
}

static void multi_pass_release_all(void)
{
    int i;

    for (i = 0; i < MULTI_PASS_VERIFY_MAX_PASSES; i++) {
        multi_pass_snapshot_release(&g_verify.passes[i]);
    }
}

static int multi_pass_serial_compare(const void *left, const void *right)
{
    const multi_pass_serial_t *a = left;
    const multi_pass_serial_t *b = right;

    if (a->denomination != b->denomination) {
        return a->denomination < b->denomination ? -1 : 1;
    }
    return strcmp(a->text, b->text);
}

static bool multi_pass_copy_serials(multi_pass_snapshot_t *dest,
                                    const counting_sim_t *source)
{
    int serial_count;
    int copied = 0;
    int nth;

    serial_count = counting_data_serial_valid_count(source);
    if (serial_count <= 0) return true;

    dest->serials = calloc((size_t)serial_count, sizeof(*dest->serials));
    if (dest->serials == NULL) return false;

    for (nth = 0; nth < serial_count; nth++) {
        int index = counting_data_serial_nth_valid_index(source, nth);

        if (index < 0 || source->sn_str[index] == NULL) continue;
        dest->serials[copied].denomination = source->denom_mix[index];
        strncpy(dest->serials[copied].text, source->sn_str[index],
                MULTI_PASS_VERIFY_SERIAL_TEXT_MAX);
        dest->serials[copied].text[MULTI_PASS_VERIFY_SERIAL_TEXT_MAX] = '\0';
        copied++;
    }

    dest->serial_count = copied;
    if (copied > 1) {
        qsort(dest->serials, (size_t)copied, sizeof(*dest->serials),
              multi_pass_serial_compare);
    }
    return true;
}

static void multi_pass_copy_reject_reasons(multi_pass_snapshot_t *dest,
                                           const counting_sim_t *source)
{
    int detail_count = counting_data_error_detail_count(source);
    int i;

    for (i = 0; i < detail_count; i++) {
        uint8_t code = source->err_code[i];
        uint32_t sum = (uint32_t)dest->reject_reason_pcs[code] +
                       (uint32_t)source->err_pcs[i];

        dest->reject_reason_pcs[code] = sum > UINT16_MAX
            ? UINT16_MAX : (uint16_t)sum;
    }
}

static bool multi_pass_snapshot_capture(multi_pass_snapshot_t *dest,
                                        const counting_session_state_t *session,
                                        const counting_sim_t *source)
{
    int denom_count;

    memset(dest, 0, sizeof(*dest));
    dest->accepted_pcs = session->last_result.pcs;
    dest->reject_pcs = session->last_result.expected_issue;
    dest->amount = multi_pass_amount_to_integer(session->last_result.amount);

    denom_count = (int)source->denom_number;
    if (denom_count > COUNTING_DENOM_MAX_ITEMS) {
        denom_count = COUNTING_DENOM_MAX_ITEMS;
    }
    if (denom_count > 0) {
        memcpy(dest->denom, source->denom,
               sizeof(dest->denom[0]) * (size_t)denom_count);
    }
    dest->denom_count = (uint8_t)denom_count;
    multi_pass_copy_reject_reasons(dest, source);
    if (!multi_pass_copy_serials(dest, source)) {
        multi_pass_snapshot_release(dest);
        return false;
    }
    dest->valid = true;
    return true;
}

static int multi_pass_snapshot_denom_pcs(const multi_pass_snapshot_t *snapshot,
                                         int denomination)
{
    int total = 0;
    int i;

    for (i = 0; i < snapshot->denom_count; i++) {
        if (snapshot->denom[i].value == denomination) {
            total += snapshot->denom[i].pcs;
        }
    }
    return total;
}

static bool multi_pass_denom_seen_before(const multi_pass_snapshot_t *snapshot,
                                         int index)
{
    int i;

    for (i = 0; i < index; i++) {
        if (snapshot->denom[i].value == snapshot->denom[index].value) {
            return true;
        }
    }
    return false;
}

static void multi_pass_compare_denominations(
    const multi_pass_snapshot_t *baseline,
    const multi_pass_snapshot_t *current,
    multi_pass_comparison_t *comparison)
{
    int i;

    comparison->denomination_match = true;
    for (i = 0; i < baseline->denom_count; i++) {
        int denomination;
        int delta;

        if (multi_pass_denom_seen_before(baseline, i)) continue;
        denomination = baseline->denom[i].value;
        delta = multi_pass_snapshot_denom_pcs(current, denomination) -
                multi_pass_snapshot_denom_pcs(baseline, denomination);
        if (delta == 0) continue;
        comparison->denomination_match = false;
        comparison->denomination_diff_count++;
        if (comparison->first_denomination == 0) {
            comparison->first_denomination = denomination;
            comparison->first_denomination_delta = delta;
        }
    }
    for (i = 0; i < current->denom_count; i++) {
        int denomination;

        if (multi_pass_denom_seen_before(current, i)) continue;
        denomination = current->denom[i].value;
        if (multi_pass_snapshot_denom_pcs(baseline, denomination) != 0) continue;
        if (multi_pass_snapshot_denom_pcs(current, denomination) == 0) continue;
        comparison->denomination_match = false;
        comparison->denomination_diff_count++;
        if (comparison->first_denomination == 0) {
            comparison->first_denomination = denomination;
            comparison->first_denomination_delta =
                multi_pass_snapshot_denom_pcs(current, denomination);
        }
    }
}

static void multi_pass_compare_reject_reasons(
    const multi_pass_snapshot_t *baseline,
    const multi_pass_snapshot_t *current,
    multi_pass_comparison_t *comparison)
{
    int i;

    comparison->reject_match = baseline->reject_pcs == current->reject_pcs;
    for (i = 0; i < 256; i++) {
        if (baseline->reject_reason_pcs[i] != current->reject_reason_pcs[i]) {
            comparison->reject_match = false;
            break;
        }
    }
}

static void multi_pass_compare_serials(const multi_pass_snapshot_t *baseline,
                                       const multi_pass_snapshot_t *current,
                                       multi_pass_comparison_t *comparison)
{
    int baseline_index = 0;
    int current_index = 0;
    int matched = 0;

    comparison->serial_comparable = baseline->serial_count > 0 &&
                                    current->serial_count > 0;
    comparison->serial_match = !comparison->serial_comparable;
    if (!comparison->serial_comparable) return;

    comparison->serial_match = true;
    while (baseline_index < baseline->serial_count &&
           current_index < current->serial_count) {
        int order = multi_pass_serial_compare(&baseline->serials[baseline_index],
                                              &current->serials[current_index]);
        if (order == 0) {
            baseline_index++;
            current_index++;
            matched++;
        } else if (order < 0) {
            comparison->serial_match = false;
            comparison->serial_missing_count++;
            if (comparison->first_missing_serial[0] == '\0') {
                strncpy(comparison->first_missing_serial,
                        baseline->serials[baseline_index].text,
                        MULTI_PASS_VERIFY_SERIAL_TEXT_MAX);
            }
            baseline_index++;
        } else {
            comparison->serial_match = false;
            comparison->serial_extra_count++;
            if (comparison->first_extra_serial[0] == '\0') {
                strncpy(comparison->first_extra_serial,
                        current->serials[current_index].text,
                        MULTI_PASS_VERIFY_SERIAL_TEXT_MAX);
            }
            current_index++;
        }
    }
    while (baseline_index < baseline->serial_count) {
        comparison->serial_match = false;
        comparison->serial_missing_count++;
        if (comparison->first_missing_serial[0] == '\0') {
            strncpy(comparison->first_missing_serial,
                    baseline->serials[baseline_index].text,
                    MULTI_PASS_VERIFY_SERIAL_TEXT_MAX);
        }
        baseline_index++;
    }
    while (current_index < current->serial_count) {
        comparison->serial_match = false;
        comparison->serial_extra_count++;
        if (comparison->first_extra_serial[0] == '\0') {
            strncpy(comparison->first_extra_serial,
                    current->serials[current_index].text,
                    MULTI_PASS_VERIFY_SERIAL_TEXT_MAX);
        }
        current_index++;
    }

    if (matched * 2 < baseline->serial_count &&
        matched * 2 < current->serial_count) {
        comparison->significant_mismatch = true;
    }
}

static void multi_pass_compare(const multi_pass_snapshot_t *baseline,
                               const multi_pass_snapshot_t *current,
                               multi_pass_comparison_t *comparison)
{
    int baseline_input;
    int current_input;
    int input_abs;
    int amount_reference;

    memset(comparison, 0, sizeof(*comparison));
    comparison->available = baseline != NULL && current != NULL &&
                            baseline->valid && current->valid;
    if (!comparison->available) return;

    comparison->accepted_delta = current->accepted_pcs - baseline->accepted_pcs;
    comparison->reject_delta = current->reject_pcs - baseline->reject_pcs;
    comparison->amount_delta = current->amount - baseline->amount;
    baseline_input = baseline->accepted_pcs + baseline->reject_pcs;
    current_input = current->accepted_pcs + current->reject_pcs;
    comparison->input_delta = current_input - baseline_input;
    comparison->totals_match = comparison->accepted_delta == 0 &&
                               comparison->amount_delta == 0;

    multi_pass_compare_denominations(baseline, current, comparison);
    multi_pass_compare_reject_reasons(baseline, current, comparison);
    multi_pass_compare_serials(baseline, current, comparison);

    comparison->exact_match = comparison->totals_match &&
                              comparison->denomination_match &&
                              comparison->reject_match &&
                              comparison->serial_match;

    input_abs = multi_pass_abs_int(comparison->input_delta);
    amount_reference = baseline->amount > INT_MAX
        ? INT_MAX : (int)multi_pass_abs_i64(baseline->amount);
    if (input_abs >= 3 ||
        (baseline_input > 0 && input_abs * 10 >= baseline_input) ||
        multi_pass_abs_i64(comparison->amount_delta) >= 100 ||
        (amount_reference > 0 &&
         multi_pass_abs_i64(comparison->amount_delta) * 10 >= amount_reference)) {
        comparison->significant_mismatch = true;
    }
}

static bool multi_pass_all_captured_passes_match(void)
{
    int i;

    if (g_verify.captured_passes == 0) return false;
    for (i = 1; i < g_verify.captured_passes; i++) {
        if (!g_verify.comparisons[i].exact_match) return false;
    }
    return true;
}

bool multi_pass_verification_start(uint8_t target_passes, bool add_enabled)
{
    if (add_enabled || target_passes < MULTI_PASS_VERIFY_MIN_PASSES ||
        target_passes > MULTI_PASS_VERIFY_MAX_PASSES) {
        return false;
    }

    multi_pass_release_all();
    memset(&g_verify, 0, sizeof(g_verify));
    g_verify.state = MULTI_PASS_VERIFY_RUNNING;
    g_verify.target_passes = target_passes;
    multi_pass_revision_bump();
    return true;
}

void multi_pass_verification_cancel(void)
{
    multi_pass_release_all();
    memset(&g_verify, 0, sizeof(g_verify));
    multi_pass_revision_bump();
}

bool multi_pass_verification_on_count_start(bool add_enabled)
{
    if (g_verify.state != MULTI_PASS_VERIFY_RUNNING ||
        g_verify.awaiting_bundle_confirmation ||
        g_verify.captured_passes >= g_verify.target_passes) {
        return false;
    }
    g_verify.count_armed = !add_enabled;
    multi_pass_revision_bump();
    return g_verify.count_armed;
}

multi_pass_capture_kind_t multi_pass_verification_capture(
    const counting_session_state_t *session,
    const counting_sim_t *counting_data,
    multi_pass_capture_event_t *event)
{
    multi_pass_snapshot_t *snapshot;
    multi_pass_capture_kind_t kind;

    if (event != NULL) memset(event, 0, sizeof(*event));
    if (g_verify.state != MULTI_PASS_VERIFY_RUNNING ||
        !g_verify.count_armed || g_verify.awaiting_bundle_confirmation ||
        session == NULL || counting_data == NULL ||
        !session->last_result.valid ||
        (session->last_result.pcs <= 0 &&
         session->last_result.expected_issue <= 0) ||
        g_verify.captured_passes >= g_verify.target_passes) {
        return MULTI_PASS_CAPTURE_IGNORED;
    }

    snapshot = &g_verify.passes[g_verify.captured_passes];
    multi_pass_snapshot_release(snapshot);
    if (!multi_pass_snapshot_capture(snapshot, session, counting_data)) {
        g_verify.count_armed = false;
        kind = MULTI_PASS_CAPTURE_MEMORY_ERROR;
    } else {
        g_verify.captured_passes++;
        g_verify.count_armed = false;
        if (g_verify.captured_passes > 1) {
            multi_pass_compare(&g_verify.passes[0], snapshot,
                               &g_verify.latest_comparison);
            g_verify.comparisons[g_verify.captured_passes - 1] =
                g_verify.latest_comparison;
        } else {
            memset(&g_verify.latest_comparison, 0,
                   sizeof(g_verify.latest_comparison));
        }

        if (g_verify.captured_passes > 1 &&
            !g_verify.latest_comparison.exact_match &&
            g_verify.latest_comparison.significant_mismatch) {
            g_verify.awaiting_bundle_confirmation = true;
            kind = MULTI_PASS_CAPTURE_REVIEW_BUNDLE;
        } else if (g_verify.captured_passes >= g_verify.target_passes) {
            g_verify.state = MULTI_PASS_VERIFY_COMPLETE;
            kind = MULTI_PASS_CAPTURE_COMPLETE;
        } else {
            kind = MULTI_PASS_CAPTURE_NEXT;
        }
    }

    if (event != NULL) {
        event->kind = kind;
        event->captured_passes = g_verify.captured_passes;
        event->target_passes = g_verify.target_passes;
        if (g_verify.captured_passes > 0) {
            event->latest = g_verify.passes[g_verify.captured_passes - 1];
        }
        event->comparison = g_verify.latest_comparison;
        event->all_passes_match = multi_pass_all_captured_passes_match();
    }
    multi_pass_revision_bump();
    return kind;
}

multi_pass_capture_kind_t multi_pass_verification_confirm_same_bundle(void)
{
    if (!g_verify.awaiting_bundle_confirmation) {
        return MULTI_PASS_CAPTURE_IGNORED;
    }

    g_verify.awaiting_bundle_confirmation = false;
    if (g_verify.captured_passes >= g_verify.target_passes) {
        g_verify.state = MULTI_PASS_VERIFY_COMPLETE;
        multi_pass_revision_bump();
        return MULTI_PASS_CAPTURE_COMPLETE;
    }
    multi_pass_revision_bump();
    return MULTI_PASS_CAPTURE_NEXT;
}

bool multi_pass_verification_restart_from_latest(void)
{
    multi_pass_snapshot_t latest;
    int latest_index;
    int i;

    if (!g_verify.awaiting_bundle_confirmation ||
        g_verify.captured_passes == 0) {
        return false;
    }

    latest_index = g_verify.captured_passes - 1;
    latest = g_verify.passes[latest_index];
    memset(&g_verify.passes[latest_index], 0,
           sizeof(g_verify.passes[latest_index]));
    for (i = 0; i < MULTI_PASS_VERIFY_MAX_PASSES; i++) {
        multi_pass_snapshot_release(&g_verify.passes[i]);
    }
    g_verify.passes[0] = latest;
    g_verify.captured_passes = 1;
    g_verify.count_armed = false;
    g_verify.awaiting_bundle_confirmation = false;
    g_verify.state = MULTI_PASS_VERIFY_RUNNING;
    memset(&g_verify.latest_comparison, 0,
           sizeof(g_verify.latest_comparison));
    memset(g_verify.comparisons, 0, sizeof(g_verify.comparisons));
    multi_pass_revision_bump();
    return true;
}

void multi_pass_verification_get_view(multi_pass_verify_view_t *view)
{
    int i;

    if (view == NULL) return;
    memset(view, 0, sizeof(*view));
    view->state = g_verify.state;
    view->target_passes = g_verify.target_passes;
    view->captured_passes = g_verify.captured_passes;
    view->count_armed = g_verify.count_armed;
    view->awaiting_bundle_confirmation =
        g_verify.awaiting_bundle_confirmation;
    view->latest_comparison = g_verify.latest_comparison;
    view->all_passes_match = multi_pass_all_captured_passes_match();
    for (i = 0; i < MULTI_PASS_VERIFY_MAX_PASSES; i++) {
        view->passes[i] = &g_verify.passes[i];
        view->comparisons[i] = &g_verify.comparisons[i];
    }
}

bool multi_pass_verification_is_active(void)
{
    return g_verify.state == MULTI_PASS_VERIFY_RUNNING;
}

uint32_t multi_pass_verification_revision(void)
{
    return g_verify_revision;
}
