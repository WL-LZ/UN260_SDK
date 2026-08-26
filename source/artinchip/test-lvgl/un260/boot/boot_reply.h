#ifndef BOOT_REPLY_H
#define BOOT_REPLY_H

#include <stdint.h>

#include "boot_service.h"

typedef enum {
    BOOT_REPLY_INVALID = 0,
    BOOT_REPLY_IGNORED,
    BOOT_REPLY_HANDSHAKE_ACCEPTED,
    BOOT_REPLY_SELF_TEST_RECORDED,
} boot_reply_kind_t;

typedef struct {
    boot_reply_kind_t kind;
    uint8_t self_test_index;
    uint8_t self_test_result;
    boot_self_test_event_t self_test_event;
    uint8_t first_failure_step;
    uint8_t first_failure_result;
} boot_reply_result_t;

boot_reply_result_t boot_reply_dispatch(uint8_t cmd,
                                        const uint8_t *buf,
                                        uint8_t len);

#endif
