#include "counting_denom_reply.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "counting_denom_query_service.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_system/app_clock.h"

static void counting_denom_record_history(const counting_denom_reply_hooks_t *hooks,
                                          const uint8_t *buf,
                                          uint8_t len)
{
    if (hooks != NULL && hooks->on_history_frame != NULL) {
        hooks->on_history_frame("0x0B", buf, len);
    }
}

static void counting_denom_notify_main_data_changed(
    const counting_denom_reply_hooks_t *hooks)
{
    if (hooks != NULL && hooks->on_main_data_changed != NULL) {
        hooks->on_main_data_changed();
    }
}

static bool counting_denom_payload_is(const uint8_t *buf, uint8_t value)
{
    for (int i = 4; i < 15; i++) {
        if (buf[i] != value) {
            return false;
        }
    }
    return true;
}

static bool counting_denom_parse_decimal(const uint8_t *data,
                                         size_t length,
                                         int *out)
{
    unsigned int value = 0;
    bool digit_seen = false;
    bool trailing_padding = false;

    if (data == NULL || out == NULL || length == 0) {
        return false;
    }

    for (size_t i = 0; i < length; i++) {
        uint8_t ch = data[i];

        if (ch >= '0' && ch <= '9') {
            unsigned int digit = (unsigned int)(ch - '0');

            if (trailing_padding || value > ((unsigned int)INT_MAX - digit) / 10U) {
                return false;
            }
            value = value * 10U + digit;
            digit_seen = true;
        } else if (ch == ' ' || ch == '\0') {
            if (digit_seen) {
                trailing_padding = true;
            }
        } else {
            return false;
        }
    }

    if (!digit_seen) {
        return false;
    }
    *out = (int)value;
    return true;
}

static uint16_t counting_denom_add_pcs(uint16_t current, int increment)
{
    if (increment <= 0) {
        return current;
    }
    if ((unsigned int)increment > UINT16_MAX - current) {
        return UINT16_MAX;
    }
    return (uint16_t)(current + (uint16_t)increment);
}

static counting_denom_reply_result_t counting_denom_handle_end(
    counting_detail_state_t *detail,
    const counting_session_state_t *session,
    counting_sim_t *sim_data,
    const uint8_t *buf,
    uint8_t len,
    const counting_denom_reply_hooks_t *hooks)
{
    bool query_was_pending;

    uart_debug_printf("0x0B denom detail receive end\n");
    if (detail->live_denom_started) {
        bool valid = !detail->live_denom_invalid;
        detail->live_denom_started = false;
        if (!valid) {
            counting_denom_record_history(hooks, buf, len);
            return COUNTING_DENOM_REPLY_IGNORED;
        }
        memcpy(sim_data->denom, detail->live_denom, sizeof(sim_data->denom));
        sim_data->denom_number = detail->live_denom_number;
        counting_denom_notify_main_data_changed(hooks);
        /* A snapshot that straddles the real stop still needs final queries. */
        if (session->phase == COUNTING_SESSION_ACTIVE) {
            counting_denom_record_history(hooks, buf, len);
            detail->wait_sn_after_reject_end = false;
            return COUNTING_DENOM_REPLY_SESSION_END;
        }
    } else if (session->phase == COUNTING_SESSION_ACTIVE && !detail->query_pending) {
        /* Never publish an orphan terminator as a finished count. */
        return COUNTING_DENOM_REPLY_IGNORED;
    }
    query_was_pending = counting_denom_query_complete(detail);
    counting_denom_record_history(hooks, buf, len);

    if (query_was_pending) {
        if (session->phase != COUNTING_SESSION_IDLE ||
            session->end_anim_wait_detail || session->last_result.valid) {
            uart_debug_printf(                        "0x0B query result discarded during counting session\n");
        } else if (counting_denom_query_commit(detail, sim_data)) {
            counting_denom_notify_main_data_changed(hooks);
        } else {
            uart_debug_printf("0x0B query result commit failed\n");
        }
        return COUNTING_DENOM_REPLY_QUERY_END;
    }

    if (session->phase != COUNTING_SESSION_ACTIVE) {
        uint8_t reject_cmd = 0x01;
        protocol_send(0x0C, &reject_cmd, 1);
    }
    detail->wait_sn_after_reject_end = session->phase != COUNTING_SESSION_ACTIVE;
    if (session->phase != COUNTING_SESSION_ACTIVE) {
        counting_denom_notify_main_data_changed(hooks);
    }
    return COUNTING_DENOM_REPLY_SESSION_END;
}

