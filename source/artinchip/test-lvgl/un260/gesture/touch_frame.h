#ifndef UN260_TOUCH_FRAME_H
#define UN260_TOUCH_FRAME_H
#include <stdbool.h>
#include <stdint.h>
#include <linux/input.h>
#define TOUCH_SLOTS 16
typedef struct { int x, y, id; bool active, has_x, has_y; } touch_slot_t;
typedef struct {
    touch_slot_t slots[TOUCH_SLOTS];
    touch_slot_t contacts[TOUCH_SLOTS];
    int slot, count, x, y, packet_count;
    bool type_b, mt, dropped, recovery;
} touch_frame_t;
void touch_frame_init(touch_frame_t *f, bool type_b);
/* Returns true only at a complete input frame, never at a partial packet. */
bool touch_frame_feed(touch_frame_t *f, const struct input_event *e);
#endif
