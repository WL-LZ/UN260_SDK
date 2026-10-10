#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "un260/protocol/stream_data_reply.h"

static void test_observed_wire_frames(void)
{
    uint8_t header[] = {0xFD,0xDF,11,0x47,0,1,1,144,0,200,0x83};
    uint8_t line[408] = {0xFD,0xDF,0x98,0x47,1,1,144};
    uint8_t end[] = {0xFD,0xDF,6,0x47,0xFF,0x4C};
    stream_data_reply_view_t stream;
    image_data_reply_view_t reply;

    for (uint8_t id = 1; id <= 6; ++id) {
        header[5] = id;
        stream = stream_data_reply_parse(header, sizeof(header));
        assert(stream.kind == STREAM_DATA_REPLY_IMAGE && stream.payload_len == 6);
        reply = image_data_reply_parse(stream.payload, stream.payload_len);
        assert(reply.kind == IMAGE_DATA_REPLY_HEADER && reply.image_id == id);
        assert(reply.width == 400 && reply.height == 200);
        assert(reply.samples == NULL && reply.sample_count == 0 && reply.scanline == 0);
        assert(image_data_reply_scanline_bytes(id, reply.width, reply.height) == 400);
        assert(image_data_reply_scanline_count(id, reply.width, reply.height) == (id <= 2 ? 400 : 200));

        line[4] = id;
        line[5] = id <= 2 ? 1 : 0;
        line[6] = id <= 2 ? 144 : 200;
        for (uint16_t i = 0; i < 400; ++i) line[7 + i] = (uint8_t)(i * 7U + 11U);
        line[407] = 0x83;
        stream = stream_data_reply_parse(line, sizeof(line));
        assert(stream.kind == STREAM_DATA_REPLY_IMAGE && stream.payload_len == 403);
        reply = image_data_reply_parse(stream.payload, stream.payload_len);
        assert(reply.kind == IMAGE_DATA_REPLY_SCANLINE && reply.image_id == id);
        assert(reply.scanline == (id <= 2 ? 400 : 200) && reply.sample_count == 400);
        assert(reply.samples == &line[7] && reply.width == 0 && reply.height == 0);
        for (uint16_t i = 0; i < reply.sample_count; ++i) {
            assert(reply.samples[i] == (uint8_t)(i * 7U + 11U));
        }
        line[7] = 0xC7;
        assert(reply.samples[0] == 0xC7); /* The view borrows storage. */
        assert(reply.samples[399] == (uint8_t)(399U * 7U + 11U));
    }

    stream = stream_data_reply_parse(end, sizeof(end));
    reply = image_data_reply_parse(stream.payload, stream.payload_len);
    assert(reply.kind == IMAGE_DATA_REPLY_END && reply.samples == NULL);
    /* Length folding is specific to a long 0x47 receive frame. */
    line[2] = 0x99;
    assert(stream_data_reply_parse(line, sizeof(line)).kind == STREAM_DATA_REPLY_INVALID);
    line[2] = 0x98; line[3] = 0x48;
    assert(stream_data_reply_parse(line, sizeof(line)).kind == STREAM_DATA_REPLY_INVALID);
    assert(image_data_reply_scanline_offset(2, 400, 200, 202) == 80400);
    assert(image_data_reply_scanline_offset(2, 400, 200, 203) == 80800);
    puts("PASS width400/height200: 408-byte 0x47 frames with LEN0x98 and complete 400-byte row-major blocks");
}

static void test_short_and_odd_samples(void)
{
    uint8_t header[] = {0,3,0,3,0,2};
    uint8_t frame[] = {0xFD,0xDF,11,0x47,3,0,2,0x10,0x81,0xFE,0x83};
    image_data_reply_view_t reply = image_data_reply_parse(header, sizeof(header));
    assert(reply.kind == IMAGE_DATA_REPLY_HEADER && reply.width == 3 && reply.height == 2);
    assert(image_data_reply_scanline_bytes(3, 3, 2) == 3);
    assert(image_data_reply_scanline_count(3, 3, 2) == 2);
    stream_data_reply_view_t stream = stream_data_reply_parse(frame, sizeof(frame));
    reply = image_data_reply_parse(stream.payload, stream.payload_len);
    assert(reply.kind == IMAGE_DATA_REPLY_SCANLINE && reply.image_id == 3);
    assert(reply.scanline == 2 && reply.sample_count == 3);
    assert(reply.samples == &frame[7] && reply.samples[2] == 0xFE);
    assert(image_data_reply_scanline_bytes(1, 400, 3) == 400);
    assert(image_data_reply_scanline_count(1, 400, 3) == 6);
    assert(image_data_reply_scanline_bytes(1, 3, 200) == 3);
    assert(image_data_reply_scanline_count(1, 3, 200) == 400);
    puts("PASS ordinary short frames, odd grayscale samples and asymmetric RGB565 geometry");
}

