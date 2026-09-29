#ifndef APP_FAULT_RECOVERY_H
#define APP_FAULT_RECOVERY_H
#include "un260/machine_state/machine_fault.h"
typedef enum {
    APP_FAULT_START_READY,
    APP_FAULT_START_WAIT,
    APP_FAULT_START_BLOCKED
} app_fault_start_state_t;
void app_fault_recovery_init(void);
void app_fault_recovery_report(machine_fault_key_t key);
void app_fault_recovery_clear(void);
void app_fault_recovery_count_started(void);
void app_fault_recovery_count_finished(void);
void app_fault_recovery_confirm(machine_fault_key_t key);
void app_fault_recovery_stacker_cleared(void);
void app_fault_recovery_handle_reply(const uint8_t *buf, uint8_t len);
void app_fault_recovery_poll(void);
/* START intent belongs to the command owner. This service only clears faults;
 * callers wait for a positive 3D reply before sending their explicit START. */
app_fault_start_state_t app_fault_recovery_prepare_start(void);
app_fault_start_state_t app_fault_recovery_start_status(void);
#endif
