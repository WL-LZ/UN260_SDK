#ifndef BOOT_SERVICE_H
#define BOOT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "un260/boot/boot_state.h"

typedef enum {
    BOOT_SERVICE_ACTION_NONE = 0,
    BOOT_SERVICE_ACTION_SEND_HANDSHAKE,
    BOOT_SERVICE_ACTION_HANDSHAKE_TIMEOUT,
    BOOT_SERVICE_ACTION_SELF_TEST_TIMEOUT,
} boot_service_action_t;

void boot_service_start(uint32_t now_ms);
void boot_service_set_stage(boot_stage_t stage);
boot_stage_t boot_service_get_stage(void);
void boot_service_advance_stage(void);
boot_service_action_t boot_service_poll(uint32_t now_ms);
void boot_service_reset_handshake(void);
bool boot_service_request_handshake(uint32_t request_tick);
void boot_service_confirm_handshake(void);
handshake_state_t boot_service_handshake_state(void);
uint32_t boot_service_handshake_tick(void);
uint32_t boot_service_handshake_start_tick(void);
void boot_service_reset_self_test(void);
bool boot_service_next_self_test_protocol_step(uint8_t *protocol_step);
uint8_t boot_service_self_test_sequence_index(void);
void boot_service_reset_self_test_results(void);
bool boot_service_record_self_test_result(uint8_t protocol_step, uint8_t result, uint8_t *index);
boot_self_test_event_t boot_service_take_self_test_event(uint8_t *failure_step, uint8_t *failure_result);

#endif