static void test_odd_rgb565_blocks_and_order(void)
{
    const uint8_t expected[] = {0xF8,0,0x07,0xE0,0,0x1F,0xFF,0xFF,0,0,0x84,0x10};
    const uint16_t sequence_order[] = {3,1,4,2};
    uint8_t assembled[sizeof(expected)] = {0};
    uint8_t frame[] = {0xFD,0xDF,11,0x47,1,0,0,0,0,0,0x83};

    assert(image_data_reply_pixel_format(1) == IMAGE_DATA_REPLY_PIXEL_RGB565_BE);
    assert(image_data_reply_scanline_bytes(1, 3, 2) == 3);
    assert(image_data_reply_scanline_count(1, 3, 2) == 4);
    for (size_t index = 0; index < 4; ++index) {
        uint16_t sequence = sequence_order[index];
        uint32_t offset = image_data_reply_scanline_offset(1, 3, 2, sequence);
        assert(offset == (uint32_t)(sequence - 1U) * 3);
        frame[6] = (uint8_t)sequence;
        memcpy(&frame[7], &expected[offset], 3);
        stream_data_reply_view_t stream = stream_data_reply_parse(frame, sizeof(frame));
        image_data_reply_view_t reply = image_data_reply_parse(stream.payload, stream.payload_len);
        assert(reply.kind == IMAGE_DATA_REPLY_SCANLINE && reply.scanline == sequence);
        assert(reply.sample_count == 3);
        memcpy(&assembled[offset], reply.samples, reply.sample_count);
    }
    assert(memcmp(assembled, expected, sizeof(expected)) == 0);
    /* The green pixel straddles blocks 1/2; black straddles blocks 3/4. */
    assert(((uint16_t)assembled[2] << 8 | assembled[3]) == 0x07E0);
    assert(((uint16_t)assembled[8] << 8 | assembled[9]) == 0);
    assert(image_data_reply_scanline_offset(1, 3, 2, 0) == UINT32_MAX);
    assert(image_data_reply_scanline_offset(1, 3, 2, 5) == UINT32_MAX);
    puts("PASS odd-width RGB565 byte pairing across blocks, out-of-order assembly and sequence bounds");
}

static void test_geometry_limits(void)
{
    uint8_t header[] = {0,1,0xFF,0xFF,0xFF,0xFF};
    image_data_reply_view_t reply = image_data_reply_parse(header, sizeof(header));
    assert(reply.kind == IMAGE_DATA_REPLY_HEADER);
    assert(reply.width == 65535 && reply.height == 65535);
    assert(image_data_reply_scanline_bytes(1, reply.width, reply.height) == 0);
    assert(image_data_reply_scanline_count(1, reply.width, reply.height) == 0);
    memset(&header[2], 0, 4);
    reply = image_data_reply_parse(header, sizeof(header));
    assert(reply.kind == IMAGE_DATA_REPLY_HEADER && reply.width == 0 && reply.height == 0);
    assert(image_data_reply_scanline_bytes(1, 0, 200) == 0);
    assert(image_data_reply_scanline_count(3, 400, 0) == 0);
    assert(image_data_reply_scanline_bytes(0, 400, 200) == 0);
    assert(image_data_reply_scanline_bytes(7, 400, 200) == 0);
    assert(image_data_reply_scanline_count(7, 400, 200) == 0);
    assert(image_data_reply_scanline_bytes(1, 513, 384) == 0);
    assert(image_data_reply_scanline_count(3, 512, 385) == 0);
    assert(image_data_reply_scanline_bytes(1, 512, 384) == 512);
    assert(image_data_reply_scanline_count(1, 512, 384) == 768);
    assert(image_data_reply_scanline_offset(1, 512, 384, 768) == 392704);
    assert(image_data_reply_scanline_offset(1, 512, 384, 769) == UINT32_MAX);
    assert(image_data_reply_scanline_bytes(3, 512, 384) == 512);
    assert(image_data_reply_scanline_count(3, 512, 384) == 384);
    assert(image_data_reply_scanline_offset(3, 512, 384, 384) == 196096);
    assert(image_data_reply_scanline_offset(3, 512, 384, 385) == UINT32_MAX);
    assert(image_data_reply_scanline_offset(0, 400, 200, 1) == UINT32_MAX);
    assert(image_data_reply_scanline_offset(1, 0, 200, 1) == UINT32_MAX);
    assert(image_data_reply_scanline_offset(1, 513, 200, 1) == UINT32_MAX);
    assert(image_data_reply_scanline_offset(1, 400, 385, 1) == UINT32_MAX);
    for (uint8_t id = 1; id <= IMAGE_DATA_REPLY_SOURCE_COUNT; ++id) {
        assert(image_data_reply_pixel_format(id) == (id <= 2 ?
            IMAGE_DATA_REPLY_PIXEL_RGB565_BE : IMAGE_DATA_REPLY_PIXEL_GRAY8));
    }
    assert(image_data_reply_pixel_format(0) == IMAGE_DATA_REPLY_PIXEL_INVALID);
    assert(image_data_reply_pixel_format(7) == IMAGE_DATA_REPLY_PIXEL_INVALID);
    assert(image_data_reply_pixel_format(255) == IMAGE_DATA_REPLY_PIXEL_INVALID);
    puts("PASS parsed literal geometry, shared width512/height384 bounds and overflow-safe scanline layout");
}

