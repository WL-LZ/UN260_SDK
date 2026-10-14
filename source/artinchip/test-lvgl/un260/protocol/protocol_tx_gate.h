#ifndef UN260_PROTOCOL_TX_GATE_H
#define UN260_PROTOCOL_TX_GATE_H
#include <stdbool.h>
#include <stdint.h>
typedef int (*protocol_tx_write_t)(const uint8_t *frame, uint16_t len);
/* Normal writes retain their immediate send semantics. Optional writes return
 * zero while normal command replies are outstanding; no queue or UI waits. */
int protocol_tx_gate_send(const uint8_t *frame, uint16_t len, bool optional,
                          protocol_tx_write_t write_frame);
void protocol_tx_gate_on_reply(uint8_t command);
void protocol_tx_gate_reset(void);
#endif
