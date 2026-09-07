#ifndef UN260_COMPILED_ASSET_H
#define UN260_COMPILED_ASSET_H
#include <stdint.h>
/* Little-endian ARGB8888 (B,G,R,A bytes), straight alpha, no premultiplication.
 * Read-only C arrays are uploaded lazily into the existing LVGL DMA cache. */
typedef struct {
    const char *name;
    uint32_t width, height, stride;
    uint8_t has_alpha;
    const unsigned char *pixels;
} un260_compiled_asset_t;
const un260_compiled_asset_t *un260_compiled_asset_find(const char *path);
unsigned un260_compiled_asset_count(void);
unsigned un260_compiled_asset_bytes(void);
#endif
