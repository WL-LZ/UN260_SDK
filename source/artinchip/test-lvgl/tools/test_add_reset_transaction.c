/* Exercise the production ADD owner. Only hardware/history/UI endpoints are
 * observable stubs; no state machine transitions are copied into this file. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "un260/app_service/app_add_runtime.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/counting/counting_action_service.h"
#include "un260/lv_components/ui_notice.h"

static bool admission = true, allowed = true, commit_allowed = true;
static bool setting_send = true, clear_send = true;
static bool add_enabled, setting_pending, clear_pending;
static unsigned setting_tx, clear_tx, commits, admissions, checks;
static unsigned commit_attempts;
static unsigned existing_count = 100, history_count = 7;
static bool last_target;
static uint32_t now;
static counting_clear_result_t clear_result;
static unsigned warnings, progress, successes;

bool app_command_runtime_prepare_add_reset(void)
{
    ++admissions;
    return admission;
}
bool app_command_runtime_add_clear_allowed(void)
{
    ++checks;
    return allowed;
}
bool app_command_runtime_commit_add_reset(void)
{
    ++commit_attempts;
    if (!commit_allowed) return false;
    ++commits;
    existing_count = 0;
    return true;
}
bool setting_service_request_add(bool target)
{
    if (!setting_send || setting_pending) return false;
    ++setting_tx;
    last_target = target;
    setting_pending = true;
    return true;
}
bool setting_service_add_is_pending(void) { return setting_pending; }
bool setting_service_add_pending(void) { return setting_pending; }
bool machine_state_add_enabled(void) { return add_enabled; }
void machine_state_confirm_add(bool enabled) { add_enabled = enabled; }
bool counting_action_request_clear(void)
{
    if (!clear_send || clear_pending) return false;
    ++clear_tx;
    clear_pending = true;
    clear_result = COUNTING_CLEAR_RESULT_NONE;
    return true;
}
counting_clear_result_t counting_action_take_clear_result(void)
{
    counting_clear_result_t value = clear_result;
    clear_result = COUNTING_CLEAR_RESULT_NONE;
    return value;
}
bool counting_action_clear_pending(void) { return clear_pending; }
bool counting_action_start_pending(void) { return false; }
uint32_t app_clock_uptime_ms(void) { return now; }
void ui_notice_post_text(ui_notice_kind_t kind, const char *key,
                         const char *title, const char *detail)
{
    assert(key && title && detail);
    if (kind == UI_NOTICE_WARNING || kind == UI_NOTICE_ERROR) ++warnings;
    else if (kind == UI_NOTICE_PROGRESS) ++progress;
    else if (kind == UI_NOTICE_SUCCESS) ++successes;
}
void ui_notice_clear(const char *key) { assert(key); }
void app_setting_notice_result(const char *key, const char *title, bool success)
{
    assert(key && title);
    if (success) ++successes; else ++warnings;
}
void app_setting_notice_timeout(const char *key, const char *title)
{ assert(key && title); ++warnings; }
void page_01_main_refresh_start_state(void) {}
void page_01_bottom_a_refresh_add(bool animation) { (void)animation; }
void page_03_update_menu_button_states_refresh(void) {}
void smart_island_refresh_summary(void) {}
void uart_debug_printf(const char *format, ...) { (void)format; }

static void setting_ack(bool target, bool success)
{
    setting_pending = false;
    if (success) add_enabled = target;
    app_add_runtime_setting_result(target, success);
    app_add_runtime_poll();
}
static void clear_ack(counting_clear_result_t result)
{
    clear_pending = false;
    clear_result = result;
    app_add_runtime_poll();
}
static void begin(bool target)
{
    assert(!app_add_runtime_busy());
    assert(app_add_runtime_before_count_start());
    assert(app_add_runtime_request(target));
    assert(app_add_runtime_busy() && app_add_runtime_waiting_setting());
    assert(setting_tx == 1 && clear_tx == 0 && last_target == target);
    assert(!app_add_runtime_request(!target));
    assert(setting_tx == 1 && existing_count == 100 && !commits);
}

static void normal(bool enabled)
{
    add_enabled = !enabled;
    begin(enabled);
    setting_ack(enabled, true);
    assert(app_add_runtime_busy() && !app_add_runtime_waiting_setting());
    assert(add_enabled == enabled && clear_tx == 1 && !commits);
    assert(existing_count == 100);
    clear_ack(COUNTING_CLEAR_RESULT_SUCCESS);
    assert(!app_add_runtime_busy() && commits == 1 && existing_count == 0);
    assert(add_enabled == enabled && history_count == 7);
    assert(app_add_runtime_before_count_start());
    /* A duplicate terminal event has no owner and cannot erase a later count. */
    existing_count = 20;
    clear_ack(COUNTING_CLEAR_RESULT_SUCCESS);
    assert(commits == 1 && existing_count == 20 && !app_add_runtime_busy());
}
static void setting_failure(bool timeout)
{
    begin(true);
    if (timeout) {
        setting_pending = false;
        app_add_runtime_setting_timeout();
        app_add_runtime_poll();
    } else setting_ack(false, false); /* A negative ACK carries no target. */
    assert(!app_add_runtime_waiting_setting() && !app_add_runtime_busy());
    assert(!add_enabled && !clear_tx && !commits && existing_count == 100);
    assert(app_add_runtime_before_count_start());
    assert(app_add_runtime_request(true) && setting_tx == 2);
}
static void clear_failure(counting_clear_result_t failure)
{
    begin(true);
    setting_ack(true, true);
    assert(clear_tx == 1);
    clear_ack(failure);
    assert(app_add_runtime_busy() && !app_add_runtime_waiting_setting());
    assert(add_enabled && existing_count == 100 && !commits);
    assert(app_add_runtime_retry_clear());
    assert(clear_tx == 2 && setting_tx == 1 && !commits);
    clear_ack(COUNTING_CLEAR_RESULT_SUCCESS);
    assert(!app_add_runtime_busy() && commits == 1 && existing_count == 0);
}
static void send_failure(bool setting)
{
    if (setting) {
        setting_send = false;
        assert(!app_add_runtime_request(true));
        assert(!app_add_runtime_busy() && !setting_tx && !clear_tx);
        assert(existing_count == 100 && !commits);
        setting_send = true;
        assert(app_add_runtime_request(true) && setting_tx == 1);
    } else {
        begin(true);
        clear_send = false;
        setting_ack(true, true);
        assert(app_add_runtime_busy() && add_enabled && !clear_tx && !commits);
        assert(existing_count == 100);
        clear_send = true;
        assert(app_add_runtime_retry_clear() && clear_tx == 1 && setting_tx == 1);
        clear_ack(COUNTING_CLEAR_RESULT_SUCCESS);
        assert(!app_add_runtime_busy() && existing_count == 0 && commits == 1);
    }
}
static void no_owner(void)
{
    /* Boot/profile synchronization can update confirmed ADD outside this owner.
     * It must neither clear data nor acquire the user's new-session intent. */
    add_enabled = true;
    app_add_runtime_setting_result(true, true);
    app_add_runtime_setting_timeout();
    clear_ack(COUNTING_CLEAR_RESULT_SUCCESS);
    assert(!app_add_runtime_busy() && !setting_tx && !clear_tx && !commits);
    assert(existing_count == 100 && history_count == 7);
}
static void admission_failure(void)
{
    admission = false;
    assert(!app_add_runtime_request(true));
    assert(!setting_tx && !clear_tx && !commits && !app_add_runtime_busy());
    assert(existing_count == 100 && !add_enabled);
}
static void boundary_wait(void)
{
    begin(true);
    allowed = false;
    setting_ack(true, true);
    assert(app_add_runtime_busy() && !clear_tx && !commits);
    allowed = true;
    app_add_runtime_poll();
    assert(clear_tx == 1 && existing_count == 100);
    clear_ack(COUNTING_CLEAR_RESULT_SUCCESS);
    assert(!app_add_runtime_busy() && commits == 1);
}
static void commit_wait(void)
{
    begin(true);
    setting_ack(true, true);
    commit_allowed = false;
    clear_ack(COUNTING_CLEAR_RESULT_SUCCESS);
    assert(app_add_runtime_busy() && existing_count == 100 && !commits);
    assert(commit_attempts == 1);
    assert(!app_add_runtime_before_count_start());
    commit_allowed = true;
    now = 99;
    app_add_runtime_poll();
    assert(app_add_runtime_busy() && commit_attempts == 1 && !commits);
    now = 100;
    app_add_runtime_poll();
    assert(!app_add_runtime_busy() && commits == 1 && commit_attempts == 2 && clear_tx == 1 && setting_tx == 1);
}

