/* Production service with deterministic UART/clock/storage boundaries. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/storage/work_mode_store.h"

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
#include "un260/app_service/work_mode_service.c"

static void reset(work_mode_record_t disk)
{
    memset(&state, 0, sizeof(state));
    disk_record = disk;
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
    reset((work_mode_record_t){0}); sync_mode(WORK_MODE_AUTO); flush();
    work_mode_service_set_diagnostic(true); flush();
    assert(!ack(WORK_MODE_AUTO) && state.request == REQUEST_MANUAL);
    fake_now += REQUEST_TIMEOUT_MS; poll(false);
    assert(state.failure == FAILURE_TIMEOUT && !state.actual_valid);
    assert(!ack(WORK_MODE_MANUAL));
    work_mode_service_retry(); poll(false);
    assert(sent == 1); /* Late-reply quarantine still active. */
    fake_now += LATE_REPLY_GUARD_MS; poll(false);
    assert(sent == 2 && ack(WORK_MODE_MANUAL)); poll(false);
    assert(work_mode_service_diagnostic_ready());
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
    assert(sent == 0 && state.record.preferred == WORK_MODE_AUTO);
    poll(false); assert(sent == 1 && wire_sent[0] == 1);
    assert(ack(WORK_MODE_AUTO)); flush();
    assert(!disk_record.restore_pending);
    /* With no recovery journal, a controller update becomes the new preference. */
    assert(sync_mode(WORK_MODE_MANUAL)); flush();
    assert(disk_record.preferred == WORK_MODE_MANUAL);
    work_mode_service_set_diagnostic(true); flush();
    assert(sent == 1 && work_mode_service_diagnostic_ready());
    work_mode_service_set_diagnostic(false); flush();
    assert(sent == 1);
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
int main(void)
{
    test_confirmation_and_restore();
    test_journal_precedes_command();
    test_ack_matching_timeout_retry();
    test_rapid_navigation();
    test_crash_recovery_and_normal_boot();
    test_store_and_send_failures();
    test_hardware_holds_and_user_preference();
    test_controller_resync();
    test_menu_recovery_after_restore_timeout();
    puts("work_mode_service: confirmation, journal, recovery, holds, timeout and navigation PASS");
    return 0;
}
