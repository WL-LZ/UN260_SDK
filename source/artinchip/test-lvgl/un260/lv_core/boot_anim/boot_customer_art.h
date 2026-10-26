#ifndef BOOT_CUSTOMER_ART_H
#define BOOT_CUSTOMER_ART_H
#include "lvgl/lvgl.h"
#include "un260/boot/boot_customer_catalog.h"
/* UI-thread only. First acquire inflates once; overlapping intro/selfcheck
 * owners share immutable descriptors until the last release. */
bool boot_customer_art_acquire(const char *language_tag);
const lv_img_dsc_t *boot_customer_art_image(unsigned id);
bool boot_customer_art_position(unsigned id, lv_coord_t *x, lv_coord_t *y);
void boot_customer_art_release(void);
#endif
