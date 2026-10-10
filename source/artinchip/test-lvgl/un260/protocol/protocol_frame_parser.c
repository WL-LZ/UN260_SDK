#include "protocol_frame_parser.h"

#include <stddef.h>

#include "protocol_frame.h"
#include "stream_data_reply.h"

static void protocol_frame_parser_reset_current(protocol_frame_parser_t *parser)
{
    parser->index = 0;
    parser->remaining = 0;
    parser->receiving = 0;
    parser->expected_length = 0;
    parser->discard_frame = 0;
}

static void protocol_frame_parser_reset(protocol_frame_parser_t *parser)
{
    protocol_frame_parser_reset_current(parser);
    parser->image_source = 0;
    parser->image_width = 0;
    parser->image_height = 0;
}

static void protocol_frame_parser_start(protocol_frame_parser_t *parser)
{
    protocol_frame_parser_reset_current(parser);
    parser->receiving = 1;
    parser->buffer[parser->index++] = PROTOCOL_FRAME_HEADER_FIRST;
}

static protocol_frame_parse_result_t protocol_frame_parser_reject(protocol_frame_parser_t *parser,
                                                                   uint8_t byte,
                                                                   protocol_frame_parse_result_t result)
{
    protocol_frame_parser_reset(parser);
    if (byte == PROTOCOL_FRAME_HEADER_FIRST) protocol_frame_parser_start(parser);
    return result;
}

static void protocol_frame_parser_track_image(protocol_frame_parser_t *parser)
{
    if (parser->buffer[3] != 0x47 || parser->index < 6) return;
    if (parser->buffer[4] != 0x00 && parser->buffer[4] != 0xFF) return;

    parser->image_source = 0;
    parser->image_width = 0;
    parser->image_height = 0;
    image_data_reply_view_t reply = image_data_reply_parse(
        &parser->buffer[PROTOCOL_FRAME_PAYLOAD_OFFSET],
        (uint16_t)(parser->index - PROTOCOL_FRAME_OVERHEAD));
    if (reply.kind != IMAGE_DATA_REPLY_HEADER) return;
    uint16_t sample_bytes = image_data_reply_scanline_bytes(reply.image_id,
                                                           reply.width, reply.height);
    if (!sample_bytes) return;
    parser->image_source = reply.image_id;
    parser->image_width = reply.width;
    parser->image_height = reply.height;
}

void protocol_frame_parser_init(protocol_frame_parser_t *parser)
{
    if (parser == NULL) return;
    protocol_frame_parser_reset(parser);
}

bool protocol_frame_parser_reset_partial_image(protocol_frame_parser_t *parser)
{
    if (parser == NULL || !parser->receiving ||
        parser->expected_length <= PROTOCOL_FRAME_MAX_SIZE ||
        parser->index < 5 || parser->buffer[3] != 0x47) return false;
    protocol_frame_parser_reset(parser);
    return true;
}

protocol_frame_parse_result_t protocol_frame_parser_feed(protocol_frame_parser_t *parser,
                                                         uint8_t byte,
                                                         protocol_frame_view_t *frame)
{
    if (parser == NULL || frame == NULL) return PROTOCOL_FRAME_PARSE_INCOMPLETE;
    frame->data = NULL;
    frame->len = 0;

    if (!parser->receiving) {
        if (byte == PROTOCOL_FRAME_HEADER_FIRST) protocol_frame_parser_start(parser);
        return PROTOCOL_FRAME_PARSE_INCOMPLETE;
    }
    if (parser->index >= (int)PROTOCOL_FRAME_RX_MAX_SIZE)
        return protocol_frame_parser_reject(parser, byte, PROTOCOL_FRAME_PARSE_OVERFLOW);
    parser->buffer[parser->index++] = byte;

    if (parser->index == 2) {
        if (byte != PROTOCOL_FRAME_HEADER_SECOND) {
            protocol_frame_parser_reset(parser);
            if (byte == PROTOCOL_FRAME_HEADER_FIRST) protocol_frame_parser_start(parser);
        }
        return PROTOCOL_FRAME_PARSE_INCOMPLETE;
    }
    if (parser->index == 3) {
        /* A legal image LEN can wrap to 0..4. Its command and source are
         * checked below before accepting any extended receive length. */
        parser->expected_length = byte;
        parser->remaining = (int)byte - 3;
        return PROTOCOL_FRAME_PARSE_INCOMPLETE;
    }
    if (parser->index == 4 && parser->expected_length < PROTOCOL_FRAME_MIN_SIZE) {
        if (byte != 0x47 || parser->image_source == 0)
            return protocol_frame_parser_reject(parser, byte, PROTOCOL_FRAME_PARSE_INVALID_LENGTH);
        return PROTOCOL_FRAME_PARSE_INCOMPLETE;
    }

    if (parser->index == 5) {
        if (parser->buffer[3] == 0x47 && parser->image_source != 0) {
            uint16_t sample_bytes = image_data_reply_scanline_bytes(
                byte, parser->image_width, parser->image_height);
            uint16_t image_length = sample_bytes ? (uint16_t)(sample_bytes + 8U) : 0U;
            if (image_length && parser->buffer[2] != (uint8_t)image_length)
                return protocol_frame_parser_reject(parser, byte, PROTOCOL_FRAME_PARSE_INVALID_LENGTH);
            if (image_length) {
                parser->expected_length = image_length;
                /* Do not forward another source or reinterpret its pixels as
                 * ordinary replies while a different header is active. */
                parser->discard_frame = byte != parser->image_source;
            }
        }
        if (parser->expected_length < PROTOCOL_FRAME_MIN_SIZE)
            return protocol_frame_parser_reject(parser, byte, PROTOCOL_FRAME_PARSE_INVALID_LENGTH);
    }
    parser->remaining = (int)parser->expected_length - parser->index;
    if (parser->remaining == 0) {
        if (!protocol_frame_is_valid(parser->buffer, (size_t)parser->index))
            return protocol_frame_parser_reject(parser, byte, PROTOCOL_FRAME_PARSE_INVALID_FRAME);
        if (parser->discard_frame) {
            protocol_frame_parser_reset_current(parser);
            return PROTOCOL_FRAME_PARSE_INCOMPLETE;
        }
        protocol_frame_parser_track_image(parser);
        frame->data = parser->buffer;
        frame->len = (uint16_t)parser->index;
        protocol_frame_parser_reset_current(parser);
        return PROTOCOL_FRAME_PARSE_READY;
    }
    if (parser->remaining < 0)
        return protocol_frame_parser_reject(parser, byte, PROTOCOL_FRAME_PARSE_INVALID_LENGTH);
    if (parser->index >= (int)PROTOCOL_FRAME_RX_MAX_SIZE)
        return protocol_frame_parser_reject(parser, byte, PROTOCOL_FRAME_PARSE_OVERFLOW);
    return PROTOCOL_FRAME_PARSE_INCOMPLETE;
}
