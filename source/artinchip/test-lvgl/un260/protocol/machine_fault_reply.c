#include "machine_fault_reply.h"
#include <stddef.h>

static uint32_t read_mask(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

machine_fault_reply_t machine_fault_reply_parse(uint8_t cmd, const uint8_t *frame,
                                               uint8_t frame_len)
{
    machine_fault_reply_t reply = { MACHINE_FAULT_REPLY_INVALID, 0 };
    if (!frame || frame_len < 4 || frame[2] != frame_len || frame[3] != cmd) return reply;
    if (cmd == 0x02 && frame_len == 10 && frame[4] == 0x01) {
        reply.kind = MACHINE_FAULT_REPLY_SENSOR_SNAPSHOT;
        reply.mask = read_mask(frame + 5);
    } else if (cmd == 0x14 && frame_len == 9) {
        reply.kind = MACHINE_FAULT_REPLY_SENSOR_ACTIVITY;
        reply.mask = read_mask(frame + 4);
    }
    return reply;
}
