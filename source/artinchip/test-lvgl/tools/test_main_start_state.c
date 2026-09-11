#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct { bool pending; } protocol_request_t;
typedef struct { bool start_confirmed; int phase; } counting_session_state_t;
typedef struct { int unused; } counting_sim_t;
enum { COUNTING_SESSION_IDLE, COUNTING_SESSION_ACTIVE, COUNTING_SESSION_FINISHED_WAIT_START };
enum { SMART_ISLAND_WARNING_LEVEL_ERROR = 1 };
static protocol_request_t g_start_request, g_clear_request;
static counting_session_state_t g_counting_session;
static struct { bool wait_sn_after_reject_end; } g_counting_detail_state;
static counting_sim_t data;
static bool reset_allowed, send_ok, observed_busy;
static unsigned sequence, reset_projection_at, finish_start_at, begin_clear_at;
static unsigned send_at, finish_clear_at, refresh_at, refreshes, warnings;
bool app_command_runtime_count_start_busy(void);

static bool protocol_request_is_pending(protocol_request_t *request) { return request->pending; }
static bool protocol_request_begin(protocol_request_t *request)
{
    assert(request == &g_clear_request); begin_clear_at = ++sequence;
    if (request->pending) return false;
    request->pending = true; return true;
}
static void protocol_request_finish(protocol_request_t *request)
{
    if (request == &g_start_request) finish_start_at = ++sequence;
    else { assert(request == &g_clear_request); finish_clear_at = ++sequence; }
    request->pending = false;
}
static int protocol_send(uint8_t command, const uint8_t *payload, unsigned length)
{
    assert(command == 0x3B && payload && payload[0] == 0x01 && length == 1);
    assert(g_clear_request.pending && !g_start_request.pending);
    send_at = ++sequence; return send_ok ? 0 : -1;
}
static bool app_counting_runtime_reset_session(counting_session_state_t *session, const char *reason)
{
    assert(session == &g_counting_session); (void)reason;
    if (!reset_allowed) return false;
    memset(session, 0, sizeof(*session)); return true;
}
static void smart_island_notify_warning_level(const char *message, int level)
{ assert(message && level == SMART_ISLAND_WARNING_LEVEL_ERROR); warnings++; }
static void app_setting_runtime_cancel_mode_clear(void) {}
static void stop_counting_sim(void) {}
static counting_sim_t *counting_data_mutable(void) { return &data; }
static void sim_reset_counting_result(counting_sim_t *value)
{
    assert(value == &data); reset_projection_at = ++sequence;
    /* The data-reset projection happens before CLEAR finishes pending START. */
    observed_busy = app_command_runtime_count_start_busy();
    assert(observed_busy);
}
static void currency_state_begin_count_session(void) {}
static void page_01_curr_img_refre(void) {}
static void page_01_main_refresh_start_state(void)
{
    refresh_at = ++sequence; refreshes++;
    observed_busy = app_command_runtime_count_start_busy();
}
static void uart_debug_printf(const char *format, ...) { assert(format); }
#include "main_start_state_under_test.h"

static void fixture(bool existing_clear, bool successful_send)
{
    g_start_request.pending = true; g_clear_request.pending = existing_clear;
    g_counting_session = (counting_session_state_t){true, COUNTING_SESSION_ACTIVE};
    g_counting_detail_state.wait_sn_after_reject_end = true;
    reset_allowed = true; send_ok = successful_send; observed_busy = true;
    sequence = reset_projection_at = finish_start_at = begin_clear_at = 0;
    send_at = finish_clear_at = refresh_at = refreshes = warnings = 0;
}
static void test_clear_projection(bool existing_clear, bool successful_send)
{
    fixture(existing_clear, successful_send);
    const bool result = app_command_runtime_clear_counting_data("test");
    assert(result == (!existing_clear && successful_send));
    assert(!g_start_request.pending && !g_counting_session.start_confirmed);
    assert(g_counting_session.phase == COUNTING_SESSION_IDLE);
    assert(!g_counting_detail_state.wait_sn_after_reject_end && !warnings);
    assert(reset_projection_at && finish_start_at > reset_projection_at);
    assert(begin_clear_at > finish_start_at);
    /* This is the regression: every CLEAR result must replace the stale busy
     * projection only after the request service has finished cancelling START. */
    assert(refreshes == 1 && refresh_at > begin_clear_at);
    assert(!observed_busy && !app_command_runtime_count_start_busy());
    if (existing_clear) {
        assert(!send_at && !finish_clear_at && g_clear_request.pending);
    } else if (successful_send) {
        assert(send_at > begin_clear_at && refresh_at > send_at);
        assert(!finish_clear_at && g_clear_request.pending);
    } else {
        assert(send_at > begin_clear_at && finish_clear_at > send_at);
        assert(refresh_at > finish_clear_at && !g_clear_request.pending);
    }
}
static void test_busy_sources_and_history_guard(void)
{
    fixture(false, true);
    g_counting_session.start_confirmed = false; g_counting_session.phase = COUNTING_SESSION_IDLE;
    assert(app_command_runtime_count_start_busy());
    g_start_request.pending = false; assert(!app_command_runtime_count_start_busy());
    g_counting_session.start_confirmed = true; assert(app_command_runtime_count_start_busy());
    g_counting_session.start_confirmed = false; g_counting_session.phase = COUNTING_SESSION_ACTIVE;
    assert(app_command_runtime_count_start_busy());
    g_counting_session.phase = COUNTING_SESSION_FINISHED_WAIT_START;
    assert(!app_command_runtime_count_start_busy());
    fixture(false, true); reset_allowed = false;
    assert(!app_command_runtime_clear_counting_data(NULL));
    assert(warnings == 1 && g_start_request.pending && g_counting_session.start_confirmed);
    assert(!refreshes && !finish_start_at && !begin_clear_at && !reset_projection_at);
}
int main(void)
{
    test_clear_projection(false, true);
    test_clear_projection(false, false);
    test_clear_projection(true, true);
    test_busy_sources_and_history_guard();
    puts("PASS: actual CLEAR success, send failure and already-pending CLEAR all project settled START after request cancellation");
    puts("PASS: pending/confirmed/active busy sources and history-reset refusal preserve the existing request/session boundary");
    return 0;
}
