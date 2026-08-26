#ifndef PROTOCOL_FRAME_H
#define PROTOCOL_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROTOCOL_FRAME_HEADER_FIRST   0xFD
#define PROTOCOL_FRAME_HEADER_SECOND  0xDF
#define PROTOCOL_FRAME_TRAILER         0x0A
#define PROTOCOL_FRAME_PAYLOAD_OFFSET  4
#define PROTOCOL_FRAME_OVERHEAD        5
#define PROTOCOL_FRAME_MIN_SIZE        PROTOCOL_FRAME_OVERHEAD
#define PROTOCOL_FRAME_MAX_SIZE        0xFF
#define PROTOCOL_FRAME_MAX_PAYLOAD     \
    (PROTOCOL_FRAME_MAX_SIZE - PROTOCOL_FRAME_OVERHEAD)

int protocol_frame_build(uint8_t *output, size_t output_capacity, uint8_t cmd, const uint8_t *payload, uint16_t payload_len);

/*
 * Validate the common receive-frame structure only. The local command
 * builder appends PROTOCOL_FRAME_TRAILER, but controller replies are framed
 * by their declared length and do not guarantee that value in the final byte.
 */
bool protocol_frame_is_valid(const uint8_t *data, size_t len);

size_t protocol_frame_format_hex(const uint8_t *data, size_t len, char *output, size_t output_size);

#endif