static void hardware_start(bool during_setting)
{
    begin(true);
    if (!during_setting) setting_ack(true, true);
    /* A physical START is a fact. It must not be rejected like a UI command,
     * nor may an eventual old CLEAR ACK erase its new result. */
    assert(app_add_runtime_before_count_start());
    existing_count = 120;
    allowed = false;
    if (during_setting) setting_ack(true, true);
    assert(app_add_runtime_busy() && !app_add_runtime_waiting_setting());
    assert(add_enabled && existing_count == 120 && !commits);
    clear_ack(COUNTING_CLEAR_RESULT_SUCCESS);
    assert(existing_count == 120 && !commits && app_add_runtime_busy());
    assert(!app_add_runtime_retry_clear());
    allowed = true;
    unsigned sent = clear_tx;
    /* Tapping ADD while recovery is pending retries CLEAR, never 39. */
    assert(app_add_runtime_request(false));
    assert(setting_tx == 1 && clear_tx == sent + 1 && add_enabled);
    clear_ack(COUNTING_CLEAR_RESULT_SUCCESS);
    assert(!app_add_runtime_busy() && existing_count == 0 && commits == 1);
}
static void boundary_timeout(void)
{
    begin(true);
    allowed = false;
    setting_ack(true, true);
    now = 2000;
    app_add_runtime_poll();
    assert(app_add_runtime_busy() && !clear_tx && !commits && existing_count == 100);
    allowed = true;
    app_add_runtime_poll();
    assert(!clear_tx); /* Timeout requires a deliberate retry. */
    assert(app_add_runtime_retry_clear() && clear_tx == 1);
    clear_ack(COUNTING_CLEAR_RESULT_SUCCESS);
    assert(!app_add_runtime_busy() && commits == 1);
}
static void unmatched_setting(void)
{
    begin(true);
    app_add_runtime_setting_result(false, true);
    assert(app_add_runtime_waiting_setting() && !clear_tx && existing_count == 100);
    setting_ack(true, true);
    clear_ack(COUNTING_CLEAR_RESULT_SUCCESS);
    assert(!app_add_runtime_busy() && commits == 1);
}
static void cancelled(bool setting)
{
    begin(true);
    if (!setting) setting_ack(true, true);
    if (setting) setting_pending = false; else clear_pending = false;
    app_add_runtime_poll();
    assert(existing_count == 100 && !commits);
    assert(!app_add_runtime_waiting_setting());
    if (setting) {
        assert(!app_add_runtime_busy() && !clear_tx && !add_enabled);
    } else {
        assert(app_add_runtime_busy() && add_enabled);
        assert(app_add_runtime_retry_clear());
        clear_ack(COUNTING_CLEAR_RESULT_SUCCESS);
        assert(!app_add_runtime_busy() && commits == 1);
    }
}
static void unchanged_value(void)
{
    assert(app_add_runtime_request(false));
    assert(!setting_tx && !clear_tx && !commits && !app_add_runtime_busy());
    assert(existing_count == 100 && !admissions);
}
static void old_clear_result(void)
{
    begin(true);
    clear_result = COUNTING_CLEAR_RESULT_SUCCESS;
    setting_ack(true, true);
    app_add_runtime_poll();
    assert(clear_tx == 1 && !commits && existing_count == 100);
    clear_ack(COUNTING_CLEAR_RESULT_SUCCESS);
    assert(commits == 1 && !app_add_runtime_busy());
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "enable")) normal(true);
    else if (!strcmp(argv[1], "disable")) normal(false);
    else if (!strcmp(argv[1], "setting-failed")) setting_failure(false);
    else if (!strcmp(argv[1], "setting-timeout")) setting_failure(true);
    else if (!strcmp(argv[1], "clear-failed")) clear_failure(COUNTING_CLEAR_RESULT_FAILED);
    else if (!strcmp(argv[1], "clear-timeout")) clear_failure(COUNTING_CLEAR_RESULT_TIMEOUT);
    else if (!strcmp(argv[1], "setting-send")) send_failure(true);
    else if (!strcmp(argv[1], "clear-send")) send_failure(false);
    else if (!strcmp(argv[1], "no-owner")) no_owner();
    else if (!strcmp(argv[1], "history-full")) admission_failure();
    else if (!strcmp(argv[1], "boundary-wait")) boundary_wait();
    else if (!strcmp(argv[1], "commit-wait")) commit_wait();
    else if (!strcmp(argv[1], "hardware-setting")) hardware_start(true);
    else if (!strcmp(argv[1], "hardware-clear")) hardware_start(false);
    else if (!strcmp(argv[1], "boundary-timeout")) boundary_timeout();
    else if (!strcmp(argv[1], "unmatched-setting")) unmatched_setting();
    else if (!strcmp(argv[1], "setting-cancel")) cancelled(true);
    else if (!strcmp(argv[1], "clear-cancel")) cancelled(false);
    else if (!strcmp(argv[1], "unchanged-value")) unchanged_value();
    else if (!strcmp(argv[1], "old-clear-result")) old_clear_result();
    else assert(false);
    printf("PASS ADD transaction %s\n", argv[1]);
    return 0;
}
