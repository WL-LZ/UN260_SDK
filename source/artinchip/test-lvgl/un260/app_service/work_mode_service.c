#include "work_mode_service.h"

#include <stddef.h>
#include "un260/lv_system/app_clock.h"
#include "un260/machine_state/machine_state.h"
#include "un260/protocol/protocol_send.h"
#include "un260/storage/work_mode_store.h"

#define REQUEST_TIMEOUT_MS 800U
#define LATE_REPLY_GUARD_MS 800U

typedef enum { REQUEST_NONE, REQUEST_USER, REQUEST_MANUAL, REQUEST_RESTORE } request_kind_t;
typedef enum { FAILURE_NONE, FAILURE_LOAD, FAILURE_SAVE, FAILURE_SEND, FAILURE_REPLY, FAILURE_TIMEOUT } failure_t;
static struct {
    bool initialized, stopped, loaded, diagnostic, actual_valid, busy;
    bool deferred_sync, failure_event, controller_seen, retry_manual;
    uint8_t actual, sync_mode, target;
    uint32_t operations, request_tick, guard_tick;
    bool guard_active;
    request_kind_t request;
    failure_t failure;
    work_mode_phase_t phase;
    work_mode_record_t record, durable;
} state;

static bool records_equal(const work_mode_record_t *a, const work_mode_record_t *b)
{
    return a->preferred_valid == b->preferred_valid &&
           a->preferred == b->preferred && a->restore_pending == b->restore_pending;
}

static void fail(failure_t reason)
{
    state.failure = reason;
    state.failure_event = true;
    state.phase = WORK_MODE_FAILED;
    state.retry_manual = false;
}

static bool guard_active(uint32_t now)
{
    if (state.guard_active && (uint32_t)(now - state.guard_tick) >= LATE_REPLY_GUARD_MS)
        state.guard_active = false;
    return state.guard_active;
}

static bool send_mode(uint8_t target, request_kind_t kind, uint32_t now)
{
    if (state.request != REQUEST_NONE || guard_active(now)) return false;
    const uint8_t wire = target == WORK_MODE_MANUAL ? 0x00 : 0x01;
    if (protocol_send(0x38, &wire, 1) < 0) {
        fail(FAILURE_SEND);
        return false;
    }
    state.target = target;
    state.request = kind;
    state.retry_manual = false;
    state.request_tick = now;
    state.phase = kind == REQUEST_RESTORE ? WORK_MODE_RESTORING : WORK_MODE_SWITCHING;
    return true;
}

static void confirm_actual(uint8_t mode)
{
    state.actual = mode;
    state.actual_valid = true;
    state.controller_seen = true;
    machine_state_confirm_work_mode(mode);
}

static void synchronize(uint8_t mode)
{
    /* A controller synchronization is a new authoritative observation, not an
     * acknowledgement of an in-flight UI command. Never consume it as one. */
    if (state.request != REQUEST_NONE) {
        state.request = REQUEST_NONE;
        state.guard_active = true;
        state.guard_tick = app_clock_uptime_ms();
    }
    confirm_actual(mode);
    if (!state.record.restore_pending) {
        state.record.preferred_valid = true;
        state.record.preferred = mode;
    }
    if (state.failure != FAILURE_LOAD && state.failure != FAILURE_SAVE)
        state.failure = FAILURE_NONE;
}

void work_mode_service_init(void)
{
    if (state.initialized) return;
    state.initialized = true;
    state.phase = WORK_MODE_WAITING_SYNC;
    if (!work_mode_store_begin_load()) fail(FAILURE_LOAD);
}

void work_mode_service_set_diagnostic(bool active)
{
    work_mode_service_init();
    if (state.diagnostic == active) return;
    state.diagnostic = active;
    /* Leaving is an explicit request to recover the saved preference. An
     * earlier temporary request may still complete, so never cancel it here. */
    if (!active && state.record.restore_pending &&
        state.failure != FAILURE_LOAD && state.failure != FAILURE_SAVE)
        state.failure = FAILURE_NONE;
    state.phase = active ? WORK_MODE_WAITING_SYNC : WORK_MODE_RESTORING;
}

void work_mode_service_hold_operation(uint32_t owner, bool active)
{
    if (active) state.operations |= owner;
    else state.operations &= ~owner;
}

bool work_mode_service_diagnostic_active(void) { return state.diagnostic; }

bool work_mode_service_diagnostic_ready(void)
{
    return state.diagnostic && state.loaded && state.actual_valid &&
           state.actual == WORK_MODE_MANUAL && state.record.preferred_valid &&
           state.request == REQUEST_NONE && state.failure == FAILURE_NONE &&
           state.phase == WORK_MODE_READY &&
           (state.record.preferred == WORK_MODE_MANUAL ||
            (state.durable.restore_pending &&
             state.durable.preferred == state.record.preferred));
}