static void test_invalid_payloads(void)
{
    uint8_t header[] = {0,1,1,144,0,200,0};
    uint8_t line[] = {1,1,144,0x80};
    uint8_t end[] = {0xFF,0};

    assert(image_data_reply_parse(NULL, 0).kind == IMAGE_DATA_REPLY_INVALID);
    assert(image_data_reply_parse(NULL, 4).kind == IMAGE_DATA_REPLY_INVALID);
    assert(stream_data_reply_parse(NULL, 408).kind == STREAM_DATA_REPLY_INVALID);
    for (uint16_t len = 0; len < 6; ++len) {
        assert(image_data_reply_parse(header, len).kind == IMAGE_DATA_REPLY_INVALID);
    }
    assert(image_data_reply_parse(header, 7).kind == IMAGE_DATA_REPLY_INVALID);
    header[1] = 0;
    assert(image_data_reply_parse(header, 6).kind == IMAGE_DATA_REPLY_INVALID);
    header[1] = 7;
    assert(image_data_reply_parse(header, 6).kind == IMAGE_DATA_REPLY_INVALID);
    for (uint16_t len = 0; len < 4; ++len) {
        assert(image_data_reply_parse(line, len).kind == IMAGE_DATA_REPLY_INVALID);
    }
    line[0] = 7;
    assert(image_data_reply_parse(line, 4).kind == IMAGE_DATA_REPLY_INVALID);
    line[0] = 1; line[1] = 0; line[2] = 0;
    assert(image_data_reply_parse(line, 4).kind == IMAGE_DATA_REPLY_INVALID);
    assert(image_data_reply_parse(end, 2).kind == IMAGE_DATA_REPLY_INVALID);
    assert(image_data_reply_parse(end, 1).kind == IMAGE_DATA_REPLY_END);
    puts("PASS strict header/end lengths, missing pixel bytes, zero scanline and unknown source rejection");
}

static void test_white_banks_and_gray_layout(void)
{
    /* Actual 400x200 capture: packet 1 is right row0; packet2 is left row0.
     * Boundary/corner fixtures are fixed expectations independent of the helper. */
    for (uint8_t id = 1; id <= 2; id++) {
        assert(image_data_reply_pixel_offset(id,400,200,0) == 200);
        assert(image_data_reply_pixel_offset(id,400,200,199) == 399);
        assert(image_data_reply_pixel_offset(id,400,200,200) == 0);
        assert(image_data_reply_pixel_offset(id,400,200,399) == 199);
        assert(image_data_reply_pixel_offset(id,400,200,400) == 600);
        assert(image_data_reply_pixel_offset(id,400,200,79999) == 79799);
        const uint32_t odd_expected[] = {2,3,4,0,1,7,8,9,5,6};
        for (unsigned i=0;i<10;i++)assert(image_data_reply_pixel_offset(id,5,2,i)==odd_expected[i]);
        assert(image_data_reply_pixel_offset(id,400,200,80000)==UINT32_MAX);
    }
    for (uint8_t id=3;id<=6;id++)
        for (unsigned i=0;i<80000;i++)assert(image_data_reply_pixel_offset(id,400,200,i)==i);
    assert(image_data_reply_pixel_offset(0,400,200,0)==UINT32_MAX);
    assert(image_data_reply_pixel_offset(7,400,200,0)==UINT32_MAX);
    assert(image_data_reply_pixel_offset(1,0,200,0)==UINT32_MAX);
    puts("PASS observed white right/left bank boundaries, odd-width pixel pairing and unchanged four gray-source layouts");
}

int main(void)
{
    test_observed_wire_frames();
    test_short_and_odd_samples();
    test_odd_rgb565_blocks_and_order();
    test_geometry_limits();
    test_white_banks_and_gray_layout();
    test_invalid_payloads();
    puts("PASS typed image data reply suite");
    return 0;
}
