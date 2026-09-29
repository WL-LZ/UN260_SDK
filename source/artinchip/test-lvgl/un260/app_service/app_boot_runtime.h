#ifndef UN260_APP_SERVICE_APP_BOOT_RUNTIME_H
#define UN260_APP_SERVICE_APP_BOOT_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "un260/counting/counting_session_state.h"
#include "un260/machine_state/machine_fault.h"

/* Only the self-test page owns this recovery navigation. */
bool app_boot_runtime_confirm_fault(machine_fault_key_t key);

void app_boot_runtime_handle_reply(counting_session_state_t *counting_session,
                                   uint8_t cmd,
                                   const uint8_t *buf,
                                   uint8_t len);
void app_boot_runtime_poll(uint32_t now_ms, bool boot_page_active);

#endif
