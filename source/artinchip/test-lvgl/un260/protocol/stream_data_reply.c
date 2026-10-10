#include "stream_data_reply.h"

#include <stddef.h>

#include "protocol_frame.h"

stream_data_reply_view_t stream_data_reply_parse(const uint8_t *frame,
                                                 uint16_t frame_len)
{
    stream_data_reply_view_t view = {
        .kind = STREAM_DATA_REPLY_INVALID,
        .payload = NULL,
        .payload_len = 0,
    };

    if (frame == NULL || frame_len <= PROTOCOL_FRAME_MIN_SIZE) {
        return view;
    }
    if (!protocol_frame_is_valid(frame, frame_len)) {
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

wave_data_reply_view_t wave_data_reply_parse(const uint8_t *payload,
                                             uint16_t payload_len)
{
    wave_data_reply_view_t view = { .kind = WAVE_DATA_REPLY_INVALID };

    if (payload == NULL || payload_len == 0) return view;

    if (payload[0] == 0x00) {
        if (payload_len == 1) view.kind = WAVE_DATA_REPLY_NO_DATA;
        return view;
    }
    if (payload[0] > WAVE_DATA_REPLY_CHANNEL_COUNT || payload_len < 2) {
        return view;
    }

    view.kind = WAVE_DATA_REPLY_CHANNEL;
    view.channel = payload[0];
    view.values = &payload[1];
    view.value_count = (uint16_t)(payload_len - 1);
    return view;
}

image_data_reply_view_t image_data_reply_parse(const uint8_t *payload,
                                               uint16_t payload_len)
{
    image_data_reply_view_t view = { .kind = IMAGE_DATA_REPLY_INVALID };

    if (payload == NULL || payload_len == 0) return view;

    if (payload[0] == 0x00) {
        if (payload_len != 6 || payload[1] == 0 ||
            payload[1] > IMAGE_DATA_REPLY_SOURCE_COUNT) return view;
        view.kind = IMAGE_DATA_REPLY_HEADER;
        view.image_id = payload[1];
        view.width = (uint16_t)(((uint16_t)payload[2] << 8) | payload[3]);
        view.height = (uint16_t)(((uint16_t)payload[4] << 8) | payload[5]);
        return view;
    }
    if (payload[0] == 0xFF) {
        if (payload_len == 1) view.kind = IMAGE_DATA_REPLY_END;
        return view;
    }
    if (payload[0] > IMAGE_DATA_REPLY_SOURCE_COUNT || payload_len < 4) {
        return view;
    }

    view.scanline = (uint16_t)(((uint16_t)payload[1] << 8) | payload[2]);
    if (view.scanline == 0) return view;
    view.kind = IMAGE_DATA_REPLY_SCANLINE;
    view.image_id = payload[0];
    view.samples = &payload[3];
    view.sample_count = (uint16_t)(payload_len - 3);
    return view;
}

static bool image_data_reply_geometry_supported(uint8_t image_id,
                                                uint16_t width, uint16_t height)
{
    return image_id >= 1 && image_id <= IMAGE_DATA_REPLY_SOURCE_COUNT &&
        width > 0 && width <= IMAGE_DATA_REPLY_MAX_WIDTH &&
        height > 0 && height <= IMAGE_DATA_REPLY_MAX_HEIGHT;
}

image_data_reply_pixel_format_t image_data_reply_pixel_format(uint8_t image_id)
{
    if (image_id == 0 || image_id > IMAGE_DATA_REPLY_SOURCE_COUNT) {
        return IMAGE_DATA_REPLY_PIXEL_INVALID;
    }
    return image_id <= IMAGE_DATA_REPLY_RGB565_SOURCE_COUNT ?
        IMAGE_DATA_REPLY_PIXEL_RGB565_BE : IMAGE_DATA_REPLY_PIXEL_GRAY8;
}

uint16_t image_data_reply_scanline_bytes(uint8_t image_id,
                                        uint16_t width, uint16_t height)
{
    if (!image_data_reply_geometry_supported(image_id, width, height)) return 0;
    return width;
}

uint16_t image_data_reply_scanline_count(uint8_t image_id,
                                        uint16_t width, uint16_t height)
{
    if (!image_data_reply_geometry_supported(image_id, width, height)) return 0;
    return image_data_reply_pixel_format(image_id) == IMAGE_DATA_REPLY_PIXEL_RGB565_BE ?
        (uint16_t)(height * 2U) : height;
}

uint32_t image_data_reply_scanline_offset(uint8_t image_id,
                                         uint16_t width, uint16_t height,
                                         uint16_t sequence)
{
    uint16_t count = image_data_reply_scanline_count(image_id, width, height);
    if (sequence == 0 || sequence > count) return UINT32_MAX;
    return (uint32_t)(sequence - 1U) * width;
}

uint32_t image_data_reply_pixel_offset(uint8_t image_id,
                                       uint16_t width, uint16_t height,
                                       uint32_t wire_pixel)
{
    if (!image_data_reply_geometry_supported(image_id, width, height) ||
        wire_pixel >= (uint32_t)width * height) return UINT32_MAX;
    if (image_data_reply_pixel_format(image_id) != IMAGE_DATA_REPLY_PIXEL_RGB565_BE)
        return wire_pixel;
    uint32_t row = wire_pixel / width;
    /* The controller starts with the right bank (ceil pixels), followed by
     * the left bank (floor pixels), even when a byte block splits an RGB word. */
    uint32_t column = (wire_pixel % width + width / 2U) % width;
    return row * width + column;
}
