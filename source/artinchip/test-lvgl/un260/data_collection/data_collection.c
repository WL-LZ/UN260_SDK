#include "data_collection.h"

#include <stdio.h>

#define DATA_COLLECTION_STATUS_MAX 128
#define DATA_COLLECTION_LATE_REPLY_GUARD_MS 3000U

typedef struct {
    bool pending;
    data_collect_mode_t target_mode;
    uint32_t started_ms;
    data_collect_mode_t late_target_mode;
    uint32_t late_guard_started_ms;
    char previous_status[DATA_COLLECTION_STATUS_MAX];
} data_collection_request_t;

static data_collect_mode_t g_mode = DATA_COLLECT_MODE_NONE;
static uint16_t g_pcs;
static char g_status[DATA_COLLECTION_STATUS_MAX] = "Please select a collection mode.";
static data_collection_request_t g_request;

static bool data_collection_mode_is_valid_request(data_collect_mode_t mode)
{
    return mode == DATA_COLLECT_MODE_NONE ||
           mode == DATA_COLLECT_MODE_ALL ||
           mode == DATA_COLLECT_MODE_FALSE;
}

static uint8_t data_collection_mode_reply_status(data_collect_mode_t mode)
{
    if (mode == DATA_COLLECT_MODE_ALL) return 0x01;
    if (mode == DATA_COLLECT_MODE_FALSE) return 0x03;
    return 0xFF;
}

static void data_collection_request_finish(void)
{
    g_request.pending = false;
    g_request.target_mode = DATA_COLLECT_MODE_NONE;
    g_request.started_ms = 0;
    g_request.previous_status[0] = '\0';
}

data_collect_mode_t data_collection_state_mode(void)
{
    return g_mode;
}

uint16_t data_collection_state_pcs(void)
{
    return g_pcs;
}

const char *data_collection_state_status(void)
{
    return g_status;
}

void data_collection_state_set_status(const char *status)
{
    snprintf(g_status, sizeof(g_status), "%s", status != NULL ? status : "");
}

void data_collection_state_select_mode(data_collect_mode_t mode, const char *status)
{
    if (mode != DATA_COLLECT_MODE_ALL && mode != DATA_COLLECT_MODE_FALSE) return;
    g_mode = mode;
    g_pcs = 0;
    data_collection_state_set_status(status);
}

void data_collection_state_reset_pcs(void)
{
    g_pcs = 0;
}

void data_collection_state_exit(const char *status)
{
    g_mode = DATA_COLLECT_MODE_NONE;
    g_pcs = 0;
    data_collection_state_set_status(status);
}

bool data_collection_request_begin(data_collect_mode_t target_mode,
                                   const char *status,
                                   uint32_t now_ms)
{
    if (!data_collection_mode_is_valid_request(target_mode) ||
        g_request.pending) {
        return false;
    }

    snprintf(g_request.previous_status, sizeof(g_request.previous_status),
             "%s", g_status);
    g_request.pending = true;
    g_request.target_mode = target_mode;
    g_request.started_ms = now_ms;
    g_request.late_target_mode = DATA_COLLECT_MODE_NONE;
    g_request.late_guard_started_ms = 0;
    data_collection_state_set_status(status);
    return true;
}

void data_collection_request_cancel(void)
{
    if (!g_request.pending) return;

    data_collection_state_set_status(g_request.previous_status);
    data_collection_request_finish();
}

bool data_collection_request_pending(void)
{
    return g_request.pending;
}

bool data_collection_request_take_timeout(uint32_t now_ms)
{
    data_collect_mode_t target_mode;

    if (!g_request.pending ||
        (uint32_t)(now_ms - g_request.started_ms) <
            DATA_COLLECTION_REQUEST_TIMEOUT_MS) {
        return false;
    }

    target_mode = g_request.target_mode;
    data_collection_request_finish();
    g_request.late_target_mode = target_mode;
    g_request.late_guard_started_ms = now_ms;
    data_collection_state_set_status("Collection mode request timed out.");
    return true;
}

static bool data_collection_reply_is_guarded(uint8_t status, uint32_t now_ms)
{
    if (g_request.late_guard_started_ms == 0) return false;
    if ((uint32_t)(now_ms - g_request.late_guard_started_ms) >=
        DATA_COLLECTION_LATE_REPLY_GUARD_MS) {
        g_request.late_guard_started_ms = 0;
        g_request.late_target_mode = DATA_COLLECT_MODE_NONE;
        return false;
    }
    if (status != data_collection_mode_reply_status(
                      g_request.late_target_mode)) {
        return false;
    }

    g_request.late_guard_started_ms = 0;
    g_request.late_target_mode = DATA_COLLECT_MODE_NONE;
    return true;
}

data_collection_reply_result_t data_collection_reply_handle(
    const uint8_t *buf, uint8_t len, uint32_t now_ms)
{
    const char *status;
    char unknown_status[48];
    uint8_t reply_status;

    /* 4 字节帧头/命令 + 1 字节状态 + 1 字节 CRC。 */
    if (buf == NULL || len < 6) return DATA_COLLECTION_REPLY_INVALID;

    reply_status = buf[4];
    if (data_collection_reply_is_guarded(reply_status, now_ms)) {
        return DATA_COLLECTION_REPLY_IGNORED;
    }

    if (g_request.pending &&
        reply_status == data_collection_mode_reply_status(
                            g_request.target_mode)) {
        data_collect_mode_t target_mode = g_request.target_mode;

        data_collection_request_finish();
        if (target_mode == DATA_COLLECT_MODE_NONE) {
            data_collection_state_exit("Collection mode exited.");
            return DATA_COLLECTION_REPLY_EXITED;
        }
        data_collection_state_select_mode(
            target_mode,
            target_mode == DATA_COLLECT_MODE_ALL ?
                "ALL DATA collection mode ready." :
                "FALSE REPORT collection mode ready.");
        return DATA_COLLECTION_REPLY_REQUEST_CONFIRMED;
    }
    if (g_request.pending &&
        (reply_status == 0x01 || reply_status == 0x03 ||
         reply_status == 0xFF)) {
        return DATA_COLLECTION_REPLY_IGNORED;
    }

    switch (reply_status) {
    case 0x01:
        status = "ALL DATA collection mode ready.";
        break;
    case 0x02:
        status = "Collection completed. Data can be copied from USB.";
        break;
    case 0x03:
        status = "FALSE REPORT collection mode ready.";
        break;
    case 0x05:
        status = "USB ready. You can start counting.";
        break;
    case 0x06:
        status = "USB not ready. Collection mode error.";
        if (g_request.pending) {
            data_collection_request_finish();
            data_collection_state_set_status(status);
            return DATA_COLLECTION_REPLY_REQUEST_FAILED;
        }
        break;
    case 0xFF:
        data_collection_state_exit("Collection mode exited.");
        return DATA_COLLECTION_REPLY_EXITED;
    default:
        snprintf(unknown_status, sizeof(unknown_status), "Collection reply: 0x%02X", reply_status);
        data_collection_state_set_status(unknown_status);
        return DATA_COLLECTION_REPLY_UNKNOWN;
    }

    data_collection_state_set_status(status);
    return DATA_COLLECTION_REPLY_STATUS_UPDATED;
}
