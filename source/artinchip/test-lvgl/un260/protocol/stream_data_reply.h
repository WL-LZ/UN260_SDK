#ifndef UN260_PROTOCOL_STREAM_DATA_REPLY_H
#define UN260_PROTOCOL_STREAM_DATA_REPLY_H

#include <stdint.h>

typedef enum {
    STREAM_DATA_REPLY_INVALID = 0,
    STREAM_DATA_REPLY_IMAGE,
    STREAM_DATA_REPLY_WAVE,
} stream_data_reply_kind_t;

typedef struct {
    stream_data_reply_kind_t kind;
    const uint8_t *payload;
    uint16_t payload_len;
} stream_data_reply_view_t;

/* The returned payload borrows storage from frame and is valid only while frame is valid. */
stream_data_reply_view_t stream_data_reply_parse(const uint8_t *frame,
                                                 uint8_t frame_len);

#endif
