#include "protocol_frame_parser.h"

#include <stddef.h>

#include "protocol_frame.h"

static void protocol_frame_parser_reset(protocol_frame_parser_t *parser)
{
    parser->index = 0;
    parser->remaining = 0;
    parser->receiving = 0;
}

static void protocol_frame_parser_start(protocol_frame_parser_t *parser)
{
    protocol_frame_parser_reset(parser);
    parser->receiving = 1;
    parser->buffer[parser->index++] = PROTOCOL_FRAME_HEADER_FIRST;
}

void protocol_frame_parser_init(protocol_frame_parser_t *parser)
{
    if (parser == NULL) {
        return;
    }

    protocol_frame_parser_reset(parser);
}

protocol_frame_parse_result_t protocol_frame_parser_feed(protocol_frame_parser_t *parser,
                                                         uint8_t byte,
                                                         protocol_frame_view_t *frame)
{
    if (parser == NULL || frame == NULL) {
        return PROTOCOL_FRAME_PARSE_INCOMPLETE;
    }

    frame->data = NULL;
    frame->len = 0;

    if (!parser->receiving) {
        if (byte == PROTOCOL_FRAME_HEADER_FIRST) {
            protocol_frame_parser_start(parser);
        }
        return PROTOCOL_FRAME_PARSE_INCOMPLETE;
    }

    if (parser->index >= PROTOCOL_FRAME_MAX_SIZE) {
        protocol_frame_parser_reset(parser);
        return PROTOCOL_FRAME_PARSE_OVERFLOW;
    }

    parser->buffer[parser->index++] = byte;

    if (parser->index == 2) {
        if (byte != PROTOCOL_FRAME_HEADER_SECOND) {
            protocol_frame_parser_reset(parser);
            if (byte == PROTOCOL_FRAME_HEADER_FIRST) {
                protocol_frame_parser_start(parser);
            }
        }
        return PROTOCOL_FRAME_PARSE_INCOMPLETE;
    }

    if (parser->index == 3) {
        if (byte < PROTOCOL_FRAME_MIN_SIZE) {
            protocol_frame_parser_reset(parser);
            return PROTOCOL_FRAME_PARSE_INVALID_LENGTH;
        }
        parser->remaining = (int)byte - 3;
        return PROTOCOL_FRAME_PARSE_INCOMPLETE;
    }

    parser->remaining--;
    if (parser->remaining == 0) {
        if (!protocol_frame_is_valid(parser->buffer, parser->index)) {
            protocol_frame_parser_reset(parser);
            if (byte == PROTOCOL_FRAME_HEADER_FIRST) {
                protocol_frame_parser_start(parser);
            }
            return PROTOCOL_FRAME_PARSE_INVALID_FRAME;
        }
        frame->data = parser->buffer;
        frame->len = (uint8_t)parser->index;
        protocol_frame_parser_reset(parser);
        return PROTOCOL_FRAME_PARSE_READY;
    }

    if (parser->index >= PROTOCOL_FRAME_MAX_SIZE) {
        protocol_frame_parser_reset(parser);
        return PROTOCOL_FRAME_PARSE_OVERFLOW;
    }

    return PROTOCOL_FRAME_PARSE_INCOMPLETE;
}
