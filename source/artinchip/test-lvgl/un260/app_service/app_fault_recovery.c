#include "app_fault_recovery.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_system/app_clock.h"
#include "un260/protocol/protocol_send.h"
#include <string.h>

/* Keep the established controller handshake separate from guide Confirm.
 * 0x3D/01 releases the fault latch; it is not START or proof of recovery.
 * Pocket reports use the legacy begin/end cadence. Other start faults have
 * a 2s post-notice delay and at most three automatic attempts. */
static struct {
    machine_fault_key_t key;
    bool valid, due, needs_ack;
    uint8_t attempts;
    uint32_t due_tick;
} recovery;

static bool pocket(void)
{
    return recovery.key.source == MACHINE_FAULT_START && recovery.key.type == 2 &&
        (recovery.key.code == 5 || recovery.key.code == 7 || recovery.key.code == 8);
}
static bool automatic_allowed(void)
{
    return recovery.valid && recovery.key.source == MACHINE_FAULT_START &&
        !fault_popup_get_auto_enabled() && !fault_popup_is_showing();
}
static bool send_ack(void)
{
    const uint8_t payload = 1;
    if (protocol_send(0x3D, &payload, 1) < 0) return false;
    recovery.needs_ack = false;
    return true;
}
static void notice_phase(machine_fault_key_t key, smart_island_fault_phase_t phase)
{
    if (!recovery.valid || !machine_fault_key_equal(key, recovery.key)) return;
    if (phase == SMART_ISLAND_FAULT_CANCEL) { recovery.due = false; return; }
    if (!automatic_allowed()) return;
    if (pocket()) {
        (void)send_ack();
    } else if (phase == SMART_ISLAND_FAULT_END) {
        recovery.due = true;
        recovery.due_tick = app_clock_uptime_ms();
    }
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
        recovery.key.type == 2 && recovery.key.code == 7) app_fault_recovery_clear();
    fault_popup_stacker_cleared();
}
void app_fault_recovery_report(machine_fault_key_t key)
{
    bool known = (key.source == MACHINE_FAULT_START && key.type == 2 && key.code >= 1 && key.code <= 13) ||
        (key.source == MACHINE_FAULT_RUNTIME && key.code >= 1 && key.code <= 7);
    if (!known) { app_fault_recovery_clear(); return; }
    if (!recovery.valid || !machine_fault_key_equal(key, recovery.key)) {
        app_fault_recovery_clear(); recovery.key = key; recovery.valid = true;
    }
    recovery.needs_ack = true;
}
void app_fault_recovery_poll(void)
{
    if (!recovery.due) return;
    if (!automatic_allowed()) { recovery.due = false; return; }
    if ((uint32_t)(app_clock_uptime_ms() - recovery.due_tick) < 2000U) return;
    recovery.due = false;
    if (recovery.attempts >= 3) {
        (void)fault_popup_show_key(recovery.key);
        return;
    }
    ++recovery.attempts;
    (void)send_ack();
}
bool app_fault_recovery_prepare_start(void)
{
    if (fault_popup_is_showing()) return false;
    recovery.due = false;
    return !recovery.valid || !recovery.needs_ack || send_ack();
}
