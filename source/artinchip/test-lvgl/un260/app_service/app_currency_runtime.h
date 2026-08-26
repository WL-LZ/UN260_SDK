#ifndef UN260_APP_SERVICE_APP_CURRENCY_RUNTIME_H
#define UN260_APP_SERVICE_APP_CURRENCY_RUNTIME_H

#include <stdint.h>

#include "un260/counting/counting_detail_state.h"
#include "un260/counting/counting_session_state.h"

void app_currency_runtime_handle_reply(counting_detail_state_t *detail_state,
                                       counting_session_state_t *session,
                                       const uint8_t *buf,
                                       uint8_t len);
void app_currency_runtime_handle_detected(const uint8_t *buf, uint8_t len);

#endif
