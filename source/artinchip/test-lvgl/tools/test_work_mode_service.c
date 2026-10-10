/* Production service with deterministic UART/clock/storage boundaries. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/storage/work_mode_store.h"

const char *ui_tr(const char *msgid) { return msgid; }

static uint32_t fake_now;
static unsigned sent;
static uint8_t wire_sent[128], confirmed;
static bool tx_ok, io_ok, io_pending, io_load, complete_io;
static work_mode_record_t disk_record, queued_record;
uint32_t app_clock_uptime_ms(void) { return fake_now; }
int protocol_send(uint8_t command, const uint8_t *data, uint16_t length)
{
    assert(command == 0x38 && length == 1);
    if (!tx_ok) return -1;
    wire_sent[sent++] = data[0];
    return 0;
}
void machine_state_confirm_work_mode(uint8_t mode) { confirmed = mode; }
bool work_mode_store_begin_load(void)
{
    assert(!io_pending);
    io_pending = true; io_load = true;
    return true;
}
bool work_mode_store_begin_save(const work_mode_record_t *record)
{
    assert(!io_pending);
    io_pending = true; io_load = false; queued_record = *record;
    return true;
}
bool work_mode_store_busy(void) { return io_pending; }
bool work_mode_store_poll(work_mode_record_t *record, bool *load, bool *ok)
{
    if (!io_pending || !complete_io) return false;
    *load = io_load; *ok = io_ok;
    if (io_ok && !io_load) disk_record = queued_record;
    *record = io_load ? disk_record : queued_record;
    io_pending = false;
    return true;
}
/* Production request timing with one deterministic pending-RX probe. */
static bool queued_reply;
static uint8_t queued_command;
static uint64_t queued_received_ms;
uint64_t app_clock_monotonic_ms(void) { return fake_now; }
static bool pending_reply(uint8_t command, uint64_t started_ms, uint64_t deadline_ms)
{
    return queued_reply && queued_command == command &&
        queued_received_ms >= started_ms && queued_received_ms < deadline_ms;
}
#include "un260/protocol/protocol_request.c"
#include "un260/app_service/work_mode_service.c"

