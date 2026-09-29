#include "counting_reject_sn_reply.h"
#include "counting_report_sync.h"
#include "un260/lv_system/app_clock.h"

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "un260/counting/counting_data_store.h"
#include "un260/lv_drivers/lv_drivers.h"

static void counting_detail_record_history(
    const counting_reject_sn_reply_hooks_t *hooks,
    const char *tag,
    const uint8_t *buf,
    uint8_t len)
{
    if (hooks != NULL && hooks->on_history_frame != NULL) {
        hooks->on_history_frame(hooks->context, tag, buf, len);
    }
}

static void counting_detail_notify(
    const counting_reject_sn_reply_hooks_t *hooks,
    void (*callback)(void *context))
{
    if (hooks != NULL && callback != NULL) {
        callback(hooks->context);
    }
}

static void counting_detail_notify_summary(
    const counting_reject_sn_reply_hooks_t *hooks, bool refresh_main)
{
    if (hooks != NULL && hooks->on_summary_changed != NULL) {
        hooks->on_summary_changed(hooks->context, refresh_main);
    }
}

static counting_detail_reply_result_t counting_reject_reply_handle(
    counting_detail_state_t *detail,
    counting_session_state_t *session,
    counting_sim_t *sim_data,
    const uint8_t *buf,
    uint8_t len,
    const counting_reject_sn_reply_hooks_t *hooks)
{
    uint8_t err_code;
    uint8_t pcs;

    if (detail == NULL || sim_data == NULL || buf == NULL || len != 7) {
        return COUNTING_DETAIL_REPLY_INVALID;
    }
    if (!counting_report_accept_reject(session)) return COUNTING_DETAIL_REPLY_IGNORED;

    err_code = buf[4];
    pcs = buf[5];
    if (err_code == 0x00 && pcs == 0x00) {
        /* 0x00 also means "reject pocket empty". Some controllers send it
         * after START with zero notes. Only tolerate this before any data
         * and with a zero live reject count; do not restart or extend timeout. */
        if (counting_report_reject_started() &&
            !sim_data->err_expected && !sim_data->err_num)
            return COUNTING_DETAIL_REPLY_IGNORED;
        if (!counting_report_reject_start()) return COUNTING_DETAIL_REPLY_INVALID;
        counting_report_touch(app_clock_uptime_ms());
        counting_data_clear_errors(sim_data);
        /* Keep err_expected from 0x0E for the main-page reject count. */
        counting_detail_record_history(hooks, "0x0C", buf, len);
        return COUNTING_DETAIL_REPLY_START;
    }

    if (err_code == 0xFF && pcs == 0xFF) {
        counting_detail_record_history(hooks, "0x0C", buf, len);
        counting_detail_notify(hooks, hooks != NULL
            ? hooks->on_reject_report_changed : NULL);
        if (hooks != NULL && hooks->on_reject_analysis_ready != NULL) {
            hooks->on_reject_analysis_ready(hooks->context);
        }
        uart_debug_printf("0x0C reject detail receive end, parsed=%u expected=%u\n",
                    (unsigned int)counting_data_error_detail_count(sim_data),
                    (unsigned int)sim_data->err_expected);
        counting_detail_notify_summary(hooks, true);
        if (counting_report_reject_end(session, sim_data, app_clock_uptime_ms()) ==
            COUNTING_REPORT_REUSED) {
            /* An unchanged empty ADD pass is not another counted report. */
            session->history_record.valid = false;
            session->history_record.end_seen = false;
        }
        return COUNTING_DETAIL_REPLY_END;
    }

    if (!counting_report_reject_started()) return COUNTING_DETAIL_REPLY_IGNORED;
    if (sim_data->err_expected == 0) {
        uart_debug_printf("0x0C detail ignored because err_expected=0\n");
        return COUNTING_DETAIL_REPLY_IGNORED;
    }
    counting_report_touch(app_clock_uptime_ms());

    if (!counting_data_ensure_error_capacity(
            sim_data, (int)sim_data->err_num + 1)) {
        uart_debug_printf("0x0C: err capacity fail idx=%u\n", sim_data->err_num);
        return COUNTING_DETAIL_REPLY_MEMORY_ERROR;
    }
    counting_detail_record_history(hooks, "0x0C", buf, len);

    {
        int index = sim_data->err_num;

        sim_data->err_pcs[index] = pcs;
        sim_data->err_code[index] = err_code;
        sim_data->err_num++;
    }

    counting_detail_notify(hooks, hooks != NULL
        ? hooks->on_reject_report_changed : NULL);
    counting_detail_notify_summary(hooks, false);
    return COUNTING_DETAIL_REPLY_DATA;
}

static void counting_sn_notify_item(
    const counting_reject_sn_reply_hooks_t *hooks)
{
    if (hooks && hooks->on_serial_item_changed)
        hooks->on_serial_item_changed(hooks->context);
}

static void counting_sn_notify_live(
    const counting_reject_sn_reply_hooks_t *hooks,
    int denomination, const char *serial_number)
{
    if (hooks && hooks->on_live_serial_received)
        hooks->on_live_serial_received(hooks->context, denomination, serial_number);
}