bool work_mode_service_request(uint8_t mode)
{
    work_mode_service_init();
    if (state.operations || state.busy) {
        state.phase = WORK_MODE_WAITING_IDLE;
        state.failure_event = true;
        return false;
    }
    bool explicit_retry = state.failure == FAILURE_SEND ||
        state.failure == FAILURE_REPLY || state.failure == FAILURE_TIMEOUT;
    if (mode > WORK_MODE_MANUAL || !state.loaded || !state.controller_seen ||
        state.stopped || state.diagnostic || state.request != REQUEST_NONE ||
        (!state.actual_valid && !explicit_retry) ||
        (state.record.restore_pending && !explicit_retry) ||
        (state.failure != FAILURE_NONE && !explicit_retry)) return false;
    uint32_t now = app_clock_uptime_ms();
    if (guard_active(now)) return false;
    /* Menu remains a reachable, explicit recovery path after a restore timeout.
     * Do not infer an AUTO retry or erase the journal: only a matching USER
     * acknowledgement may replace the saved preference and resolve recovery. */
    if (explicit_retry) {
        state.failure = FAILURE_NONE;
        state.failure_event = false;
    }
    return send_mode(mode, REQUEST_USER, now);
}

bool work_mode_service_handle_reply(const uint8_t *frame, uint8_t length)
{
    if (!frame || (length != 6 && length != 7) || frame[3] != 0x38) return false;
    work_mode_service_init();
    if (state.stopped) return false;
    if (length == 7) {
        if (frame[4] != 0x02 || frame[5] > 1) return false;
        uint8_t mode = frame[5] == 0x00 ? WORK_MODE_MANUAL : WORK_MODE_AUTO;
        if (!state.loaded) {
            state.deferred_sync = true;
            state.sync_mode = mode;
            confirm_actual(mode);
        } else synchronize(mode);
        return true;
    }
    if (state.request == REQUEST_NONE) return false;
    uint32_t now = app_clock_uptime_ms();
    if ((uint32_t)(now - state.request_tick) >= REQUEST_TIMEOUT_MS) return false;
    if (frame[4] > 1) {
        state.request = REQUEST_NONE;
        state.actual_valid = false;
        state.guard_active = true;
        state.guard_tick = now;
        fail(FAILURE_REPLY);
        return false;
    }
    uint8_t actual = frame[4] == 0x00 ? WORK_MODE_MANUAL : WORK_MODE_AUTO;
    /* No sequence number exists in this protocol. Matching mode prevents an
     * old AUTO echo from acknowledging a newer MANUAL request, and vice versa.
     * Same-mode duplicate echoes cannot be distinguished without controller
     * protocol support; no stronger delivery guarantee is claimed here. */
    if (actual != state.target) return false;
    request_kind_t completed = state.request;
    state.request = REQUEST_NONE;
    confirm_actual(actual);
    if (completed == REQUEST_USER) {
        state.record.preferred_valid = true;
        state.record.preferred = actual;
    }
    return true;
}

static void poll_store(void)
{
    work_mode_record_t record;
    bool load, ok;
    if (!work_mode_store_poll(&record, &load, &ok)) return;
    if (!ok) { fail(load ? FAILURE_LOAD : FAILURE_SAVE); return; }
    state.durable = record;
    if (load) {
        state.record = record;
        state.loaded = true;
        if (state.deferred_sync) {
            state.deferred_sync = false;
            synchronize(state.sync_mode);
        }
    }
}

