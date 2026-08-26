#include "protocol_frame.h"

#include <string.h>

int protocol_frame_build(uint8_t *output, size_t output_capacity, uint8_t cmd, const uint8_t *payload, uint16_t payload_len)
{
    size_t frame_len;

    if (output == NULL || payload_len > PROTOCOL_FRAME_MAX_PAYLOAD) {
        return -1;
    }
    if (payload_len > 0 && payload == NULL) {
        return -1;
    }

    frame_len = (size_t)payload_len + PROTOCOL_FRAME_OVERHEAD;
    if (output_capacity < frame_len) {
        return -1;
    }

    output[0] = PROTOCOL_FRAME_HEADER_FIRST;
    output[1] = PROTOCOL_FRAME_HEADER_SECOND;
    output[2] = (uint8_t)frame_len;
    output[3] = cmd;
    if (payload_len > 0) {
        memcpy(&output[PROTOCOL_FRAME_PAYLOAD_OFFSET], payload, payload_len);
    }
    output[frame_len - 1] = PROTOCOL_FRAME_TRAILER;

    return (int)frame_len;
}

bool protocol_frame_is_valid(const uint8_t *data, size_t len)
{
    if (data == NULL || len < PROTOCOL_FRAME_MIN_SIZE ||
        len > PROTOCOL_FRAME_MAX_SIZE) {
        return false;
    }
    if (data[0] != PROTOCOL_FRAME_HEADER_FIRST ||
        data[1] != PROTOCOL_FRAME_HEADER_SECOND) {
        return false;
    }
    return (size_t)data[2] == len;
}

size_t protocol_frame_format_hex(const uint8_t *data, size_t len, char *output, size_t output_size)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    size_t pos = 0;

    if (output == NULL || output_size == 0U) {
        return 0;
    }
    output[0] = '\0';
    if (data == NULL) {
        return 0;
    }

    for (size_t i = 0; i < len; i++) {
        size_t required = i == 0U ? 2U : 3U;

        if (pos + required >= output_size) {
            break;
        }
        if (i > 0U) {
            output[pos++] = ' ';
        }
        output[pos++] = hex_digits[data[i] >> 4];
        output[pos++] = hex_digits[data[i] & 0x0FU];
    }
    output[pos] = '\0';
    return pos;
}
