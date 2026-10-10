#ifndef APP_FAULT_RECOVERY_H
#define APP_FAULT_RECOVERY_H
#include "un260/machine_state/machine_fault.h"
#include "un260/counting/counting_session_state.h"
void app_fault_recovery_init(void);
void app_fault_recovery_report(machine_fault_key_t key);
/* Matched controller refusals create transient operation guides only.
 * An adjacent duplicate report may coalesce; independent jams retain their ABI. */
bool app_fault_recovery_report_setting_rejection(uint8_t command);
void app_fault_recovery_cancel_setting_detail(void);
void app_fault_recovery_cancel_setting_presentation(void);
void app_fault_recovery_resolve_setting_rejection(uint8_t command);
bool app_fault_recovery_observe_setting_detail(uint8_t command,
    const uint8_t *frame, uint16_t length, const counting_session_state_t *session);
void app_fault_recovery_clear(void);
void app_fault_recovery_stacker_cleared(void);
void app_fault_recovery_poll(void);
/* Full pockets 0x0A/02/{05,07,08}, jams 0x0F/01..05 and preset 0x06/04 use
 * 0x3D. A full-pocket ACK releases the latch without proving physical recovery. */
bool app_fault_recovery_request_clear(machine_fault_key_t key);
void app_fault_recovery_handle_clear_result(uint8_t result);
/* Explicit START releases a known clearable controller fault latch first. */
bool app_fault_recovery_prepare_start(void);
#endif
