#ifndef UN260_MACHINE_FAULT_REPLY_H
#define UN260_MACHINE_FAULT_REPLY_H
#include <stdint.h>

typedef enum {
    MACHINE_FAULT_REPLY_INVALID = 0,
    MACHINE_FAULT_REPLY_SENSOR_SNAPSHOT,
    /* 0x14 reports occupied/idle sensors, not failed/healthy sensors. */
    MACHINE_FAULT_REPLY_SENSOR_ACTIVITY
} machine_fault_reply_kind_t;

typedef struct {
    machine_fault_reply_kind_t kind;
    uint32_t mask;
} machine_fault_reply_t;

/* Consumes an already CRC-validated complete protocol frame. */
machine_fault_reply_t machine_fault_reply_parse(uint8_t cmd, const uint8_t *frame,
                                               uint8_t frame_len);
#endif
