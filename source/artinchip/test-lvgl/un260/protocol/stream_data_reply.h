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
                                                 uint16_t frame_len);

#define WAVE_DATA_REPLY_CHANNEL_COUNT 7U

typedef enum {
    WAVE_DATA_REPLY_INVALID = 0,
    WAVE_DATA_REPLY_NO_DATA,
    WAVE_DATA_REPLY_CHANNEL,
} wave_data_reply_kind_t;

typedef struct {
    wave_data_reply_kind_t kind;
    uint8_t channel;
    const uint8_t *values;
    uint16_t value_count;
} wave_data_reply_view_t;

/* Documented 0x48 payloads: 00 means no capture; 01..07 identify channels.
 * Unknown selectors are unsupported and do not terminate a capture.
 * The returned values borrow the payload storage. */
wave_data_reply_view_t wave_data_reply_parse(const uint8_t *payload,
                                             uint16_t payload_len);

#define IMAGE_DATA_REPLY_SOURCE_COUNT 6U
#define IMAGE_DATA_REPLY_RGB565_SOURCE_COUNT 2U
#define IMAGE_DATA_REPLY_MAX_WIDTH 512U
#define IMAGE_DATA_REPLY_MAX_HEIGHT 384U

typedef enum {
    IMAGE_DATA_REPLY_INVALID = 0,
    IMAGE_DATA_REPLY_HEADER,
    IMAGE_DATA_REPLY_SCANLINE,
    IMAGE_DATA_REPLY_END,
} image_data_reply_kind_t;

typedef enum {
    IMAGE_DATA_REPLY_PIXEL_INVALID = 0,
    IMAGE_DATA_REPLY_PIXEL_GRAY8,
    IMAGE_DATA_REPLY_PIXEL_RGB565_BE,
} image_data_reply_pixel_format_t;

typedef struct {
    image_data_reply_kind_t kind;
    uint8_t image_id;
    uint16_t width;
    uint16_t height;
    uint16_t scanline;
    const uint8_t *samples;
    uint16_t sample_count; /* Byte count, before source-specific pixel decoding. */
} image_data_reply_view_t;

/* 0x47: 00,id,L_hi,L_lo,W_hi,W_lo defines image width L and height W.
 * A data payload is id,sequence_hi,sequence_lo followed by pixel bytes;
 * FF alone ends the transfer. Dimensions are validated by the consumer.
 * The returned samples borrow the payload storage. */
image_data_reply_view_t image_data_reply_parse(const uint8_t *payload,
                                               uint16_t payload_len);

/* Pixel words form row-major capture rows; white sources transfer the right
 * bank before the left bank. Grayscale rows retain their wire pixel order. Sources 1/2 are big-endian RGB565;
 * 3..6 are 8-bit grayscale. Unknown sources return INVALID. */
image_data_reply_pixel_format_t image_data_reply_pixel_format(uint8_t image_id);

/* A sequence identifies a width-byte transfer block, not a pixel column.
 * RGB565 uses 2*height blocks and grayscale uses height blocks. A pixel's
 * two bytes may straddle blocks when width is odd. Zero means invalid
 * source/unsupported geometry. */
uint16_t image_data_reply_scanline_bytes(uint8_t image_id,
                                        uint16_t width, uint16_t height);
uint16_t image_data_reply_scanline_count(uint8_t image_id,
                                        uint16_t width, uint16_t height);

/* Byte offset into the complete row-major image; UINT32_MAX means invalid
 * source/geometry/sequence. This remains a byte offset for RGB565. */
uint32_t image_data_reply_scanline_offset(uint8_t image_id,
                                         uint16_t width, uint16_t height,
                                         uint16_t sequence);

/* Native image pixel corresponding to a decoded wire-order pixel word.
 * Left bank has floor(width/2) pixels; right bank has the remaining ceil pixels; only sources 1/2 reorder their two banks.
 * The byte-block offsets above remain wire offsets (including split RGB words). */
uint32_t image_data_reply_pixel_offset(uint8_t image_id,
                                       uint16_t width, uint16_t height,
                                       uint32_t wire_pixel);

#endif
