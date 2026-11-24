#ifndef UN260_PROTOCOL_TX_GATE_H
#define UN260_PROTOCOL_TX_GATE_H

#include <stdbool.h>
#include <stdint.h>

typedef int (*protocol_tx_write_t)(const uint8_t *frame, uint16_t len);
typedef enum {
    PROTOCOL_TX_IMMEDIATE,
    PROTOCOL_TX_FEEDBACK_IF_IDLE,
    PROTOCOL_TX_AFTER_FEEDBACK,
    PROTOCOL_TX_STOP_AFTER_FEEDBACK
} protocol_tx_policy_t;

int protocol_tx_gate_send(const uint8_t *frame, uint16_t len, protocol_tx_policy_t policy,
                          protocol_tx_write_t write_frame);
void protocol_tx_gate_on_reply(const uint8_t *frame, uint16_t len);
void protocol_tx_gate_reset(void);

#endif
