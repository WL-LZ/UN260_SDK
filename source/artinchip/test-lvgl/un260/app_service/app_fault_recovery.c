#include "app_fault_recovery.h"
#include "app_command_runtime.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_system/app_clock.h"
#include "un260/protocol/protocol_send.h"
#include "un260/protocol/protocol_request.h"
#include "un260/counting/counting_action_service.h"
#include "un260/currency/currency_service.h"
#include <string.h>

#define BATCH_RECOVERY_RETRY_MS 3000U

/* 0x3D/01 has no fault id. Keep at most one command awaiting a result so a
 * response cannot resolve a later, unrelated report. Both UI Confirm and a
 * completed island animation use this sender. Batch-only retries stay here. */
static struct {
    machine_fault_key_t key, pending_key, waiting_key;
    bool valid, needs_clear, pending, auto_popup, pending_auto_popup;
    bool pending_superseded, waiting_pocket, waiting_auto_popup, notice_end_pending;
    bool batch_retry_armed;
    uint32_t sent_tick, batch_attempt_tick;
} recovery;

static struct {
    uint8_t command;
    uint64_t received_ms;
} setting_rejection;
static uint8_t deferred_setting_rejection;

void app_fault_recovery_cancel_setting_detail(void)
{
    memset(&setting_rejection, 0, sizeof(setting_rejection));
}

static bool setting_command(uint8_t command)
{
    return command == 0x03 || command == 0x04 ||
           command == 0x06 || command == 0x3A;
}

void app_fault_recovery_cancel_setting_presentation(void)
{
    app_fault_recovery_cancel_setting_detail();
    deferred_setting_rejection = 0;
    fault_popup_resolve_setting_rejection(0);
}

void app_fault_recovery_resolve_setting_rejection(uint8_t command)
{
    if (!setting_command(command)) return;
    if (deferred_setting_rejection == command) deferred_setting_rejection = 0;
    if (setting_rejection.command == command) app_fault_recovery_cancel_setting_detail();
    fault_popup_resolve_setting_rejection(command);
}

static void setting_request_started(uint8_t command)
{
    app_fault_recovery_cancel_setting_detail();
    if (setting_command(command) || command == 0x0A)
        app_fault_recovery_cancel_setting_presentation();
}

bool app_fault_recovery_report_setting_rejection(uint8_t command)
{
    if (!setting_command(command)) return false;
    setting_rejection.command = command;
    setting_rejection.received_ms = protocol_request_result_time_ms();
    /* These four matched controller refusal ACKs mean notes block switching
     * on this machine. Do not apply that contract to timeouts or other ACKs. */
    if (app_command_runtime_count_start_busy())
        deferred_setting_rejection = command;
    else
        fault_popup_report_setting_rejection(command, MACHINE_FAULT_SETTING_NOTES);
    return true;
}

bool app_fault_recovery_observe_setting_detail(uint8_t command,
    const uint8_t *frame, uint16_t length, const counting_session_state_t *session)
{
    uint8_t code = frame && length >= 6 ? frame[4] : 0;
    bool currency_detail = currency_service_take_switch_rejection_detail(command, code);
    uint8_t operation = setting_rejection.command;
    uint64_t rejected_ms = setting_rejection.received_ms;
    memset(&setting_rejection, 0, sizeof(setting_rejection));
    if ((operation == 0x03 && !currency_detail) || !operation || command != 0x0F || !frame || length != 6 ||
        frame[2] != length || frame[3] != command || !session ||
        session->start_confirmed || session->phase == COUNTING_SESSION_ACTIVE ||
        counting_action_start_pending()) return false;
    uint64_t received_ms = protocol_request_result_time_ms();
    if (received_ms < rejected_ms || received_ms - rejected_ms >= 800U) return false;
    /* 0F/07 is the documented stacker-note report. The observed older
     * controller currency refusal uses 03/02 + immediate 0F/02 instead.
     * Never infer that overloaded meaning for other commands or later jams. */
    uint8_t reason = frame[4];
    if (reason != 7 && !(operation == 0x03 && reason == 2)) return false;
    machine_fault_record_t record;
    for (size_t i = 0; machine_fault_at(i, &record); ++i)
        if (record.key.source != MACHINE_FAULT_SETTING) return false;
    /* The negative ACK already published the operation guide. Its companion
     * is the same event, so do not restart the modal/island timeline. */
    return true;
}

static bool pocket_full(machine_fault_key_t key)
{
    return key.source == MACHINE_FAULT_START && key.type == 2 &&
        (key.code == 5 || key.code == 7 || key.code == 8);
}

static bool batch_full(machine_fault_key_t key)
{
    return key.source == MACHINE_FAULT_PRESET && key.type == 0 && key.code == 4;
}

/* Full pockets need the established controller latch handshake in either
 * Popup mode. Other START reports remain presentation acknowledgements. */
