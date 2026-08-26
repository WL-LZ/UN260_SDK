#ifndef UN260_APP_SERVICE_APP_BOOT_RUNTIME_H
#define UN260_APP_SERVICE_APP_BOOT_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "un260/counting/counting_session_state.h"

void app_boot_runtime_handle_reply(counting_session_state_t *counting_session,
                                   uint8_t cmd,
                                   const uint8_t *buf,
                                   uint8_t len);
void app_boot_runtime_poll(uint32_t now_ms, bool boot_page_active);

#endif