static void reset(work_mode_record_t disk)
{
    memset(&state, 0, sizeof(state));
    disk_record = disk;
    protocol_request_result_context_end();
    protocol_request_set_pending_reply_probe(pending_reply);
    queued_reply = false; queued_command = 0; queued_received_ms = 0;
    fake_now = 100; sent = 0; confirmed = 99;
    tx_ok = io_ok = complete_io = true; io_pending = false;
    work_mode_service_init();
}
static void poll(bool busy)
{
    ++fake_now;
    work_mode_service_poll(fake_now, busy);
}
static void flush(void) { for (int i = 0; i < 5; ++i) poll(false); }
static bool sync_mode(uint8_t mode)
{
    uint8_t frame[] = {0xFD,0xDF,7,0x38,2,mode == WORK_MODE_AUTO ? 1 : 0,0};
    return work_mode_service_handle_reply(frame, sizeof(frame));
}
static bool ack(uint8_t mode)
{
    uint8_t frame[] = {0xFD,0xDF,6,0x38,mode == WORK_MODE_AUTO ? 1 : 0,0};
    return work_mode_service_handle_reply(frame, sizeof(frame));
}
static bool ack_received(uint8_t mode, uint64_t received_ms)
{
    protocol_request_result_context_begin(received_ms);
    bool accepted = ack(mode);
    protocol_request_result_context_end();
    return accepted;
}
static void queue_echo(uint64_t received_ms)
{
    queued_reply = true; queued_command = 0x38; queued_received_ms = received_ms;
}
static void begin_auto_diagnostic(void)
{
    reset((work_mode_record_t){0});
    assert(sync_mode(WORK_MODE_AUTO)); flush();
    work_mode_service_set_diagnostic(true); flush();
    assert(sent == 1 && wire_sent[0] == 0 && disk_record.restore_pending);
    assert(!work_mode_service_diagnostic_ready());
    assert(ack(WORK_MODE_MANUAL)); poll(false);
    assert(work_mode_service_diagnostic_ready());
    assert(disk_record.preferred == WORK_MODE_AUTO);
}
static void test_confirmation_and_restore(void)
{
    begin_auto_diagnostic();
    assert(confirmed == WORK_MODE_MANUAL);
    work_mode_service_set_diagnostic(false);
    poll(true);
    assert(sent == 1 && state.phase == WORK_MODE_WAITING_IDLE);
    poll(false);
    assert(sent == 2 && wire_sent[1] == 1);
    assert(disk_record.restore_pending);
    assert(ack(WORK_MODE_AUTO)); flush();
    assert(confirmed == WORK_MODE_AUTO && !disk_record.restore_pending);
}
static void test_journal_precedes_command(void)
{
    reset((work_mode_record_t){0}); sync_mode(WORK_MODE_AUTO); flush();
    complete_io = false;
    work_mode_service_set_diagnostic(true); flush();
    assert(sent == 0 && !work_mode_service_diagnostic_ready());
    complete_io = true; poll(false);
    assert(sent == 1 && disk_record.restore_pending);
}
static void test_ack_matching_timeout_retry(void)
{
    reset((work_mode_record_t){0});sync_mode(WORK_MODE_AUTO);flush();
    work_mode_service_set_diagnostic(true);flush();
    work_mode_service_poll(fake_now-1,false);
    assert(state.failure==FAILURE_NONE&&state.request==REQUEST_TEMPORARY);
    fake_now+=1550;poll(false);assert(state.request==REQUEST_TEMPORARY);
    assert(ack(WORK_MODE_MANUAL));flush();assert(work_mode_service_diagnostic_ready());
    reset((work_mode_record_t){0});sync_mode(WORK_MODE_AUTO);flush();
    work_mode_service_set_diagnostic(true);flush();fake_now+=REQUEST_TIMEOUT_MS+1;
    assert(ack(WORK_MODE_MANUAL));flush();assert(work_mode_service_diagnostic_ready());
    reset((work_mode_record_t){0}); sync_mode(WORK_MODE_AUTO); flush();
    work_mode_service_set_diagnostic(true); flush();
    assert(!ack(WORK_MODE_AUTO) && state.request == REQUEST_TEMPORARY);
    fake_now += REQUEST_TIMEOUT_MS; poll(false);
    assert(state.failure == FAILURE_TIMEOUT && !state.actual_valid);
    assert(ack(WORK_MODE_MANUAL));poll(false);
    assert(work_mode_service_diagnostic_ready());
    reset((work_mode_record_t){0});sync_mode(WORK_MODE_AUTO);flush();
    work_mode_service_set_diagnostic(true);flush();
    fake_now+=REQUEST_TIMEOUT_MS;poll(false);
    work_mode_service_retry(); poll(false);
    assert(sent == 1); /* Late-reply quarantine still active. */
    fake_now += LATE_REPLY_GUARD_MS; poll(false);
    assert(sent == 2 && ack(WORK_MODE_MANUAL)); poll(false);
    assert(work_mode_service_diagnostic_ready());
    reset((work_mode_record_t){0});sync_mode(WORK_MODE_AUTO);flush();
    work_mode_service_set_diagnostic(true);flush();fake_now+=REQUEST_TIMEOUT_MS;poll(false);
    work_mode_service_set_diagnostic(false);work_mode_service_set_diagnostic(true);
    fake_now+=LATE_REPLY_GUARD_MS;flush();
    assert(sent==2&&ack(WORK_MODE_MANUAL));flush();assert(work_mode_service_diagnostic_ready());
}
static void test_rapid_navigation(void)
{
    reset((work_mode_record_t){0}); sync_mode(WORK_MODE_AUTO); flush();
    work_mode_service_set_diagnostic(true); flush();
    work_mode_service_set_diagnostic(false);
    work_mode_service_set_diagnostic(true);
    assert(ack(WORK_MODE_MANUAL)); flush();
    assert(sent == 1 && work_mode_service_diagnostic_ready());
    work_mode_service_set_diagnostic(false); poll(false);
    work_mode_service_set_diagnostic(true);
    assert(ack(WORK_MODE_AUTO)); flush();
    assert(sent == 3 && wire_sent[2] == 0 && disk_record.restore_pending);
    assert(ack(WORK_MODE_MANUAL)); flush();
    assert(work_mode_service_diagnostic_ready());
}
static void test_crash_recovery_and_normal_boot(void)
{
    begin_auto_diagnostic();
    work_mode_service_stop();
    reset(disk_record); flush();
    assert(sent == 0 && state.phase == WORK_MODE_WAITING_SYNC);
    assert(sync_mode(WORK_MODE_MANUAL)); poll(true);
    assert(sent == 0 && state.record.preferred == WORK_MODE_MANUAL);
    flush(); fake_now += REQUEST_TIMEOUT_MS + 1; flush();
    assert(sent == 0 && state.actual_valid && !work_mode_service_take_failure());
    assert(!disk_record.restore_pending && disk_record.preferred == WORK_MODE_MANUAL);
    /* The same boot frame may arrive before asynchronous storage completes. */
    reset((work_mode_record_t){true, WORK_MODE_AUTO, true});
    complete_io = false; assert(sync_mode(WORK_MODE_MANUAL)); poll(true);
    assert(sent == 0); complete_io = true; flush();
    fake_now += REQUEST_TIMEOUT_MS + 1; flush();
    assert(sent == 0 && !disk_record.restore_pending && !work_mode_service_take_failure());
    /* With no recovery journal, a controller update becomes the new preference. */
    assert(sync_mode(WORK_MODE_MANUAL)); flush();
    assert(disk_record.preferred == WORK_MODE_MANUAL);
    work_mode_service_set_diagnostic(true); flush();
    assert(sent == 0 && work_mode_service_diagnostic_ready());
    work_mode_service_set_diagnostic(false); flush();
    assert(sent == 0);
}
static void test_store_and_send_failures(void)
{
    reset((work_mode_record_t){0}); sync_mode(WORK_MODE_AUTO); flush();
    work_mode_service_set_diagnostic(true); poll(false);
    io_ok = false; poll(false);
    assert(state.failure == FAILURE_SAVE && sent == 0);
    io_ok = true; work_mode_service_retry(); flush();
    assert(sent == 1 && disk_record.restore_pending);
    work_mode_service_set_diagnostic(false);
    assert(ack(WORK_MODE_MANUAL));
    tx_ok = false; poll(false);
    assert(state.failure == FAILURE_SEND && disk_record.restore_pending);
    tx_ok = true; work_mode_service_retry(); poll(false);
    assert(sent == 2 && ack(WORK_MODE_AUTO)); flush();
    assert(!disk_record.restore_pending);
    reset((work_mode_record_t){0}); io_ok = false; poll(false);
    assert(state.failure == FAILURE_LOAD && sent == 0);
}
static void test_fresh_boot_sync(void)
{
    for (uint8_t mode = WORK_MODE_AUTO; mode <= WORK_MODE_MANUAL; ++mode) {
        for (unsigned load_first = 0; load_first < 2; ++load_first) {
            reset((work_mode_record_t){true, WORK_MODE_AUTO, true});
            if (load_first) flush();
            assert(sync_mode(mode)); poll(true); flush();
            fake_now += REQUEST_TIMEOUT_MS + 1; flush();
            assert(sent == 0 && state.actual_valid && confirmed == mode);
            assert(state.record.preferred == mode && !disk_record.restore_pending);
            assert(!work_mode_service_take_failure());
        }
    }
    /* A genuine operation timeout still raises an event. A later authoritative
     * boot synchronization resolves it without replaying the stale event. */
    reset((work_mode_record_t){0});sync_mode(WORK_MODE_MANUAL);flush();
    assert(work_mode_service_request(WORK_MODE_AUTO));
    fake_now += REQUEST_TIMEOUT_MS;poll(false);
    assert(state.failure == FAILURE_TIMEOUT && state.failure_event);
    assert(sync_mode(WORK_MODE_MANUAL));flush();
    assert(state.failure == FAILURE_NONE && !work_mode_service_take_failure());
    /* In-session recovery is not an old on-disk journal. */
    begin_auto_diagnostic();work_mode_service_set_diagnostic(false);
    work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN, true);
    assert(sync_mode(WORK_MODE_MANUAL));flush();
    assert(state.record.preferred == WORK_MODE_AUTO && disk_record.restore_pending);
    work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN, false);flush();
    assert(sent == 2 && ack(WORK_MODE_AUTO));flush();
    assert(!disk_record.restore_pending);
}
static void test_hardware_holds_and_user_preference(void)
{
    begin_auto_diagnostic();
    work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN, true);
    work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_REJECT, true);
    work_mode_service_set_diagnostic(false); flush();
    assert(sent == 1 && state.phase == WORK_MODE_WAITING_IDLE);
    assert(!work_mode_service_request(WORK_MODE_AUTO));
    work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN, false); flush();
    assert(sent == 1);
    work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_REJECT, false); poll(false);
    assert(sent == 2 && ack(WORK_MODE_AUTO)); flush();
    assert(work_mode_service_request(WORK_MODE_MANUAL));
    assert(!ack(WORK_MODE_AUTO));
    assert(disk_record.preferred == WORK_MODE_AUTO);
    assert(ack(WORK_MODE_MANUAL)); flush();
    assert(disk_record.preferred == WORK_MODE_MANUAL);
}
static void test_controller_resync(void)
{
    reset((work_mode_record_t){0}); sync_mode(WORK_MODE_AUTO); flush();
    work_mode_service_set_diagnostic(true); flush();
    assert(sync_mode(WORK_MODE_AUTO)); flush();
    assert(!ack(WORK_MODE_MANUAL) && sent == 1);
    fake_now += LATE_REPLY_GUARD_MS; poll(false);
    assert(sent == 2 && ack(WORK_MODE_MANUAL)); flush();
    assert(work_mode_service_diagnostic_ready() && state.record.preferred == WORK_MODE_AUTO);
    uint8_t short_frame[1] = {0};
    assert(!work_mode_service_handle_reply(short_frame, 1));
    uint8_t invalid[] = {0,0,7,0x38,2,9,0};
    assert(!work_mode_service_handle_reply(invalid, 7));
}
static void test_menu_recovery_after_restore_timeout(void)
{
    for (uint8_t chosen = WORK_MODE_AUTO; chosen <= WORK_MODE_MANUAL; ++chosen) {
        begin_auto_diagnostic();
        work_mode_service_set_diagnostic(false); poll(false);
        assert(sent == 2 && state.request == REQUEST_RESTORE);
        fake_now += REQUEST_TIMEOUT_MS; poll(false);
        assert(state.failure == FAILURE_TIMEOUT && !state.actual_valid);
        assert(disk_record.restore_pending && disk_record.preferred == WORK_MODE_AUTO);
        assert(strstr(work_mode_service_status_text(), "Menu"));
        assert(!work_mode_service_request(chosen)); /* Late ACK quarantine. */
        assert(!ack(WORK_MODE_AUTO));
        fake_now += LATE_REPLY_GUARD_MS;
        work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN, true);
        assert(!work_mode_service_request(chosen));
        work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN, false);
        poll(true); assert(!work_mode_service_request(chosen)); poll(false);
        assert(sent == 2); /* Failure never retries itself. */
        assert(work_mode_service_request(chosen) && sent == 3);
        assert(disk_record.restore_pending && disk_record.preferred == WORK_MODE_AUTO);
        assert(!work_mode_service_request(chosen)); /* No duplicate in flight. */
        assert(ack(chosen)); flush();
        assert(state.failure == FAILURE_NONE && state.actual_valid);
        assert(!disk_record.restore_pending && disk_record.preferred == chosen);
    }
    reset((work_mode_record_t){0}); io_ok=false; poll(false);
    assert(!work_mode_service_request(WORK_MODE_AUTO)); /* Cannot bypass failed load. */
    begin_auto_diagnostic(); work_mode_service_set_diagnostic(false); poll(false);
    assert(ack(WORK_MODE_AUTO)); io_ok=false; poll(false); poll(false);
    assert(state.failure == FAILURE_SAVE);
    assert(!work_mode_service_request(WORK_MODE_AUTO)); /* Cannot bypass failed save. */
}
static void test_user_confirmation_event(void)
{
    reset((work_mode_record_t){0}); sync_mode(WORK_MODE_AUTO); flush();
    assert(!work_mode_service_take_user_confirmation());
    assert(work_mode_service_request(WORK_MODE_MANUAL));
    assert(!work_mode_service_take_user_confirmation());
    assert(!ack(WORK_MODE_AUTO));
    assert(!work_mode_service_take_user_confirmation());
    assert(ack(WORK_MODE_MANUAL));
    assert(work_mode_service_take_user_confirmation());
    assert(!work_mode_service_take_user_confirmation());
    assert(!ack(WORK_MODE_MANUAL));
    assert(!work_mode_service_take_user_confirmation());
    flush();
    assert(work_mode_service_request(WORK_MODE_AUTO));
    fake_now += REQUEST_TIMEOUT_MS; poll(false);
    assert(!work_mode_service_take_user_confirmation());
    assert(!ack(WORK_MODE_AUTO));
    assert(!work_mode_service_take_user_confirmation());
    begin_auto_diagnostic();
    assert(!work_mode_service_take_user_confirmation());
    work_mode_service_set_diagnostic(false); poll(false);
    assert(ack(WORK_MODE_AUTO)); flush();
    assert(!work_mode_service_take_user_confirmation());
    puts("PASS user confirmation: matching ACK once; no sync, mismatch, duplicate, timeout, diagnostic or restore success");
}
static void test_reset_owns_mode_recovery(void)
{
    reset((work_mode_record_t){0});sync_mode(WORK_MODE_AUTO);flush();
    work_mode_service_set_diagnostic(true);flush();assert(ack(WORK_MODE_MANUAL));flush();
    assert(!work_mode_service_operation_active());
    work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN,true);
    assert(work_mode_service_operation_active());
    work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN,false);
    work_mode_service_set_diagnostic(false);
    work_mode_service_set_reset_pending(true);
    unsigned before=sent;flush();assert(sent==before);
    assert(!work_mode_service_request(WORK_MODE_AUTO));
    work_mode_service_set_reset_pending(false);flush();assert(sent==before+1);
    assert(ack(WORK_MODE_AUTO));flush();
}
static void test_uart_arrival_deadline(void)
{
    /* A USER echo is already in RX while UI work holds its dispatch. */
    reset((work_mode_record_t){0}); sync_mode(WORK_MODE_AUTO); flush();
    assert(work_mode_service_request(WORK_MODE_MANUAL));
    uint64_t started = state.lifecycle.started_ms;
    queue_echo(started + 20);
    fake_now += REQUEST_TIMEOUT_MS + 200; poll(false);
    assert(state.request == REQUEST_USER && state.failure == FAILURE_NONE);
    queued_reply = false;
    assert(ack_received(WORK_MODE_MANUAL, started + 20)); flush();
    assert(confirmed == WORK_MODE_MANUAL && work_mode_service_take_user_confirmation());

    /* Temporary MANUAL also waits for an echo that has already arrived. */
    reset((work_mode_record_t){0}); sync_mode(WORK_MODE_AUTO); flush();
    work_mode_service_set_diagnostic(true); flush();
    started = state.lifecycle.started_ms;
    queue_echo(started + 20);
    fake_now += REQUEST_TIMEOUT_MS + 200; poll(false);
    assert(state.request == REQUEST_TEMPORARY && state.failure == FAILURE_NONE);
    queued_reply = false;
    assert(ack_received(WORK_MODE_MANUAL, started + 20)); flush();
    assert(work_mode_service_diagnostic_ready());

    /* Leaving diagnostics must restore AUTO even if a timely echo is dispatched late. */
    begin_auto_diagnostic(); work_mode_service_set_diagnostic(false); poll(false);
    assert(state.request == REQUEST_RESTORE && sent == 2);
    started = state.lifecycle.started_ms;
    queue_echo(started + REQUEST_TIMEOUT_MS - 1);
    fake_now += REQUEST_TIMEOUT_MS + 200; poll(false);
    assert(state.request == REQUEST_RESTORE && state.failure == FAILURE_NONE);
    assert(disk_record.restore_pending && confirmed == WORK_MODE_MANUAL);
    queued_reply = false;
    assert(ack_received(WORK_MODE_AUTO, started + REQUEST_TIMEOUT_MS - 1)); flush();
    assert(confirmed == WORK_MODE_AUTO && !disk_record.restore_pending);
    assert(!work_mode_service_take_user_confirmation());

    /* A truly late RESTORE echo keeps the prior explicit-retry policy. */
    begin_auto_diagnostic(); work_mode_service_set_diagnostic(false); poll(false);
    started = state.lifecycle.started_ms;
    queue_echo(started + REQUEST_TIMEOUT_MS);
    fake_now += REQUEST_TIMEOUT_MS + 20; poll(false);
    assert(state.failure == FAILURE_TIMEOUT && state.request == REQUEST_NONE);
    queued_reply = false;
    assert(!ack_received(WORK_MODE_AUTO, started + REQUEST_TIMEOUT_MS));
    assert(disk_record.restore_pending);

    /* A matching pre-request frame cannot postpone timeout or acknowledge a retry. */
    reset((work_mode_record_t){0}); sync_mode(WORK_MODE_AUTO); flush();
    assert(work_mode_service_request(WORK_MODE_MANUAL));
    started = state.lifecycle.started_ms;
    assert(!ack_received(WORK_MODE_MANUAL, started - 1));
    assert(state.request == REQUEST_USER && confirmed == WORK_MODE_AUTO);
    queue_echo(started - 1);
    fake_now += REQUEST_TIMEOUT_MS; poll(false);
    assert(state.failure == FAILURE_TIMEOUT);
    queued_reply = false;
    fake_now += LATE_REPLY_GUARD_MS; poll(false);
    assert(work_mode_service_request(WORK_MODE_MANUAL));
    assert(!ack_received(WORK_MODE_MANUAL, started + 20));
    assert(state.request == REQUEST_USER && ack(WORK_MODE_MANUAL)); flush();

    /* MANUAL retains late recovery, but even its recovery rejects an older echo. */
    reset((work_mode_record_t){0}); sync_mode(WORK_MODE_AUTO); flush();
    work_mode_service_set_diagnostic(true); flush();
    started = state.lifecycle.started_ms;
    assert(!ack_received(WORK_MODE_MANUAL, started - 1));
    fake_now += REQUEST_TIMEOUT_MS; poll(false);
    assert(state.failure == FAILURE_TIMEOUT && state.late_request);
    assert(!ack_received(WORK_MODE_MANUAL, started - 1));
    assert(ack_received(WORK_MODE_MANUAL, started + REQUEST_TIMEOUT_MS + 20)); flush();
    assert(work_mode_service_diagnostic_ready());
    puts("PASS mode RX timing: timely queued USER/RESTORE, late MANUAL recovery, true timeout and stale echoes");
}
static void begin_manual_auto_lease(void)
{
    reset((work_mode_record_t){0});assert(sync_mode(WORK_MODE_MANUAL));flush();
    work_mode_service_set_diagnostic_mode(true,WORK_MODE_AUTO);flush();
    assert(sent==1&&wire_sent[0]==1&&disk_record.restore_pending);
    assert(disk_record.preferred==WORK_MODE_MANUAL&&!work_mode_service_diagnostic_ready());
}
static void test_automatic_diagnostic_requirement(void)
{
    begin_manual_auto_lease();assert(!ack(WORK_MODE_MANUAL));
    assert(ack(WORK_MODE_AUTO));flush();assert(work_mode_service_diagnostic_ready());
    assert(confirmed==WORK_MODE_AUTO&&disk_record.preferred==WORK_MODE_MANUAL);
    assert(!work_mode_service_take_user_confirmation());
    work_mode_service_set_diagnostic(false);poll(true);
    assert(sent==1&&state.phase==WORK_MODE_WAITING_IDLE);poll(false);
    assert(sent==2&&wire_sent[1]==0&&state.request==REQUEST_RESTORE);
    assert(ack(WORK_MODE_MANUAL));flush();
    assert(confirmed==WORK_MODE_MANUAL&&!disk_record.restore_pending&&disk_record.preferred==WORK_MODE_MANUAL);

    reset((work_mode_record_t){0});assert(sync_mode(WORK_MODE_AUTO));flush();
    work_mode_service_set_diagnostic_mode(true,WORK_MODE_AUTO);flush();
    assert(sent==0&&work_mode_service_diagnostic_ready()&&!disk_record.restore_pending);
    work_mode_service_set_diagnostic(false);flush();
    assert(sent==0&&disk_record.preferred==WORK_MODE_AUTO&&!disk_record.restore_pending);

    reset((work_mode_record_t){0});assert(sync_mode(WORK_MODE_MANUAL));flush();
    complete_io=false;work_mode_service_set_diagnostic_mode(true,WORK_MODE_AUTO);flush();
    assert(sent==0&&state.phase==WORK_MODE_SAVING);complete_io=true;poll(false);
    assert(sent==1&&disk_record.restore_pending&&ack(WORK_MODE_AUTO));flush();
    assert(work_mode_service_diagnostic_ready());
}
static void test_automatic_lease_navigation_convergence(void)
{
    begin_manual_auto_lease();work_mode_service_set_diagnostic(false);
    assert(ack(WORK_MODE_AUTO));flush();
    assert(sent==2&&wire_sent[1]==0&&ack(WORK_MODE_MANUAL));flush();
    assert(!disk_record.restore_pending);

    begin_manual_auto_lease();work_mode_service_set_diagnostic(true);
    assert(ack(WORK_MODE_AUTO));flush();
    assert(sent==2&&wire_sent[1]==0&&state.request==REQUEST_TEMPORARY);
    assert(ack(WORK_MODE_MANUAL));flush();assert(work_mode_service_diagnostic_ready());
    work_mode_service_set_diagnostic(false);flush();assert(sent==2&&!disk_record.restore_pending);

    begin_manual_auto_lease();assert(ack(WORK_MODE_AUTO));flush();
    work_mode_service_set_diagnostic(false);poll(false);
    work_mode_service_set_diagnostic_mode(true,WORK_MODE_AUTO);
    assert(ack(WORK_MODE_MANUAL));flush();assert(sent==3&&wire_sent[2]==1);
    assert(ack(WORK_MODE_AUTO));flush();assert(work_mode_service_diagnostic_ready());
    work_mode_service_set_diagnostic(false);flush();assert(sent==4&&ack(WORK_MODE_MANUAL));flush();
    assert(!disk_record.restore_pending&&disk_record.preferred==WORK_MODE_MANUAL);
}
static void test_automatic_lease_failures_and_arrival(void)
{
    begin_manual_auto_lease();uint64_t started=state.lifecycle.started_ms;
    queue_echo(started+20);fake_now=(uint32_t)started+REQUEST_TIMEOUT_MS+1;poll(false);
    assert(state.request==REQUEST_TEMPORARY&&state.failure==FAILURE_NONE);
    queued_reply=false;assert(!ack_received(WORK_MODE_AUTO,started-1));
    assert(ack_received(WORK_MODE_AUTO,started+20));flush();assert(work_mode_service_diagnostic_ready());

    begin_manual_auto_lease();fake_now+=REQUEST_TIMEOUT_MS;poll(false);
    assert(state.failure==FAILURE_TIMEOUT&&!work_mode_service_diagnostic_ready());
    assert(ack(WORK_MODE_AUTO));flush();assert(work_mode_service_diagnostic_ready());
    work_mode_service_set_diagnostic(false);fake_now+=LATE_REPLY_GUARD_MS;
    flush();assert(sent==2&&ack(WORK_MODE_MANUAL));flush();

    begin_manual_auto_lease();work_mode_service_set_diagnostic(false);
    fake_now+=REQUEST_TIMEOUT_MS;poll(false);
    assert(sent==1&&state.guard_active&&work_mode_service_take_failure());
    fake_now+=LATE_REPLY_GUARD_MS;flush();
    assert(sent==2&&wire_sent[1]==0&&state.request==REQUEST_RESTORE);
    assert(ack(WORK_MODE_MANUAL));flush();assert(!disk_record.restore_pending);

    reset((work_mode_record_t){0});assert(sync_mode(WORK_MODE_AUTO));flush();
    work_mode_service_set_diagnostic(true);flush();assert(sent==1&&wire_sent[0]==0);
    work_mode_service_set_diagnostic(false);fake_now+=REQUEST_TIMEOUT_MS;poll(false);
    assert(sent==1&&work_mode_service_take_failure());fake_now+=LATE_REPLY_GUARD_MS;flush();
    assert(sent==2&&wire_sent[1]==1&&ack(WORK_MODE_AUTO));flush();
    assert(!disk_record.restore_pending&&disk_record.preferred==WORK_MODE_AUTO);

    begin_manual_auto_lease();work_mode_service_set_diagnostic(true);
    fake_now+=REQUEST_TIMEOUT_MS;poll(false);assert(sent==1&&work_mode_service_take_failure());
    fake_now+=LATE_REPLY_GUARD_MS;flush();assert(sent==2&&wire_sent[1]==0);
    assert(ack(WORK_MODE_MANUAL));flush();assert(work_mode_service_diagnostic_ready());

    reset((work_mode_record_t){0});assert(sync_mode(WORK_MODE_MANUAL));flush();tx_ok=false;
    work_mode_service_set_diagnostic_mode(true,WORK_MODE_AUTO);flush();
    assert(sent==0&&state.failure==FAILURE_SEND&&disk_record.restore_pending);
    tx_ok=true;work_mode_service_retry();flush();assert(sent==1&&wire_sent[0]==1);
    assert(ack(WORK_MODE_AUTO));flush();assert(work_mode_service_diagnostic_ready());
    assert(disk_record.preferred==WORK_MODE_MANUAL);
}
int main(void)
{
    test_automatic_diagnostic_requirement();
    test_automatic_lease_navigation_convergence();
    test_automatic_lease_failures_and_arrival();
    test_uart_arrival_deadline();
    test_reset_owns_mode_recovery();
    test_user_confirmation_event();
    test_confirmation_and_restore();
    test_journal_precedes_command();
    test_ack_matching_timeout_retry();
    test_rapid_navigation();
    test_crash_recovery_and_normal_boot();
    test_fresh_boot_sync();
    test_store_and_send_failures();
    test_hardware_holds_and_user_preference();
    test_controller_resync();
    test_menu_recovery_after_restore_timeout();
    puts("work_mode_service: confirmation, journal, recovery, holds, timeout and navigation PASS");
    return 0;
}