static bool clearable(machine_fault_key_t key)
{
    return pocket_full(key) ||
           (key.source == MACHINE_FAULT_RUNTIME && key.type == 0 &&
            key.code >= 1 && key.code <= 5) ||
           batch_full(key);
}

static bool send_clear(machine_fault_key_t key, bool auto_popup)
{
    const uint8_t payload = 0x01;
    if (batch_full(key)) recovery.batch_attempt_tick = app_clock_uptime_ms();
    if (protocol_send(0x3D, &payload, 1) < 0) return false;
    recovery.pending_key = key;
    recovery.pending_auto_popup = auto_popup;
    recovery.pending = true;
    recovery.pending_superseded = false;
    recovery.sent_tick = app_clock_uptime_ms();
    return true;
}

bool app_fault_recovery_request_clear(machine_fault_key_t key)
{
    if (!clearable(key)) return true;
    /* Batch can be reported again instead of a clear ACK while notes remain.
     * END/Confirm authorizes its bounded retries, never CLEAR or START. */
    if (batch_full(key)) recovery.batch_retry_armed = true;
    /* The reply carries no fault identity. A different fault must wait for
     * the previous reply or timeout; Confirm then stays open for retry. */
    bool auto_popup = recovery.valid && machine_fault_key_equal(recovery.key,key)
        ? recovery.auto_popup : fault_popup_get_auto_enabled();
    if (recovery.pending) {
        bool same_key = machine_fault_key_equal(recovery.pending_key, key);
        if (pocket_full(key) && (!same_key || recovery.pending_superseded)) {
            recovery.waiting_key = key;
            recovery.waiting_auto_popup = auto_popup;
            recovery.waiting_pocket = true;
        }
        return same_key;
    }
    return send_clear(key, auto_popup);
}

static void notice_phase(machine_fault_key_t key, smart_island_fault_phase_t phase)
{
    if (key.source == MACHINE_FAULT_SETTING) {
        if (phase == SMART_ISLAND_FAULT_END || phase == SMART_ISLAND_FAULT_CANCEL)
            fault_popup_setting_notice_finished(key);
        return;
    }
    machine_fault_record_t record;
    if (!recovery.valid ||
        !machine_fault_key_equal(key, recovery.key) || !clearable(key) ||
        !machine_fault_find(key, &record) || phase != SMART_ISLAND_FAULT_END ||
        !recovery.notice_end_pending) return;
    if (!pocket_full(key) && (recovery.auto_popup || fault_popup_is_showing())) return;
    /* Consume this report's END opportunity before a failed send can replay
     * its guide. Confirmed reports cannot authorize another automatic send. */
    recovery.notice_end_pending = false;
    if (record.acknowledged) return;
    if (!app_fault_recovery_request_clear(key))
        fault_popup_repeat_notice(key);
}

void app_fault_recovery_init(void)
{
    app_fault_recovery_clear();
    protocol_request_set_start_observer(setting_request_started);
    smart_island_register_fault_phase_cb(notice_phase);
}

void app_fault_recovery_clear(void)
{
    memset(&recovery, 0, sizeof(recovery));
    app_fault_recovery_cancel_setting_presentation();
}

void app_fault_recovery_stacker_cleared(void)
{
    app_fault_recovery_cancel_setting_presentation();
    const machine_fault_key_t batch = {MACHINE_FAULT_PRESET, 0, 4};
    recovery.batch_retry_armed = false;
    if (recovery.valid && batch_full(recovery.key)) {
        recovery.valid = false;
        recovery.needs_clear = false;
        recovery.notice_end_pending = false;
    }
    if (machine_fault_find(batch, NULL)) fault_popup_resolve_key(batch);
    if (recovery.valid && recovery.key.source == MACHINE_FAULT_START &&
        recovery.key.type == 2 && recovery.key.code == 7)
        recovery.valid = false;
    if (recovery.waiting_pocket && recovery.waiting_key.code == 7)
        recovery.waiting_pocket = false;
    fault_popup_stacker_cleared();
}

