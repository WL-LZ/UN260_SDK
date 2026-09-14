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

#define BOOT_SELF_TEST_COUNT 5U
typedef struct {
    bool received;
    uint8_t result;
} boot_item_result_t;
typedef struct {
    boot_stage_t stage;
    bool connected;
    uint8_t requested_count;
    uint8_t completed_count;
    boot_item_result_t items[BOOT_SELF_TEST_COUNT];
} boot_snapshot_t;

void boot_service_snapshot(boot_snapshot_t *snapshot);
void boot_service_cancel(void);
bool boot_service_reply_window_open(uint32_t now_ms);

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
