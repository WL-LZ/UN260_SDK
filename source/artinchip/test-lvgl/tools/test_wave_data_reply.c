#include "un260/protocol/protocol_frame.h"
#include "un260/protocol/protocol_frame_parser.h"
#include "un260/protocol/stream_data_reply.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void assert_invalid(const uint8_t *payload, uint16_t len)
{
    wave_data_reply_view_t reply = wave_data_reply_parse(payload, len);
    assert(reply.kind == WAVE_DATA_REPLY_INVALID);
    assert(reply.values == NULL && reply.value_count == 0);
}

static void typed_payloads(void)
{
    uint8_t payload[250];
    assert(WAVE_DATA_REPLY_CHANNEL_COUNT == 10U);
    for (uint8_t type = 1; type <= WAVE_DATA_REPLY_CHANNEL_COUNT; ++type) {
        payload[0] = type;
        for (unsigned i = 1; i < sizeof(payload); ++i) payload[i] = (uint8_t)i;
        payload[1] = 0xFF;
        payload[2] = 0xFD;
        payload[3] = 0xDF;
        wave_data_reply_view_t reply = wave_data_reply_parse(payload, sizeof(payload));
        assert(reply.kind == WAVE_DATA_REPLY_CHANNEL && reply.channel == type);
        assert(reply.values == payload + 1 && reply.value_count == sizeof(payload) - 1);
        assert(reply.values[0] == 0xFF && reply.values[1] == 0xFD && reply.values[2] == 0xDF);
        payload[1] = 37;
        assert(reply.values[0] == 37); /* Borrowed storage, copied by its consumer. */
        assert_invalid(payload, 1); /* A type without samples is not a data packet. */
        reply = wave_data_reply_parse(payload, 2);
        assert(reply.kind == WAVE_DATA_REPLY_CHANNEL && reply.value_count == 1);
    }

    const uint8_t end[] = {0xFF};
    const uint8_t no_data[] = {0x00};
    const uint8_t bad_end[] = {0xFF, 0x01};
    const uint8_t bad_no_data[] = {0x00, 0x01};
    const uint8_t unknown[] = {0x0B, 0xFF};
    const uint8_t invalid_type[] = {0xFE, 0x01};
    wave_data_reply_view_t reply = wave_data_reply_parse(end, sizeof(end));
    assert(reply.kind == WAVE_DATA_REPLY_END && !reply.values && !reply.value_count);
    reply = wave_data_reply_parse(no_data, sizeof(no_data));
    assert(reply.kind == WAVE_DATA_REPLY_NO_DATA && !reply.values && !reply.value_count);
    assert_invalid(NULL, 0);
    assert_invalid(NULL, 2);
    assert_invalid(end, 0);
    assert_invalid(bad_end, sizeof(bad_end));
    assert_invalid(bad_no_data, sizeof(bad_no_data));
    assert_invalid(unknown, sizeof(unknown));
    assert_invalid(invalid_type, sizeof(invalid_type));
}

static unsigned feed_wave(protocol_frame_parser_t *parser, const uint8_t *data,
                          uint16_t len, wave_data_reply_kind_t kind, uint8_t type)
{
    unsigned delivered = 0;
    for (uint16_t i = 0; i < len; ++i) {
        protocol_frame_view_t frame;
        protocol_frame_parse_result_t result = protocol_frame_parser_feed(parser, data[i], &frame);
        if (i + 1U != len) {
            assert(result == PROTOCOL_FRAME_PARSE_INCOMPLETE);
            continue;
        }
        assert(result == PROTOCOL_FRAME_PARSE_READY && frame.len == len);
        stream_data_reply_view_t stream = stream_data_reply_parse(frame.data, frame.len);
        assert(stream.kind == STREAM_DATA_REPLY_WAVE && stream.payload_len == len - 5U);
        wave_data_reply_view_t wave = wave_data_reply_parse(stream.payload, stream.payload_len);
        assert(wave.kind == kind);
        if (kind == WAVE_DATA_REPLY_CHANNEL) {
            assert(wave.channel == type && wave.value_count == len - 6U);
            assert(memcmp(wave.values, data + 5, wave.value_count) == 0);
        }
        ++delivered;
    }
    return delivered;
}

static void full_frame_boundaries(void)
{
    protocol_frame_parser_t parser;
    protocol_frame_parser_init(&parser);
    uint8_t data[255] = {0xFD, 0xDF, 0, 0x48, 0};
    for (unsigned i = 5; i < sizeof(data); ++i) data[i] = (uint8_t)i;
    const uint8_t embedded_end[] = {0xFD, 0xDF, 0x06, 0x48, 0xFF, 0x0A};
    memcpy(data + 70, embedded_end, sizeof(embedded_end));
    data[5] = 0xFF;
    for (uint8_t type = 1; type <= WAVE_DATA_REPLY_CHANNEL_COUNT; ++type) {
        data[4] = type;
        data[2] = 250;
        data[249] = 0x0A;
        assert(feed_wave(&parser, data, 250, WAVE_DATA_REPLY_CHANNEL, type) == 1);
        data[2] = sizeof(data);
        data[254] = 0x0A;
        assert(feed_wave(&parser, data, sizeof(data), WAVE_DATA_REPLY_CHANNEL, type) == 1);

        uint8_t request[6];
        int built = protocol_frame_build(request, sizeof(request), 0x48, &type, 1);
        assert(built == (int)sizeof(request) && request[4] == type);
        assert(protocol_frame_is_valid(request, sizeof(request)));
    }
    const uint8_t end[] = {0xFD, 0xDF, 0x06, 0x48, 0xFF, 0x0A};
    const uint8_t no_data[] = {0xFD, 0xDF, 0x06, 0x48, 0x00, 0x0A};
    const uint8_t invalid_end[] = {0xFD, 0xDF, 0x07, 0x48, 0xFF, 0x01, 0x0A};
    assert(feed_wave(&parser, end, sizeof(end), WAVE_DATA_REPLY_END, 0) == 1);
    assert(feed_wave(&parser, no_data, sizeof(no_data), WAVE_DATA_REPLY_NO_DATA, 0) == 1);
    assert(feed_wave(&parser, invalid_end, sizeof(invalid_end), WAVE_DATA_REPLY_INVALID, 0) == 1);

    uint8_t invalid[256] = {0xFD, 0xDF, 0x00, 0x48, 0x08};
    assert(stream_data_reply_parse(invalid, sizeof(invalid)).kind == STREAM_DATA_REPLY_INVALID);
    assert(stream_data_reply_parse(NULL, 6).kind == STREAM_DATA_REPLY_INVALID);
    assert(stream_data_reply_parse(end, 5).kind == STREAM_DATA_REPLY_INVALID);
    memcpy(invalid, end, sizeof(end));
    invalid[0] = 0;
    assert(stream_data_reply_parse(invalid, 6).kind == STREAM_DATA_REPLY_INVALID);
    invalid[0] = 0xFD;
    invalid[2] = 7;
    assert(stream_data_reply_parse(invalid, 6).kind == STREAM_DATA_REPLY_INVALID);
    invalid[2] = 6;
    invalid[3] = 0x49;
    assert(stream_data_reply_parse(invalid, 6).kind == STREAM_DATA_REPLY_INVALID);
}

int main(void)
{
    typed_payloads();
    full_frame_boundaries();
    puts("wave typed parser ten types/control/byte samples/frame boundaries passed");
    return 0;
}
