#include "auxiliary_reply.h"

#include <stddef.h>

#define STATUS_REPLY_MIN_FRAME_LEN 6U
#define PRINT_DETAIL_FRAME_LEN     0x10U
#define PRINT_DONE_FRAME_LEN       0x08U

auxiliary_reply_result_t auxiliary_reply_dispatch(uint8_t cmd,
                                                  const uint8_t *frame,
                                                  uint8_t frame_len)
{
    auxiliary_reply_result_t result = {
        .kind = AUXILIARY_REPLY_INVALID,
        .value = 0,
        .frame_len = frame_len,
    };

    if (frame == NULL) {
        return result;
    }

    switch (cmd) {
    case 0x40:
        if (frame_len < STATUS_REPLY_MIN_FRAME_LEN) {
            return result;
        }

        result.value = frame[4];
        if (result.value == 0x00) {
            result.kind = AUXILIARY_REPLY_DISPLAY_MAIN;
        } else if (result.value == 0x01) {
            result.kind = AUXILIARY_REPLY_DISPLAY_DETAIL;
        } else {
            result.kind = AUXILIARY_REPLY_DISPLAY_UNKNOWN;
        }
        return result;

    case 0x3C:
        if (frame_len == PRINT_DETAIL_FRAME_LEN) {
            result.kind = AUXILIARY_REPLY_PRINT_DETAIL;
        } else if (frame_len == PRINT_DONE_FRAME_LEN) {
            result.kind = AUXILIARY_REPLY_PRINT_DONE;
        } else {
            result.kind = AUXILIARY_REPLY_PRINT_UNKNOWN;
        }
        return result;

    case 0x3B:
        if (frame_len < STATUS_REPLY_MIN_FRAME_LEN) {
            return result;
        }

        result.kind = AUXILIARY_REPLY_CLEAR_DATA_ACK;
        result.value = frame[4];
        return result;

    default:
        return result;
    }
}
