#ifndef STARTUP_SYNC_REPLY_H
#define STARTUP_SYNC_REPLY_H

#include <stdint.h>

typedef enum {
    STARTUP_SYNC_REPLY_INVALID = 0,
    STARTUP_SYNC_REPLY_IGNORED,
    STARTUP_SYNC_REPLY_START,
    STARTUP_SYNC_REPLY_DATA,
    STARTUP_SYNC_REPLY_END,
} startup_sync_reply_result_t;

startup_sync_reply_result_t startup_sync_reply_dispatch(uint8_t cmd,
                                                        const uint8_t *buf,
                                                        uint8_t len);

#endif
