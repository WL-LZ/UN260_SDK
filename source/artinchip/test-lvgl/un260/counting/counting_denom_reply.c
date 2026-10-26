#include "counting_denom_reply.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "counting_denom_query_service.h"
#include "counting_report_sync.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/lv_system/app_clock.h"

static void counting_denom_record_history(const counting_denom_reply_hooks_t *hooks,
                                          const uint8_t *buf, uint8_t len)
{
    if (hooks && hooks->on_history_frame) hooks->on_history_frame("0x0B", buf, len);
}

static void counting_denom_notify_changed(const counting_denom_reply_hooks_t *hooks)
{
    if (hooks && hooks->on_main_data_changed) hooks->on_main_data_changed();
}

static bool counting_denom_payload_is(const uint8_t *buf, uint8_t value)
{
    for (unsigned i = 4; i < 15; ++i)
        if (buf[i] != value) return false;
    return true;
}

static bool counting_denom_parse_decimal(const uint8_t *data, size_t length, int *out)
{
    unsigned value = 0;
    bool digit_seen = false, trailing_padding = false;
    for (size_t i = 0; i < length; ++i) {
        uint8_t ch = data[i];
        if (ch >= '0' && ch <= '9') {
            unsigned digit = (unsigned)(ch - '0');
            if (trailing_padding || value > ((unsigned)INT_MAX - digit) / 10U)
                return false;
            value = value * 10U + digit;
            digit_seen = true;
        } else if (ch == ' ' || ch == '\0') {
            if (digit_seen) trailing_padding = true;
        } else return false;
    }
    if (!digit_seen) return false;
    *out = (int)value;
    return true;
}

void counting_denom_reply_begin_count(counting_detail_state_t *detail)
{
    if (!detail) return;
    counting_denom_query_cancel(detail);
    detail->live_denom_started = false;
    detail->live_denom_count_owned = true;
    detail->live_denom_seen = 0;
}

bool counting_denom_reply_currency_changed(counting_detail_state_t *detail,
                                            counting_sim_t *data)
{
    if (!detail || !data || data->denom_number > COUNTING_DENOM_MAX_ITEMS)
        return false;
    denom_t rows[COUNTING_DENOM_MAX_ITEMS] = {0};
    uint8_t count = 0;
    if (detail->live_denom_count_owned) {
        for (unsigned i = 0; i < data->denom_number; ++i)
            if (detail->live_denom_seen & (1U << i)) rows[count++] = data->denom[i];
    }
    bool changed = count != data->denom_number || memcmp(rows, data->denom, sizeof(rows));
    memcpy(data->denom, rows, sizeof(rows));
    data->denom_number = count;
    detail->live_denom_seen = (uint16_t)((1U << count) - 1U);
    return changed;
}
static counting_denom_reply_result_t counting_denom_start(
    counting_detail_state_t *detail, counting_sim_t *data,
    const uint8_t *buf, uint8_t len, const counting_denom_reply_hooks_t *hooks)
{
    if (detail->query_pending) {
        detail->live_denom_started = false;
        counting_denom_query_mark_start(detail);
    } else {
        detail->live_denom_started = true;
        detail->live_denom_seen = 0;
        bool changed = false;
        if (data->denom_number <= COUNTING_DENOM_MAX_ITEMS) {
            /* Keep the currency catalogue: a clear is 10 / 0 / 0, never 0 / 0 / 0. */
            for (unsigned i = 0; i < data->denom_number; ++i) {
                changed |= data->denom[i].pcs != 0 || data->denom[i].amount != 0;
                data->denom[i].pcs = 0;
                data->denom[i].amount = 0;
            }
        }
        if (changed) counting_denom_notify_changed(hooks);
    }
    counting_denom_record_history(hooks, buf, len);
    return COUNTING_DENOM_REPLY_START;
}

