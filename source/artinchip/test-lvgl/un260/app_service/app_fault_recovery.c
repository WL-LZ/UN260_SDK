#include "app_fault_recovery.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_system/app_clock.h"
#include "un260/protocol/protocol_send.h"
#include <string.h>

/* 0x3D/01 has no fault id. Keep at most one command awaiting a result so a
 * response cannot resolve a later, unrelated report. Both UI Confirm and a
 * completed island animation use this sender. */
static struct {
    machine_fault_key_t key, pending_key;
    bool valid, needs_clear, pending;
    uint32_t sent_tick;
} recovery;

/* 0x3D/01 is reserved for running paper jams (0x0F/01..05) and
 * the reached batch target (0x06/04). All other reports are presentation
 * acknowledgements only, including No banknotes and 0x0A/02 faults. */
static bool clearable(machine_fault_key_t key)
{
    return (key.source == MACHINE_FAULT_RUNTIME && key.type == 0 &&
            key.code >= 1 && key.code <= 5) ||
           (key.source == MACHINE_FAULT_PRESET && key.type == 0 &&
            key.code == 4);
}

static bool send_clear(machine_fault_key_t key)
{
    const uint8_t payload = 0x01;
    if (protocol_send(0x3D, &payload, 1) < 0) return false;
    recovery.pending_key = key;
    recovery.pending = true;
    recovery.sent_tick = app_clock_uptime_ms();
    return true;
}

bool app_fault_recovery_request_clear(machine_fault_key_t key)
{
    if (!clearable(key)) return true;
    /* The reply carries no fault identity. A different fault must wait for
     * the previous reply or timeout; Confirm then stays open for retry. */
    if (recovery.pending && !machine_fault_key_equal(recovery.pending_key, key))
        return false;
    return send_clear(key);
}

static void notice_phase(machine_fault_key_t key, smart_island_fault_phase_t phase)
{
    if (phase != SMART_ISLAND_FAULT_END || !recovery.valid ||
        !machine_fault_key_equal(key, recovery.key) || !clearable(key) ||
        fault_popup_get_auto_enabled() || fault_popup_is_showing() ||
        !machine_fault_find(key, NULL)) return;
    if (!app_fault_recovery_request_clear(key))
        fault_popup_repeat_notice(key);
}

void app_fault_recovery_init(void)
{
    app_fault_recovery_clear();
    smart_island_register_fault_phase_cb(notice_phase);
}

void app_fault_recovery_clear(void) { memset(&recovery, 0, sizeof(recovery)); }

void app_fault_recovery_stacker_cleared(void)
{
    if (recovery.valid && recovery.key.source == MACHINE_FAULT_START &&
        recovery.key.type == 2 && recovery.key.code == 7)
        recovery.valid = false;
    fault_popup_stacker_cleared();
}

void app_fault_recovery_report(machine_fault_key_t key)
{
    bool known = (key.source == MACHINE_FAULT_START &&
                  (key.type == 1 || key.type == 2)) ||
        (key.source == MACHINE_FAULT_RUNTIME && key.code >= 1 && key.code <= 7) ||
        key.source == MACHINE_FAULT_PRESET;
    if (!known) { recovery.valid = false; return; }
    recovery.key = key;
    recovery.valid = true;
    recovery.needs_clear = clearable(key);
}

static void present_unresolved(machine_fault_key_t key)
{
    if (!machine_fault_find(key, NULL)) return;
    if (fault_popup_get_auto_enabled())
        (void)fault_popup_show_key(key);
    else
        fault_popup_repeat_notice(key);
}

void app_fault_recovery_handle_clear_result(uint8_t result)
{
    if (!recovery.pending || (result != 0x01 && result != 0x02)) return;
    machine_fault_key_t key = recovery.pending_key;
    recovery.pending = false;
    if (result == 0x01) {
        if (recovery.valid && machine_fault_key_equal(recovery.key, key)) {
            recovery.valid = false;
            recovery.needs_clear = false;
        }
        if (clearable(key)) fault_popup_resolve_key(key);
    } else {
        present_unresolved(key);
    }
}

void app_fault_recovery_poll(void)
{
    if (!recovery.pending ||
        (uint32_t)(app_clock_uptime_ms() - recovery.sent_tick) < 3000U) return;
    machine_fault_key_t key = recovery.pending_key;
    recovery.pending = false;
    present_unresolved(key);
}

bool app_fault_recovery_prepare_start(void)
{
    if (fault_popup_is_showing()) return false;
    return !recovery.valid || !recovery.needs_clear ||
        app_fault_recovery_request_clear(recovery.key);
}
