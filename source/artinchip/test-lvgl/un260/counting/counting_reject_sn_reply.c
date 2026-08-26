#include "counting_reject_sn_reply.h"

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "un260/counting/counting_data_store.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/protocol/protocol_send.h"

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
    counting_sim_t *sim_data,
    const uint8_t *buf,
    uint8_t len,
    const counting_reject_sn_reply_hooks_t *hooks)
{
    uint8_t err_code;
    uint8_t pcs;

    if (detail == NULL || sim_data == NULL || buf == NULL || len < 7) {
        return COUNTING_DETAIL_REPLY_INVALID;
    }

    err_code = buf[4];
    pcs = buf[5];
    if (err_code == 0x00 && pcs == 0x00) {
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
        if (detail->wait_sn_after_reject_end) {
            uint8_t sn_req[2] = {0x01, 0x01};
            protocol_send(0x0D, sn_req, 2);
            detail->wait_sn_after_reject_end = false;
        }
        return COUNTING_DETAIL_REPLY_END;
    }

    if (sim_data->err_expected == 0) {
        uart_debug_printf("0x0C detail ignored because err_expected=0\n");
        return COUNTING_DETAIL_REPLY_IGNORED;
    }

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

static bool counting_sn_payload_is(const uint8_t *buf, int payload_end, uint8_t value)
{
    for (int i = 4; i < payload_end; i++) {
        if (buf[i] != value) {
            return false;
        }
    }
    return true;
}

static void counting_sn_notify_item(
    const counting_reject_sn_reply_hooks_t *hooks)
{
    if (hooks != NULL && hooks->on_serial_item_changed != NULL) {
        hooks->on_serial_item_changed(hooks->context);
    }
}

static void counting_sn_notify_live(
    const counting_reject_sn_reply_hooks_t *hooks,
    int denomination,
    const char *serial_number)
{
    if (hooks != NULL && hooks->on_live_serial_received != NULL) {
        hooks->on_live_serial_received(hooks->context,
                                       denomination,
                                       serial_number);
    }
}

static counting_detail_reply_result_t counting_sn_reply_handle(
    counting_session_state_t *session,
    counting_sim_t *sim_data,
    const uint8_t *buf,
    uint8_t len,
    const counting_reject_sn_reply_hooks_t *hooks)
{
    int payload_len;
    int payload_end;
    uint8_t sequence;
    int index;
    int ascii_len;
    char ascii_buf[32];
    char *cursor;
    int denom = 0;

    if (session == NULL || sim_data == NULL || buf == NULL || len < 6) {
        return COUNTING_DETAIL_REPLY_INVALID;
    }

    payload_len = len - 4;
    if (payload_len < 2) {
        return COUNTING_DETAIL_REPLY_INVALID;
    }
    payload_end = len - 1;

    if (counting_sn_payload_is(buf, payload_end, 0x00)) {
        counting_data_clear_serials(sim_data);
        counting_detail_notify(hooks, hooks != NULL
            ? hooks->on_serial_data_started : NULL);
        counting_detail_record_history(hooks, "0x0D", buf, len);
        return COUNTING_DETAIL_REPLY_START;
    }

    if (counting_sn_payload_is(buf, payload_end, 0xFF)) {
        bool begin_end_anim = session->end_anim_wait_detail;

        counting_detail_notify(hooks, hooks != NULL
            ? hooks->on_serial_report_ready : NULL);
        counting_detail_record_history(hooks, "0x0D", buf, len);
        session->history_record.end_seen = true;
        if (hooks != NULL && hooks->on_history_record_ready != NULL) {
            hooks->on_history_record_ready(hooks->context);
        }
        if (session->end_anim_wait_detail) {
            session->end_anim_wait_detail = false;
        }
        if (hooks != NULL && hooks->on_serial_ui_complete != NULL) {
            hooks->on_serial_ui_complete(hooks->context, begin_end_anim);
        }
        if (hooks != NULL && hooks->on_detail_complete != NULL) {
            hooks->on_detail_complete(hooks->context);
        }
        return COUNTING_DETAIL_REPLY_END;
    }

    sequence = buf[4];
    if (sequence == 0x00 || sequence == 0xFF) {
        return COUNTING_DETAIL_REPLY_IGNORED;
    }
    index = (int)sequence - 1;
    if (index < 0 || index >= COUNTING_DATA_MAX_ITEMS) {
        return COUNTING_DETAIL_REPLY_IGNORED;
    }

    ascii_len = payload_len - 2;
    if (ascii_len <= 0) {
        return COUNTING_DETAIL_REPLY_IGNORED;
    }
    counting_detail_record_history(hooks, "0x0D", buf, len);

    if (ascii_len >= (int)sizeof(ascii_buf)) {
        ascii_len = (int)sizeof(ascii_buf) - 1;
    }
    memcpy(ascii_buf, &buf[5], (size_t)ascii_len);
    ascii_buf[ascii_len] = '\0';

    while (ascii_len > 0 && ascii_buf[ascii_len - 1] == ' ') {
        ascii_buf[--ascii_len] = '\0';
    }
    cursor = ascii_buf;
    while (*cursor == ' ') {
        cursor++;
    }
    if (*cursor == '\0') {
        return COUNTING_DETAIL_REPLY_IGNORED;
    }

    while (*cursor != '\0' && isdigit((unsigned char)*cursor)) {
        int digit = *cursor - '0';
        if (denom > (INT_MAX - digit) / 10) {
            return COUNTING_DETAIL_REPLY_IGNORED;
        }
        denom = denom * 10 + digit;
        cursor++;
    }
    while (*cursor == ' ') {
        cursor++;
    }
    if (*cursor == '\0') {
        return COUNTING_DETAIL_REPLY_IGNORED;
    }

    if (!counting_data_ensure_serial_capacity(sim_data, index + 1)) {
        uart_debug_printf("0x0D: SN capacity fail idx=%d\n", index);
        return COUNTING_DETAIL_REPLY_MEMORY_ERROR;
    }

    {
        size_t sn_len = strlen(cursor);
        char *sn_copy = malloc(sn_len + 1);
        if (sn_copy == NULL) {
            uart_debug_printf("0x0D: SN malloc fail idx=%d\n", index);
            return COUNTING_DETAIL_REPLY_MEMORY_ERROR;
        }
        memcpy(sn_copy, cursor, sn_len + 1);
        free(sim_data->sn_str[index]);
        sim_data->sn_str[index] = sn_copy;
        sim_data->denom_mix[index] = denom;
    }

    counting_sn_notify_item(hooks);
    return COUNTING_DETAIL_REPLY_DATA;
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

    index = 0;
    while (index < counting_data_serial_scan_limit(sim_data) &&
           sim_data->sn_str[index] != NULL) {
        index++;
    }
    if (index >= COUNTING_DATA_MAX_ITEMS ||
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
    switch (cmd) {
    case 0x0C:
        return counting_reject_reply_handle(detail, sim_data, buf, len, hooks);
    case 0x0D:
        return counting_sn_reply_handle(session, sim_data, buf, len, hooks);
    case 0x49:
        return counting_sn_push_handle(session, sim_data, buf, len, hooks);
    default:
        return COUNTING_DETAIL_REPLY_INVALID;
    }
}
