#ifndef APP_FAULT_RECOVERY_H
#define APP_FAULT_RECOVERY_H
#include "un260/machine_state/machine_fault.h"
void app_fault_recovery_init(void);
void app_fault_recovery_report(machine_fault_key_t key);
void app_fault_recovery_clear(void);
void app_fault_recovery_stacker_cleared(void);
void app_fault_recovery_poll(void);
/* Explicit START may release the last controller fault latch first.
 * No-note, unknown reports and presentation acknowledgements never do. */
bool app_fault_recovery_prepare_start(void);
#endif
