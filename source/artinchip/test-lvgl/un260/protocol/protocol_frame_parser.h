#ifndef PROTOCOL_FRAME_PARSER_H
#define PROTOCOL_FRAME_PARSER_H

#include <stdint.h>

#include "protocol_frame.h"

typedef enum {
    PROTOCOL_FRAME_PARSE_INCOMPLETE = 0,
    PROTOCOL_FRAME_PARSE_READY,
    PROTOCOL_FRAME_PARSE_INVALID_LENGTH,
    PROTOCOL_FRAME_PARSE_OVERFLOW,
    PROTOCOL_FRAME_PARSE_INVALID_FRAME,
} protocol_frame_parse_result_t;

typedef struct {
    const uint8_t *data;
    uint16_t len;
} protocol_frame_view_t;

typedef struct {
    uint8_t buffer[PROTOCOL_FRAME_RX_MAX_SIZE];
    int index;
    int remaining;
    int receiving;
    uint16_t expected_length;
    uint16_t image_width;
    uint16_t image_height;
    uint8_t discard_frame;
    uint8_t image_source;
} protocol_frame_parser_t;

void protocol_frame_parser_init(protocol_frame_parser_t *parser);
/* Cancel only an incomplete extended image frame after the RX owner observes
 * a bounded idle interval. Other partial frame behavior remains unchanged. */
bool protocol_frame_parser_reset_partial_image(protocol_frame_parser_t *parser);
protocol_frame_parse_result_t protocol_frame_parser_feed(protocol_frame_parser_t *parser,
                                                         uint8_t byte,
                                                         protocol_frame_view_t *frame);

#endif
