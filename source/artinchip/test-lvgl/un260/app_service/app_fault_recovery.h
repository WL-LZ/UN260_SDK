#ifndef APP_FAULT_RECOVERY_H
#define APP_FAULT_RECOVERY_H
#include "un260/machine_state/machine_fault.h"
void app_fault_recovery_init(void);
void app_fault_recovery_report(machine_fault_key_t key);
void app_fault_recovery_clear(void);
void app_fault_recovery_stacker_cleared(void);
void app_fault_recovery_poll(void);
/* Only 0x0F/01..05 and 0x06/04 transmit 0x3D; all other Confirm actions are UI-only. */
bool app_fault_recovery_request_clear(machine_fault_key_t key);
void app_fault_recovery_handle_clear_result(uint8_t result);
/* Explicit START releases a known clearable controller fault latch first. */
bool app_fault_recovery_prepare_start(void);
#endif
