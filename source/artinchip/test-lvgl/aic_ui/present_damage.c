#include "present_damage.h"
#include <string.h>

uint64_t present_damage_pixels(const present_rect_t *rects, size_t count)
{
    uint64_t pixels = 0;
    for (size_t i = 0; i < count; ++i)
        pixels += (uint64_t)(rects[i].x2 - rects[i].x1 + 1) *
                  (uint64_t)(rects[i].y2 - rects[i].y1 + 1);
    return pixels;
}

static bool append(present_rect_t *out, size_t capacity, size_t *count,
                   int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    if (x1 > x2 || y1 > y2) return true;
    if (*count == capacity) return false;
    out[(*count)++] = (present_rect_t){x1, y1, x2, y2};
    return true;
}

bool present_damage_plan(const present_rect_t *previous, size_t previous_count,
                         const present_rect_t *redraw, size_t redraw_count,
                         present_rect_t *out, size_t capacity, size_t *count)
{
    present_rect_t next[PRESENT_DAMAGE_CAPACITY];
    if (!out || !count || capacity > PRESENT_DAMAGE_CAPACITY ||
        previous_count > capacity) return false;
    *count = previous_count;
    if (previous_count) memcpy(out, previous, previous_count * sizeof(*out));
    for (size_t r = 0; r < redraw_count; ++r) {
        size_t n = 0;
        for (size_t i = 0; i < *count; ++i) {
            present_rect_t a = out[i], b = redraw[r];
            int32_t x1 = a.x1 > b.x1 ? a.x1 : b.x1;
            int32_t y1 = a.y1 > b.y1 ? a.y1 : b.y1;
            int32_t x2 = a.x2 < b.x2 ? a.x2 : b.x2;
            int32_t y2 = a.y2 < b.y2 ? a.y2 : b.y2;
            if (x1 > x2 || y1 > y2) {
                if (!append(next, capacity, &n, a.x1, a.y1, a.x2, a.y2)) return false;
            } else if (!append(next, capacity, &n, a.x1, a.y1, a.x2, y1-1) ||
                       !append(next, capacity, &n, a.x1, y2+1, a.x2, a.y2) ||
                       !append(next, capacity, &n, a.x1, y1, x1-1, y2) ||
                       !append(next, capacity, &n, x2+1, y1, a.x2, y2)) return false;
        }
        *count = n;
        memcpy(out, next, n * sizeof(*out));
    }
    return true;
}