static counting_denom_reply_result_t counting_denom_end(
    counting_detail_state_t *detail, const counting_session_state_t *session,
    counting_sim_t *data, const uint8_t *buf, uint8_t len,
    const counting_denom_reply_hooks_t *hooks)
{
    if (detail->live_denom_started) {
        detail->live_denom_started = false;
        counting_denom_record_history(hooks, buf, len);
        if (detail->live_denom_count_owned)
            counting_report_denom_complete(session, app_clock_uptime_ms());
        return COUNTING_DENOM_REPLY_SESSION_END;
    }
    if (!detail->query_pending) return COUNTING_DENOM_REPLY_IGNORED;
    (void)counting_denom_query_complete(detail);
    counting_denom_record_history(hooks, buf, len);
    if (!session->start_confirmed && session->phase == COUNTING_SESSION_IDLE &&
        !session->end_anim_wait_detail && !session->last_result.valid &&
        counting_denom_query_commit(detail, data))
        counting_denom_notify_changed(hooks);
    return COUNTING_DENOM_REPLY_QUERY_END;
}

static counting_denom_reply_result_t counting_denom_data(
    counting_detail_state_t *detail, counting_sim_t *data,
    const uint8_t *buf, uint8_t len, const counting_denom_reply_hooks_t *hooks)
{
    const bool live = detail->live_denom_started;
    if (!live && (!detail->query_pending || !counting_denom_query_accepts_data(detail)))
        return COUNTING_DENOM_REPLY_IGNORED;

    int value, pcs;
    if (!counting_denom_parse_decimal(buf + 4, 8, &value) || value <= 0 ||
        !counting_denom_parse_decimal(buf + 12, 3, &pcs)) {
        if (!live) detail->query_overflow = true;
        uart_debug_printf("0x0B invalid denom detail frame\n");
        return COUNTING_DENOM_REPLY_IGNORED;
    }

    denom_t *rows = live ? data->denom : detail->query_denom;
    uint8_t *count = live ? &data->denom_number : &detail->query_denom_number;
    if (*count > COUNTING_DENOM_MAX_ITEMS) return COUNTING_DENOM_REPLY_IGNORED;
    unsigned slot = 0;
    while (slot < *count && rows[slot].value != value) ++slot;
    if (slot == COUNTING_DENOM_MAX_ITEMS) {
        if (!live) detail->query_overflow = true;
        uart_debug_printf("0x0B denom capacity exhausted value=%d\n", value);
        return COUNTING_DENOM_REPLY_IGNORED;
    }

    bool changed = slot == *count || rows[slot].pcs != (uint16_t)pcs;
    if (slot == *count) {
        rows[slot] = (denom_t){.value = value};
        ++*count;
    }
    /* Each row is the controller's current quantity, including zero/decrease
     * and retransmission. Never add a local baseline or count a 49 here. */
    rows[slot].pcs = (uint16_t)pcs;
    rows[slot].amount = (float)((uint64_t)(unsigned)value * (unsigned)pcs);
    counting_denom_record_history(hooks, buf, len);
    if (live) {
        detail->live_denom_seen |= (uint16_t)(1U << slot);
        if (changed) counting_denom_notify_changed(hooks);
    } else detail->query_activity_tick = app_clock_uptime_ms();
    return COUNTING_DENOM_REPLY_DATA;
}

counting_denom_reply_result_t counting_denom_reply_handle(
    counting_detail_state_t *detail, const counting_session_state_t *session,
    counting_sim_t *data, const uint8_t *buf, uint8_t len,
    const counting_denom_reply_hooks_t *hooks)
{
    if (!detail || !session || !data || !buf || len != 16)
        return COUNTING_DENOM_REPLY_INVALID;
    if (counting_denom_payload_is(buf, 0))
        return counting_denom_start(detail, data, buf, len, hooks);
    if (counting_denom_payload_is(buf, 0xFF))
        return counting_denom_end(detail, session, data, buf, len, hooks);
    return counting_denom_data(detail, data, buf, len, hooks);
}
