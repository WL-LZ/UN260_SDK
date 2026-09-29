#include "app_fault_recovery.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/protocol/protocol_send.h"
#include "un260/protocol/protocol_frame.h"
#include "un260/protocol/protocol_request.h"
#include <string.h>

/* Controller recovery is independent of animation and of message read state.
 * One 3D is in flight. Only its positive reply releases the pending START;
 * failure/timeout retains the latch and requires a fresh operator/device event.
 * No UI callback here sends START or treats clearing as physical recovery. */
static struct {
    /* START, RUNTIME and BATCH can coexist; a later door report must not
     * discard an earlier batch latch and its physical-removal route. */
    uint32_t reported_sources, stacker_sources;
    bool needs_ack, due, stacker_removed;
    uint32_t generation, sent_generation;
    protocol_request_t request;
} recovery;
static bool counting;

static bool recoverable(machine_fault_key_t key)
{
    return (key.source == MACHINE_FAULT_START && key.type == 2 && key.code >= 1 && key.code <= 13) ||
        (key.source == MACHINE_FAULT_RUNTIME && key.code >= 1 && key.code <= 7) ||
        (key.source == MACHINE_FAULT_BATCH && key.code == 4);
}
static bool no_note(machine_fault_key_t key)
{ return key.source == MACHINE_FAULT_START && key.type == 1 && key.code == 2; }
static bool stacker_condition(machine_fault_key_t key)
{
    return key.source == MACHINE_FAULT_BATCH ||
        (key.source == MACHINE_FAULT_START && key.type == 2 &&
         (key.code == 7 || key.code == 8)) ||
        (key.source == MACHINE_FAULT_RUNTIME && key.code == 7);
}
static void failure(const char *message)
{
    /* A removal received during this request is a new authorized event.
     * Keep it queued on failure; a failure itself never schedules a retry. */
    recovery.needs_ack = true;
    ui_notice_post_text(UI_NOTICE_ERROR,"machine.recovery",UI_N_("Machine recovery"),message);
}
static bool request_clear(void)
{
    if (protocol_request_is_pending(&recovery.request)) return true;
    const uint8_t payload = 1;
    recovery.request.timeout_ms = 2000U;
    if (!protocol_request_begin(&recovery.request)) return false;
    recovery.sent_generation = recovery.generation;
    recovery.due = false;
    ui_notice_clear("machine.recovery");
    if (protocol_send(0x3D,&payload,1) < 0) {
        protocol_request_finish(&recovery.request);
        failure(UI_N_("Could not send the clear request. Try again."));
        return false;
    }
    return true;
}
void app_fault_recovery_init(void) { counting=false; app_fault_recovery_clear(); }
void app_fault_recovery_clear(void)
{
    memset(&recovery,0,sizeof(recovery));
    ui_notice_clear("machine.recovery");
}
void app_fault_recovery_count_started(void)
{
    counting=true;
    app_fault_recovery_clear();
}
void app_fault_recovery_count_finished(void) { counting=false; }
void app_fault_recovery_report(machine_fault_key_t key)
{
    /* A no-note/unknown warning must not erase an outstanding controller latch.
     * 0F/00 concerns runtime faults, not a batch/full-pocket condition. */
    if (key.source == MACHINE_FAULT_RUNTIME && key.code == 0) {
        recovery.reported_sources &= ~(UINT32_C(1)<<MACHINE_FAULT_RUNTIME);
        recovery.stacker_sources &= ~(UINT32_C(1)<<MACHINE_FAULT_RUNTIME);
        if (!recovery.reported_sources && !protocol_request_is_pending(&recovery.request))
            app_fault_recovery_clear();
        return;
    }
    if (!recoverable(key)) return;
    uint32_t source_bit=UINT32_C(1)<<key.source;
    recovery.reported_sources |= source_bit;
    recovery.stacker_sources &= ~source_bit;
    recovery.needs_ack=true;
    if (stacker_condition(key)) {
        recovery.stacker_sources |= source_bit;
        recovery.stacker_removed=false;
    }
    ++recovery.generation;
    /* A report asserts a condition; it is not permission to clear it.
     * It also invalidates an older confirmation deferred while counting. */
    recovery.due=false;
}
void app_fault_recovery_confirm(machine_fault_key_t key)
{
    if ((!recoverable(key) && !no_note(key)) ||
        protocol_request_is_pending(&recovery.request)) return;
    /* Explicit no-note confirmation also permits releasing a stale device
     * latch, without borrowing the identity of another on-screen message. */
    recovery.needs_ack=true;
    recovery.due=true;
    if (!counting) (void)request_clear();
}
void app_fault_recovery_stacker_cleared(void)
{
    if (recovery.stacker_sources && recovery.needs_ack && !recovery.stacker_removed) {
        recovery.stacker_removed=true;
        recovery.due=true;
        if (!counting) (void)request_clear();
    }
    /* Physical removal retires only these presentation records. It does not
     * cancel an in-flight 3D or fabricate a successful clear response. */
    fault_popup_stacker_cleared();
}
void app_fault_recovery_handle_reply(const uint8_t *buf,uint8_t len)
{
    if (len != 6 || !protocol_frame_is_valid(buf,len) || buf[3] != 0x3D ||
        (buf[4] != 1 && buf[4] != 2) || !protocol_request_take_result(&recovery.request)) return;
    if (buf[4] == 2) { failure(UI_N_("The machine could not clear the fault. Check it and try again.")); return; }
    if (recovery.sent_generation == recovery.generation) {
        recovery.needs_ack=false;recovery.due=false;
    }
    ui_notice_clear("machine.recovery");
}
void app_fault_recovery_poll(void)
{
    if (protocol_request_take_timeout(&recovery.request)) {
        failure(UI_N_("No clear confirmation received. Check the connection and try again."));
        return;
    }
    if (!recovery.due || counting || protocol_request_is_pending(&recovery.request)) return;
    (void)request_clear();
}
app_fault_start_state_t app_fault_recovery_start_status(void)
{
    if (protocol_request_is_pending(&recovery.request)) return APP_FAULT_START_WAIT;
    return recovery.needs_ack ? APP_FAULT_START_BLOCKED : APP_FAULT_START_READY;
}
app_fault_start_state_t app_fault_recovery_prepare_start(void)
{
    if (counting || fault_popup_is_showing()) return APP_FAULT_START_BLOCKED;
    if (protocol_request_is_pending(&recovery.request)) return APP_FAULT_START_WAIT;
    if (!recovery.needs_ack) return APP_FAULT_START_READY;
    return request_clear() ? APP_FAULT_START_WAIT : APP_FAULT_START_BLOCKED;
}
