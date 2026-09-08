#ifndef UN260_PRESENT_DAMAGE_H
#define UN260_PRESENT_DAMAGE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Inclusive framebuffer coordinates. No LVGL/global state: host-testable. */
typedef struct { int32_t x1, y1, x2, y2; } present_rect_t;
#define PRESENT_DAMAGE_CAPACITY 64U
/* Compute previous damage minus the areas guaranteed to be redrawn now.
 * On fragmentation/capacity failure return false; caller copies ALL previous
 * damage. Never silently truncate a region. */
bool present_damage_plan(const present_rect_t *previous, size_t previous_count,
                         const present_rect_t *redraw, size_t redraw_count,
                         present_rect_t *out, size_t capacity, size_t *count);
uint64_t present_damage_pixels(const present_rect_t *rects, size_t count);
#endif
