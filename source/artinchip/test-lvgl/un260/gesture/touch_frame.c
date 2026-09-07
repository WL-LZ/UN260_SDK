#include "touch_frame.h"
#include <string.h>
void touch_frame_init(touch_frame_t *f, bool type_b)
{
    memset(f, 0, sizeof(*f));
    f->type_b = type_b;
}
bool touch_frame_feed(touch_frame_t *f, const struct input_event *e)
{
    if(e->type == EV_SYN && e->code == SYN_DROPPED) {
        f->dropped = true;
        f->recovery = true;
        memset(f->slots, 0, sizeof(f->slots));
        f->count = 0;
        return false;
    }
    if(f->dropped) {
        if(e->type == EV_SYN && e->code == SYN_REPORT) {
            f->dropped = false;
            return true; /* Cancel the old contact; require all fingers up. */
        }
        return false;
    }
    if(f->recovery) {
        if(e->type == EV_KEY && e->code == BTN_TOUCH && e->value == 0) {
            f->recovery = false;
            f->slot = 0;
        }
        return e->type == EV_SYN && e->code == SYN_REPORT;
    }
    if(e->type == EV_ABS && e->code == ABS_MT_SLOT) {
        f->mt = true;
        f->slot = (e->value >= 0 && e->value < TOUCH_SLOTS) ? e->value : -1;
    } else if(e->type == EV_ABS &&
              (e->code == ABS_MT_TRACKING_ID || e->code == ABS_MT_POSITION_X ||
               e->code == ABS_MT_POSITION_Y)) {
        f->mt = true;
        if(f->slot >= 0 && f->slot < TOUCH_SLOTS) {
            touch_slot_t *s = &f->slots[f->slot];
            if(e->code == ABS_MT_TRACKING_ID) {
                /* Type-B position values survive tracking-ID changes. Linux
                 * suppresses unchanged ABS values on a second tap at the same
                 * coordinate, so clearing has_x/has_y would lose that tap. */
                s->id = e->value;
                s->active = e->value >= 0;
            } else if(e->code == ABS_MT_POSITION_X) {
                s->x = e->value; s->has_x = true;
                if(!f->type_b) s->active = true;
            } else {
                s->y = e->value; s->has_y = true;
                if(!f->type_b) s->active = true;
            }
        }
    } else if(e->type == EV_SYN && e->code == SYN_MT_REPORT && !f->type_b) {
        f->packet_count++;
        f->slot = f->packet_count < TOUCH_SLOTS ? f->packet_count : -1;
    } else if(e->type == EV_KEY && e->code == BTN_TOUCH && e->value == 0) {
        for(int i = 0; i < TOUCH_SLOTS; ++i) f->slots[i].active = false;
    }
    if(e->type != EV_SYN || e->code != SYN_REPORT) return false;
    int x = 0, y = 0;
    f->count = 0;
    for(int i = 0; i < TOUCH_SLOTS; ++i) {
        touch_slot_t *s = &f->slots[i];
        if(s->active && s->has_x && s->has_y) {
            f->contacts[f->count] = *s;
            if(!f->type_b) f->contacts[f->count].id = i;
            f->count++; x += s->x; y += s->y;
        }
    }
    if(f->count) { f->x = x / f->count; f->y = y / f->count; }
    if(!f->type_b) {
        memset(f->slots, 0, sizeof(f->slots));
        f->slot = f->packet_count = 0;
    }
    return true;
}
