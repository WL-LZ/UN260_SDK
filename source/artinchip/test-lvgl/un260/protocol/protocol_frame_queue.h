#ifndef PROTOCOL_FRAME_QUEUE_H
#define PROTOCOL_FRAME_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "protocol_frame.h"

#define PROTOCOL_FRAME_QUEUE_CAPACITY 256

typedef struct {
    uint8_t data[PROTOCOL_FRAME_MAX_SIZE];
    uint8_t len;
} protocol_frame_t;

bool protocol_frame_queue_push(const uint8_t *data, int len);
bool protocol_frame_queue_pop(protocol_frame_t *frame);
void protocol_frame_queue_clear(void);

#endif
