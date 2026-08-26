#ifndef UN260_APP_SERVICE_APP_COUNTING_RUNTIME_H
#define UN260_APP_SERVICE_APP_COUNTING_RUNTIME_H

#include <stdint.h>

#include "un260/counting/counting_data_types.h"
#include "un260/counting/counting_detail_state.h"
#include "un260/counting/counting_session_state.h"

void app_counting_runtime_reset_session(counting_session_state_t *session,
                                        const char *reason);
void app_counting_runtime_handle_info(counting_session_state_t *session,
                                      counting_sim_t *sim_data,
                                      const uint8_t *buf,
                                      uint8_t len);
void app_counting_runtime_handle_control(uint8_t cmd,
                                         counting_session_state_t *session,
                                         const uint8_t *buf,
                                         uint8_t len);
void app_counting_runtime_handle_denom(counting_detail_state_t *detail_state,
                                       counting_session_state_t *session,
                                       counting_sim_t *sim_data,
                                       const uint8_t *buf,
                                       uint8_t len);
void app_counting_runtime_handle_detail(uint8_t cmd,
                                        counting_detail_state_t *detail_state,
                                        counting_session_state_t *session,
                                        counting_sim_t *sim_data,
                                        const uint8_t *buf,
                                        uint8_t len);
void app_counting_runtime_handle_detail_complete(
    counting_session_state_t *session);
void app_counting_runtime_poll_history(counting_session_state_t *session,
                                       const counting_sim_t *sim_data,
                                       uint32_t now_ms);

#endif
