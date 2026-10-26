#ifndef BOOT_CUSTOMER_CATALOG_H
#define BOOT_CUSTOMER_CATALOG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    BOOT_CUSTOMER_ART_BG=1, BOOT_CUSTOMER_ART_LOGO_LARGE,
    BOOT_CUSTOMER_ART_LOGO_SMALL, BOOT_CUSTOMER_ART_HAND_1,
    BOOT_CUSTOMER_ART_HAND_2, BOOT_CUSTOMER_ART_HAND_3,
    BOOT_CUSTOMER_ART_HAND_4, BOOT_CUSTOMER_ART_HAND_5,
    BOOT_CUSTOMER_ART_INIT, BOOT_CUSTOMER_ART_SYSTEM,
    BOOT_CUSTOMER_ART_READY_GRAY, BOOT_CUSTOMER_ART_VERSION,
    BOOT_CUSTOMER_ART_POWERED, BOOT_CUSTOMER_ART_READY_ORANGE
};
enum { BOOT_CUSTOMER_BGRA=1, BOOT_CUSTOMER_A8=2 };
typedef struct {
    const uint8_t *data;
    size_t size;
    unsigned count;
} boot_customer_catalog_t;
typedef struct {
    const uint8_t *data;
    uint32_t width, height, format, color;
    int32_t x, y;
} boot_customer_image_t;
/* Views borrow validated, immutable catalog storage. No allocations or I/O. */
bool boot_customer_catalog_open(boot_customer_catalog_t *, const uint8_t *, size_t);
bool boot_customer_catalog_find(const boot_customer_catalog_t *, unsigned id,
                                const char *tag, boot_customer_image_t *);
#endif
