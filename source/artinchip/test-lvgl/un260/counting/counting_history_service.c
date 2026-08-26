#include "counting_history_service.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "un260/lv_system/ui_history_data.h"

#define COUNTING_HISTORY_FRAME_TEXT_SIZE 160
#define COUNTING_HISTORY_SESSION_LOG_SIZE 4096
#define COUNTING_HISTORY_HEX_BUFFER_SIZE (UINT8_MAX * 3U + 1U)
#define COUNTING_HISTORY_SAVE_RETRY_MAX 3U
#define COUNTING_HISTORY_SAVE_RETRY_MS 1000U

static char g_last_error_frame_text[COUNTING_HISTORY_FRAME_TEXT_SIZE];
static char g_last_start_frame_text[COUNTING_HISTORY_FRAME_TEXT_SIZE];
static char g_last_end_frame_text[COUNTING_HISTORY_FRAME_TEXT_SIZE];
static char g_session_log_text[COUNTING_HISTORY_SESSION_LOG_SIZE];
static size_t g_session_log_len;

static void counting_history_frame_to_hex(const uint8_t *buf,
                                          uint8_t len,
                                          char *out,
                                          size_t out_size)
{
    size_t pos = 0;

    if (out == NULL || out_size == 0U) {
        return;
    }
    out[0] = '\0';
    if (buf == NULL || len == 0U) {
        return;
    }

    for (uint16_t i = 0; i < len && pos + 3U < out_size; i++) {
        int written = snprintf(out + pos, out_size - pos, "%02X ", buf[i]);

        if (written != 3) {
            break;
        }
        pos += 3U;
    }
    if (pos > 0U) {
        out[pos - 1U] = '\0';
    }
}

static void counting_history_session_reset(void)
{
    g_last_start_frame_text[0] = '\0';
    g_last_end_frame_text[0] = '\0';
    g_session_log_text[0] = '\0';
    g_session_log_len = 0;
}

void counting_history_append_frame(const char *tag,
                                   const uint8_t *buf,
                                   uint8_t len)
{
    char hex[COUNTING_HISTORY_HEX_BUFFER_SIZE];
    int written;

    if (tag == NULL || buf == NULL || len == 0U ||
        g_session_log_len >= sizeof(g_session_log_text) - 1U) {
        return;
    }

    counting_history_frame_to_hex(buf, len, hex, sizeof(hex));
    written = snprintf(g_session_log_text + g_session_log_len,
                       sizeof(g_session_log_text) - g_session_log_len,
                       "%s %s\n", tag, hex);
    if (written <= 0) {
        return;
    }

    g_session_log_len += (size_t)written;
    if (g_session_log_len >= sizeof(g_session_log_text)) {
        g_session_log_len = sizeof(g_session_log_text) - 1U;
        g_session_log_text[g_session_log_len] = '\0';
    }
}

void counting_history_session_start(const uint8_t *buf, uint8_t len)
{
    g_last_error_frame_text[0] = '\0';
    counting_history_session_reset();
    counting_history_frame_to_hex(buf, len,
                                  g_last_start_frame_text,
                                  sizeof(g_last_start_frame_text));
    counting_history_append_frame("0x0A", buf, len);
}

void counting_history_capture_error(const char *tag,
                                    const uint8_t *buf,
                                    uint8_t len)
{
    counting_history_frame_to_hex(buf, len,
                                  g_last_error_frame_text,
                                  sizeof(g_last_error_frame_text));
    counting_history_append_frame(tag, buf, len);
}

void counting_history_capture_end(const uint8_t *buf, uint8_t len)
{
    counting_history_append_frame("0x0E", buf, len);
    counting_history_frame_to_hex(buf, len,
                                  g_last_end_frame_text,
                                  sizeof(g_last_end_frame_text));
}

static void counting_history_clear_pending(counting_session_state_t *session)
{
    session->history_record.valid = false;
    session->history_record.end_seen = false;
    session->history_record.save_attempts = 0;
    session->history_record.retry_tick = 0;
    session->history_record.pcs = 0;
    session->history_record.total_after = 0;
    session->history_record.amount = 0.0f;
    g_last_error_frame_text[0] = '\0';
    counting_history_session_reset();
}

counting_history_commit_result_t counting_history_try_commit(
    counting_session_state_t *session,
    const counting_sim_t *sim_data,
    uint32_t now_ms)
{
    bool appended;

    if (session == NULL || sim_data == NULL ||
        !session->history_record.valid || !session->history_record.end_seen) {
        return COUNTING_HISTORY_COMMIT_NOT_READY;
    }
    if (session->history_record.save_attempts >= COUNTING_HISTORY_SAVE_RETRY_MAX) {
        return COUNTING_HISTORY_COMMIT_NOT_READY;
    }
    appended = ui_history_record_append_from_session(
        sim_data,
        session->history_record.pcs,
        session->history_record.amount,
        session->history_record.total_after,
        g_last_error_frame_text,
        g_last_start_frame_text,
        g_last_end_frame_text,
        g_session_log_text);
    if (!appended) {
        session->history_record.save_attempts++;
        session->history_record.retry_tick = now_ms;
        return session->history_record.save_attempts >= COUNTING_HISTORY_SAVE_RETRY_MAX
            ? COUNTING_HISTORY_COMMIT_FAILED
            : COUNTING_HISTORY_COMMIT_RETRY_PENDING;
    }

    counting_history_clear_pending(session);
    return COUNTING_HISTORY_COMMIT_SAVED;
}

counting_history_commit_result_t counting_history_poll_commit(
    counting_session_state_t *session,
    const counting_sim_t *sim_data,
    uint32_t now_ms)
{
    if (session == NULL || sim_data == NULL ||
        !session->history_record.valid || !session->history_record.end_seen ||
        session->history_record.save_attempts == 0 ||
        session->history_record.save_attempts >= COUNTING_HISTORY_SAVE_RETRY_MAX ||
        (uint32_t)(now_ms - session->history_record.retry_tick) <
            COUNTING_HISTORY_SAVE_RETRY_MS) {
        return COUNTING_HISTORY_COMMIT_NOT_READY;
    }

    return counting_history_try_commit(session, sim_data, now_ms);
}

bool counting_history_discard_pending(counting_session_state_t *session)
{
    if (session == NULL || !session->history_record.valid) {
        return false;
    }

    counting_history_clear_pending(session);
    return true;
}
