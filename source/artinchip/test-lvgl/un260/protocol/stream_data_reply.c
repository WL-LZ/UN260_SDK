#include "stream_data_reply.h"

#include <stddef.h>

#include "protocol_frame.h"

stream_data_reply_view_t stream_data_reply_parse(const uint8_t *frame,
                                                 uint8_t frame_len)
{
    stream_data_reply_view_t view = {
        .kind = STREAM_DATA_REPLY_INVALID,
        .payload = NULL,
        .payload_len = 0,
    };

    if (frame == NULL || frame_len <= PROTOCOL_FRAME_MIN_SIZE) {
        return view;
    }
    if (frame[0] != PROTOCOL_FRAME_HEADER_FIRST ||
        frame[1] != PROTOCOL_FRAME_HEADER_SECOND ||
        frame[2] != frame_len) {
        return view;
    }

    if (frame[3] == 0x47) {
        view.kind = STREAM_DATA_REPLY_IMAGE;
    } else if (frame[3] == 0x48) {
        view.kind = STREAM_DATA_REPLY_WAVE;
    } else {
        return view;
    }

    view.payload = &frame[PROTOCOL_FRAME_PAYLOAD_OFFSET];
    view.payload_len = (uint16_t)(frame_len - PROTOCOL_FRAME_MIN_SIZE);
    return view;
}