void app_fault_recovery_report(machine_fault_key_t key)
{
    if (key.source == MACHINE_FAULT_BOOT || key.source == MACHINE_FAULT_SENSOR) {
        /* These reports enter outside counting recovery. A new physical
         * guide must stop only Batch's retries and its older failure reply. */
        recovery.batch_retry_armed = false;
        if (recovery.pending && batch_full(recovery.pending_key))
            recovery.pending_superseded = true;
        if (recovery.valid && batch_full(recovery.key)) {
            recovery.valid = false;
            recovery.needs_clear = false;
            recovery.notice_end_pending = false;
        }
        return;
    }
    bool known = (key.source == MACHINE_FAULT_START &&
                  (key.type == 1 || key.type == 2)) ||
        (key.source == MACHINE_FAULT_RUNTIME && key.code >= 1 && key.code <= 7) ||
        key.source == MACHINE_FAULT_PRESET;
    if (known || (key.source == MACHINE_FAULT_RUNTIME && !key.code))
        app_fault_recovery_cancel_setting_presentation();
    if (!batch_full(key)) recovery.batch_retry_armed = false;
    if (!known) { recovery.valid = false; return; }
    bool new_report = !recovery.valid || !machine_fault_key_equal(recovery.key,key) ||
        key.source == MACHINE_FAULT_START || batch_full(key) || !machine_fault_find(key,NULL);
    recovery.key = key;
    recovery.valid = true;
    recovery.needs_clear = clearable(key);
    /* A preference change applies to the next controller report, not to the
     * animation/clear request already handling this report. */
    if (new_report) {
        recovery.auto_popup = fault_popup_get_auto_enabled();
        recovery.notice_end_pending = clearable(key);
        if (recovery.pending && machine_fault_key_equal(recovery.pending_key, key))
            recovery.pending_superseded = true;
    }
    /* Request the full-pocket latch handshake even when Main/the island is
     * hidden. END can repeat it after its reply. Never send CLEAR or START. */
    if (pocket_full(key)) (void)app_fault_recovery_request_clear(key);
}

static void present_unresolved(machine_fault_key_t key,bool auto_popup)
{
    machine_fault_record_t record;
    if (!machine_fault_find(key, &record) || (record.acknowledged && !batch_full(key))) return;
    /* Independent sources may coexist. An older clear failure cannot replace
     * the guide belonging to a newer active controller report. */
    if (recovery.valid && !machine_fault_key_equal(recovery.key, key) &&
        machine_fault_find(recovery.key, NULL)) return;
    if (auto_popup)
        (void)fault_popup_show_key(key);
    else
        fault_popup_repeat_notice(key);
}

static void request_waiting_pocket(void)
{
    if (recovery.pending || !recovery.waiting_pocket) return;
    machine_fault_key_t key = recovery.waiting_key;
    bool auto_popup = recovery.waiting_auto_popup;
    recovery.waiting_pocket = false;
    if (machine_fault_find(key, NULL) && !send_clear(key, auto_popup))
        present_unresolved(key, auto_popup);
}

void app_fault_recovery_handle_clear_result(uint8_t result)
{
    if (!recovery.pending || (result != 0x01 && result != 0x02)) return;
    machine_fault_key_t key = recovery.pending_key;
    bool superseded = recovery.pending_superseded;
    recovery.pending = false;
    if (result == 0x01) {
        /* A newer controller report owns its own recovery. An old successful
         * reply must not erase that report, even if the fault key repeats. */
        if (!superseded) {
            if (recovery.valid && machine_fault_key_equal(recovery.key, key))
                recovery.needs_clear = false;
            /* A latch ACK does not prove a full pocket or Batch was taken.
             * Batch waits for the stacker notification or a real START. */
            if (!pocket_full(key) && !batch_full(key)) {
                if (recovery.valid && machine_fault_key_equal(recovery.key, key))
                    recovery.valid = false;
                if (clearable(key)) fault_popup_resolve_key(key);
            }
        }
    } else if (!superseded) {
        present_unresolved(key,recovery.pending_auto_popup);
    }
    request_waiting_pocket();
}

static void present_deferred_setting_rejection(void)
{
    if (deferred_setting_rejection && !app_command_runtime_count_start_busy()) {
        uint8_t command = deferred_setting_rejection;
        deferred_setting_rejection = 0;
        fault_popup_report_setting_rejection(command, MACHINE_FAULT_SETTING_NOTES);
    }
}

void app_fault_recovery_poll(void)
{
    present_deferred_setting_rejection();
    uint32_t now = app_clock_uptime_ms();
    if (recovery.pending && (uint32_t)(now - recovery.sent_tick) >= 3000U) {
        /* Missing ACKs do not prove recovery or authorize another guide. */
        recovery.pending = false;
        request_waiting_pocket();
    }
    if (!recovery.batch_retry_armed) return;
    if (!recovery.valid || !batch_full(recovery.key) ||
        !machine_fault_find(recovery.key, NULL)) {
        recovery.batch_retry_armed = false;
        return;
    }
    /* Only a Batch END/Confirm authorizes this path. Preserve the shared
     * in-flight transaction and throttle failed sends as well as missing ACKs. */
    if (!recovery.pending &&
        (uint32_t)(now - recovery.batch_attempt_tick) >= BATCH_RECOVERY_RETRY_MS)
        (void)send_clear(recovery.key, recovery.auto_popup);
}

bool app_fault_recovery_prepare_start(void)
{
    if (fault_popup_is_showing()) return false;
    return !recovery.valid || !recovery.needs_clear ||
        app_fault_recovery_request_clear(recovery.key);
}