static counting_denom_reply_result_t counting_denom_handle_data(
    counting_detail_state_t *detail,
    counting_sim_t *sim_data,
    const uint8_t *buf,
    uint8_t len,
    const counting_denom_reply_hooks_t *hooks)
{
    denom_t *denom_items;
    uint8_t *denom_count;
    int denom_capacity = COUNTING_DENOM_MAX_ITEMS;
    int denom;
    int pcs;

    if (!counting_denom_parse_decimal(&buf[4], 8, &denom) ||
        !counting_denom_parse_decimal(&buf[12], 3, &pcs) ||
        denom <= 0 || pcs < 0) {
        if (detail->live_denom_started) detail->live_denom_invalid = true;
        uart_debug_printf("0x0B invalid denom detail frame\n");
        return COUNTING_DENOM_REPLY_IGNORED;
    }
    if (!counting_denom_query_accepts_data(detail)) {
        uart_debug_printf("0x0B query data ignored before start frame\n");
        return COUNTING_DENOM_REPLY_IGNORED;
    }
    if (detail->live_denom_started) {
        denom_items = detail->live_denom;
        denom_count = &detail->live_denom_number;
    } else if (detail->query_pending) {
        detail->query_activity_tick = app_clock_uptime_ms();
        denom_items = detail->query_denom;
        denom_count = &detail->query_denom_number;
    } else {
        denom_items = sim_data->denom;
        denom_count = &sim_data->denom_number;
    }
    if (*denom_count > denom_capacity) {
        uart_debug_printf("0x0B invalid denom item count=%u\n",
                    (unsigned int)*denom_count);
        return COUNTING_DENOM_REPLY_IGNORED;
    }

    counting_denom_record_history(hooks, buf, len);

    for (int i = 0; i < *denom_count; i++) {
        if (denom_items[i].value == denom) {
            uint16_t old_pcs = denom_items[i].pcs;

            denom_items[i].pcs = counting_denom_add_pcs(old_pcs, pcs);
            if (denom_items[i].pcs == UINT16_MAX &&
                ((unsigned int)pcs > UINT16_MAX - old_pcs)) {
                uart_debug_printf("0x0B denom pcs saturated value=%d\n", denom);
            }
            denom_items[i].amount =
                (float)denom * (float)denom_items[i].pcs;
            return COUNTING_DENOM_REPLY_DATA;
        }
    }

    if (*denom_count < denom_capacity) {
        int index = *denom_count;

        denom_items[index].value = denom;
        denom_items[index].pcs = counting_denom_add_pcs(0, pcs);
        denom_items[index].amount =
            (float)denom * (float)denom_items[index].pcs;
        (*denom_count)++;
        return COUNTING_DENOM_REPLY_DATA;
    }

    if (detail->query_pending) detail->query_overflow = true;
    if (detail->live_denom_started) detail->live_denom_invalid = true;
    uart_debug_printf("0x0B denom capacity exhausted value=%d\n", denom);
    return COUNTING_DENOM_REPLY_IGNORED;
}

counting_denom_reply_result_t counting_denom_reply_handle(
    counting_detail_state_t *detail,
    const counting_session_state_t *session,
    counting_sim_t *sim_data,
    const uint8_t *buf,
    uint8_t len,
    const counting_denom_reply_hooks_t *hooks)
{
    if (detail == NULL || session == NULL || sim_data == NULL || buf == NULL || len < 15) {
        return COUNTING_DENOM_REPLY_INVALID;
    }

    if (counting_denom_payload_is(buf, 0x00)) {
        if (detail->query_failed && session->phase != COUNTING_SESSION_IDLE) {
            detail->query_pending = false;
            detail->query_expired = false;
            detail->query_started = false;
        }
        if (session->phase == COUNTING_SESSION_ACTIVE && !detail->query_pending) {
            memset(detail->live_denom, 0, sizeof(detail->live_denom));
            detail->live_denom_number = 0;
            detail->live_denom_invalid = false;
            detail->live_denom_started = true;
            counting_denom_record_history(hooks, buf, len);
            return COUNTING_DENOM_REPLY_START;
        }
        detail->live_denom_started = false;
        if (!counting_denom_query_mark_start(detail)) {
            memset(sim_data->denom, 0, sizeof(sim_data->denom));
            sim_data->denom_number = 0;
        }
        counting_denom_record_history(hooks, buf, len);
        uart_debug_printf("0x0B denom detail receive start\n");
        return COUNTING_DENOM_REPLY_START;
    }

    if (counting_denom_payload_is(buf, 0xFF)) {
        return counting_denom_handle_end(detail, session, sim_data,
                                         buf, len, hooks);
    }

    if (session->phase == COUNTING_SESSION_ACTIVE &&
        !detail->query_pending && !detail->live_denom_started)
        return COUNTING_DENOM_REPLY_IGNORED;
    return counting_denom_handle_data(detail, sim_data, buf, len, hooks);
}