static counting_detail_reply_result_t counting_sn_reply_handle(
    counting_session_state_t *session, counting_sim_t *sim_data,
    const uint8_t *buf, uint8_t len,
    const counting_reject_sn_reply_hooks_t *hooks)
{
    if (!session || !sim_data || !buf) return COUNTING_DETAIL_REPLY_INVALID;
    if (counting_report_accept_serial(session))
        counting_detail_record_history(hooks, "0x0D", buf, len);
    counting_report_result_t result = counting_report_serial(
        session, sim_data, buf, len, app_clock_uptime_ms());
    if (result == COUNTING_REPORT_FAILED) return COUNTING_DETAIL_REPLY_INVALID;
    if (result != COUNTING_REPORT_READY) return COUNTING_DETAIL_REPLY_IGNORED;
    counting_detail_notify(hooks, hooks ? hooks->on_serial_report_ready : NULL);
    session->history_record.end_seen = true;
    counting_detail_notify(hooks, hooks ? hooks->on_history_record_ready : NULL);
    /* Report completion publishes data, never a machine start/stop event. */
    if (hooks && hooks->on_serial_ui_complete)
        hooks->on_serial_ui_complete(hooks->context, false);
    counting_detail_notify(hooks, hooks ? hooks->on_detail_complete : NULL);
    return COUNTING_DETAIL_REPLY_END;
}

static counting_detail_reply_result_t counting_sn_push_handle(
    counting_session_state_t *session,
    counting_sim_t *sim_data,
    const uint8_t *buf,
    uint8_t len,
    const counting_reject_sn_reply_hooks_t *hooks)
{
    enum { DENOM_FIELD_LEN = 7, PUSH_FRAME_LEN = 0x18 };
    char denom_text[DENOM_FIELD_LEN + 1];
    char serial_text[32];
    char *denom_start;
    char *denom_end;
    char *serial_start;
    char *serial_end;
    char *sn_copy;
    long denom_value;
    int index;
    int serial_len;

    if (session == NULL || sim_data == NULL || buf == NULL ||
        len != PUSH_FRAME_LEN) {
        return COUNTING_DETAIL_REPLY_INVALID;
    }
    if (!session->start_confirmed || session->phase == COUNTING_SESSION_FINISHED_WAIT_START)
        return COUNTING_DETAIL_REPLY_IGNORED;

    memcpy(denom_text, &buf[4], DENOM_FIELD_LEN);
    denom_text[DENOM_FIELD_LEN] = '\0';
    denom_start = denom_text;
    while (*denom_start == ' ') denom_start++;
    denom_end = denom_start + strlen(denom_start);
    while (denom_end > denom_start && denom_end[-1] == ' ') *--denom_end = '\0';
    if (*denom_start == '\0') return COUNTING_DETAIL_REPLY_IGNORED;

    denom_value = strtol(denom_start, &denom_end, 10);
    if (denom_value <= 0 || denom_value > INT_MAX || *denom_end != '\0') {
        return COUNTING_DETAIL_REPLY_IGNORED;
    }

    serial_len = (int)len - 1 - 4 - DENOM_FIELD_LEN;
    if (serial_len <= 0 || serial_len >= (int)sizeof(serial_text)) {
        return COUNTING_DETAIL_REPLY_INVALID;
    }
    memcpy(serial_text, &buf[4 + DENOM_FIELD_LEN], (size_t)serial_len);
    serial_text[serial_len] = '\0';
    serial_start = serial_text;
    while (*serial_start == ' ') serial_start++;
    serial_end = serial_start + strlen(serial_start);
    while (serial_end > serial_start && serial_end[-1] == ' ') *--serial_end = '\0';
    if (*serial_start == '\0') return COUNTING_DETAIL_REPLY_IGNORED;

    bool cleared;
    index = counting_report_live_slot(sim_data, &cleared);
    if (cleared) counting_detail_notify(hooks, hooks ? hooks->on_serial_data_started : NULL);
    if (index < 0 || index >= COUNTING_DATA_MAX_ITEMS ||
        !counting_data_ensure_serial_capacity(sim_data, index + 1)) {
        return COUNTING_DETAIL_REPLY_MEMORY_ERROR;
    }

    sn_copy = malloc(strlen(serial_start) + 1U);
    if (sn_copy == NULL) return COUNTING_DETAIL_REPLY_MEMORY_ERROR;
    strcpy(sn_copy, serial_start);
    free(sim_data->sn_str[index]);
    sim_data->sn_str[index] = sn_copy;
    sim_data->denom_mix[index] = (int)denom_value;

    counting_detail_record_history(hooks, "0x49", buf, len);
    counting_sn_notify_item(hooks);
    counting_sn_notify_live(hooks, (int)denom_value, serial_start);
    return COUNTING_DETAIL_REPLY_DATA;
}

counting_detail_reply_result_t counting_reject_sn_reply_dispatch(
    uint8_t cmd,
    counting_detail_state_t *detail,
    counting_session_state_t *session,
    counting_sim_t *sim_data,
    const uint8_t *buf,
    uint8_t len,
    const counting_reject_sn_reply_hooks_t *hooks)
{
    if (buf && counting_report_discard_stale(cmd, buf, len))
        return COUNTING_DETAIL_REPLY_IGNORED;
    switch (cmd) {
    case 0x0C:
        return counting_reject_reply_handle(detail, session, sim_data, buf, len, hooks);
    case 0x0D:
        return counting_sn_reply_handle(session, sim_data, buf, len, hooks);
    case 0x49:
        return counting_sn_push_handle(session, sim_data, buf, len, hooks);
    default:
        return COUNTING_DETAIL_REPLY_INVALID;
    }
}
