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
    uint8_t len;
} protocol_frame_view_t;

typedef struct {
    uint8_t buffer[PROTOCOL_FRAME_MAX_SIZE];
    int index;
    int remaining;
    int receiving;
} protocol_frame_parser_t;

void protocol_frame_parser_init(protocol_frame_parser_t *parser);
protocol_frame_parse_result_t protocol_frame_parser_feed(protocol_frame_parser_t *parser,
                                                         uint8_t byte,
                                                         protocol_frame_view_t *frame);

#endif
