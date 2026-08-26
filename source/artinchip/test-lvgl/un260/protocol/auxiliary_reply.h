#ifndef UN260_PROTOCOL_AUXILIARY_REPLY_H
#define UN260_PROTOCOL_AUXILIARY_REPLY_H

#include <stdint.h>

typedef enum {
    AUXILIARY_REPLY_INVALID = 0,
    AUXILIARY_REPLY_DISPLAY_MAIN,
    AUXILIARY_REPLY_DISPLAY_DETAIL,
    AUXILIARY_REPLY_DISPLAY_UNKNOWN,
    AUXILIARY_REPLY_PRINT_DETAIL,
    AUXILIARY_REPLY_PRINT_DONE,
    AUXILIARY_REPLY_PRINT_UNKNOWN,
    AUXILIARY_REPLY_CLEAR_DATA_ACK,
} auxiliary_reply_kind_t;

typedef struct {
    auxiliary_reply_kind_t kind;
    uint8_t value;
    uint8_t frame_len;
} auxiliary_reply_result_t;

auxiliary_reply_result_t auxiliary_reply_dispatch(uint8_t cmd,
                                                  const uint8_t *frame,
                                                  uint8_t frame_len);

#endif
