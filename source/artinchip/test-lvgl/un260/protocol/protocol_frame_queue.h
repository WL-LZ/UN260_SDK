#ifndef PROTOCOL_FRAME_QUEUE_H
#define PROTOCOL_FRAME_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "protocol_frame.h"

#define PROTOCOL_FRAME_QUEUE_CAPACITY 256

typedef struct {
    uint8_t data[PROTOCOL_FRAME_RX_MAX_SIZE];
    uint16_t len;
    uint64_t received_ms;
} protocol_frame_t;

bool protocol_frame_queue_push(const uint8_t *data, int len);
/* Wait only for queue capacity; a false result leaves ownership with caller.
 * RX retries its complete current frame, preserving order under backpressure. */
bool protocol_frame_queue_push_wait(const uint8_t *data, int len,
                                    uint32_t timeout_ms);
/* RX retries retain the timestamp of the first complete frame. */
bool protocol_frame_queue_push_wait_at(const uint8_t *data, int len,
                                       uint32_t timeout_ms, uint64_t received_ms);
/* Observe timely queued replies without changing protocol order. */
bool protocol_frame_queue_has_command_between(uint8_t command,
                                              uint64_t started_ms,
                                              uint64_t deadline_ms);
bool protocol_frame_queue_pop(protocol_frame_t *frame);
bool protocol_frame_queue_has_pending(void);
void protocol_frame_queue_clear(void);

#endif