void work_mode_service_poll(uint32_t now, bool machine_busy)
{
    work_mode_service_init();
    if (state.stopped) return;
    state.busy = machine_busy;
    poll_store();
    if (state.request != REQUEST_NONE &&
        (uint32_t)(now - state.request_tick) >= REQUEST_TIMEOUT_MS) {
        state.request = REQUEST_NONE;
        state.actual_valid = false;
        state.guard_active = true;
        state.guard_tick = now;
        fail(FAILURE_TIMEOUT);
    }
    if (state.failure != FAILURE_NONE) { state.phase = WORK_MODE_FAILED; return; }
    if (!state.loaded) { state.phase = WORK_MODE_SAVING; return; }

    if (state.diagnostic && state.record.preferred_valid &&
        state.record.preferred == WORK_MODE_AUTO)
        state.record.restore_pending = true;

    /* Completion, not a page close, clears the recovery journal. On re-entry
     * before a restore ACK, the lease still wins and requests MANUAL again. */
    if (!state.diagnostic && state.record.restore_pending && state.actual_valid &&
        state.request == REQUEST_NONE && !machine_busy && !state.operations &&
        state.actual == state.record.preferred)
        state.record.restore_pending = false;

    if (!work_mode_store_busy() && !records_equal(&state.record, &state.durable)) {
        if (!work_mode_store_begin_save(&state.record)) { fail(FAILURE_SAVE); return; }
    }
    if (state.request != REQUEST_NONE) {
        state.phase = state.request == REQUEST_RESTORE ? WORK_MODE_RESTORING : WORK_MODE_SWITCHING;
        return;
    }
    if (!state.record.preferred_valid) { state.phase = WORK_MODE_WAITING_SYNC; return; }
    if (state.diagnostic) {
        if (state.record.preferred == WORK_MODE_AUTO &&
            (!state.durable.restore_pending || state.durable.preferred != state.record.preferred)) {
            state.phase = WORK_MODE_SAVING;
            return;
        }
        if (!state.actual_valid && !state.retry_manual) {
            state.phase = WORK_MODE_WAITING_SYNC;
            return;
        }
        if (state.actual_valid && state.actual == WORK_MODE_MANUAL) {
            state.phase = WORK_MODE_READY;
            return;
        }
        if (machine_busy || state.operations) { state.phase = WORK_MODE_WAITING_IDLE; return; }
        state.phase = WORK_MODE_SWITCHING;
        (void)send_mode(WORK_MODE_MANUAL, REQUEST_MANUAL, now);
    } else if (state.record.restore_pending) {
        if (!state.controller_seen) { state.phase = WORK_MODE_WAITING_SYNC; return; }
        if (machine_busy || state.operations) { state.phase = WORK_MODE_WAITING_IDLE; return; }
        state.phase = WORK_MODE_RESTORING;
        (void)send_mode(state.record.preferred, REQUEST_RESTORE, now);
    } else if (state.operations) state.phase = WORK_MODE_WAITING_IDLE;
    else state.phase = state.actual_valid ? WORK_MODE_READY : WORK_MODE_WAITING_SYNC;
}

void work_mode_service_retry(void)
{
    if (state.stopped || state.request != REQUEST_NONE || work_mode_store_busy()) return;
    failure_t previous = state.failure;
    state.failure = FAILURE_NONE;
    state.failure_event = false;
    if (!state.loaded) {
        if (!work_mode_store_begin_load()) fail(FAILURE_LOAD);
    } else if (previous == FAILURE_SEND || previous == FAILURE_TIMEOUT || previous == FAILURE_REPLY) {
        /* MANUAL is safe to request again under the existing recovery journal;
         * an unknown original preference still requires a controller sync. */
        if (state.diagnostic && state.record.preferred_valid)
            state.retry_manual = true;
    }
}

void work_mode_service_stop(void)
{
    state.stopped = true;
    state.request = REQUEST_NONE;
    state.actual_valid = false;
    /* Do not erase the persisted recovery journal on application shutdown. */
}

bool work_mode_service_take_failure(void)
{
    bool result = state.failure_event;
    state.failure_event = false;
    return result;
}

const char *work_mode_service_status_text(void)
{
    if (state.failure == FAILURE_LOAD) return state.diagnostic ?
        "Mode preference unavailable. Retry." : "Mode storage unavailable. Open a diagnostic page to retry.";
    if (state.failure == FAILURE_SAVE) return state.diagnostic ?
        "Could not save mode preference. Retry." : "Mode could not be saved. Open a diagnostic page to retry.";
    if (state.failure == FAILURE_SEND) return state.diagnostic ?
        "Mode request not sent. Retry." : "Mode request not sent. Retry Start mode in Menu.";
    if (state.failure == FAILURE_REPLY) return state.diagnostic ?
        "Mode change not confirmed. Retry." : "Mode not confirmed. Retry Start mode in Menu.";
    if (state.failure == FAILURE_TIMEOUT) return state.diagnostic ?
        "Mode confirmation timed out. Retry." : "Mode timed out. Retry Start mode in Menu.";
    switch (state.phase) {
    case WORK_MODE_WAITING_SYNC: return "Waiting for controller mode";
    case WORK_MODE_SAVING: return "Saving mode preference";
    case WORK_MODE_WAITING_IDLE:
        return state.actual_valid && state.actual == WORK_MODE_MANUAL ?
            "Manual mode retained until the operation finishes" :
            "Waiting for the operation to finish";
    case WORK_MODE_SWITCHING:
        return state.request == REQUEST_USER && state.target == WORK_MODE_AUTO ?
            "Confirming automatic mode" : "Confirming manual mode";
    case WORK_MODE_RESTORING: return "Restoring start mode";
    case WORK_MODE_READY: return state.diagnostic ? "Manual mode confirmed" : "Start mode confirmed";
    default: return "Start mode unavailable";
    }
}

void work_mode_service_get_snapshot(work_mode_snapshot_t *snapshot)
{
    if (!snapshot) return;
    *snapshot = (work_mode_snapshot_t){.phase = state.phase,
        .actual_valid = state.actual_valid, .preferred_valid = state.record.preferred_valid,
        .diagnostic = state.diagnostic, .pending = state.request != REQUEST_NONE,
        .restore_pending = state.record.restore_pending,
        .actual = state.actual, .preferred = state.record.preferred};
}
