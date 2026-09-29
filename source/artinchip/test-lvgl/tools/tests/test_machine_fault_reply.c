#include "un260/protocol/machine_fault_reply.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    uint8_t state[] = { 0xfd, 0xdf, 10, 2, 1, 0x80, 0x80, 0, 0x03, 0 };
    uint8_t activity[] = { 0xfd, 0xdf, 9, 0x14, 0x80, 0x80, 0, 3, 0 };
    machine_fault_reply_t reply = machine_fault_reply_parse(2, state, sizeof(state));
    assert(reply.kind == MACHINE_FAULT_REPLY_SENSOR_SNAPSHOT);
    assert(reply.mask == 0x80800003u);
    for (unsigned length = 0; length < sizeof(state); ++length)
        assert(machine_fault_reply_parse(2, state, length).kind == MACHINE_FAULT_REPLY_INVALID);
    assert(machine_fault_reply_parse(2, NULL, 10).kind == MACHINE_FAULT_REPLY_INVALID);
    state[4] = 2;
    assert(machine_fault_reply_parse(2, state, 10).kind == MACHINE_FAULT_REPLY_INVALID);
    state[4] = 1; state[2] = 9;
    assert(machine_fault_reply_parse(2, state, 10).kind == MACHINE_FAULT_REPLY_INVALID);
    state[2] = 10;
    assert(machine_fault_reply_parse(0x14, state, 10).kind == MACHINE_FAULT_REPLY_INVALID);
    reply = machine_fault_reply_parse(0x14, activity, sizeof(activity));
    assert(reply.kind == MACHINE_FAULT_REPLY_SENSOR_ACTIVITY && reply.mask == 0x80800003u);
    for (unsigned i = 5; i <= 8; ++i) state[i] = 0;
    reply = machine_fault_reply_parse(2, state, sizeof(state));
    assert(reply.kind == MACHINE_FAULT_REPLY_SENSOR_SNAPSHOT && reply.mask == 0);
    state[6] = 4;
    assert(machine_fault_reply_parse(2, state, sizeof(state)).mask == (1u << 18));
    puts("machine_fault_reply: PASS (full frame, endian, unknown bits, idle versus fault)");
    return 0;
}
