#ifndef UN260_LV_CARD_SURFACE_H
#define UN260_LV_CARD_SURFACE_H

#include <stdint.h>
#include "lvgl/lvgl.h"

/* One card footprint for its background, inside border and cached image.
 * Colors are 0xRRGGBB. The owner retains content, position and input policy. */
typedef struct {
    int16_t width;
    int16_t height;
    int16_t radius;
    uint32_t background;
    uint32_t border;
    uint8_t border_width;
} lv_card_surface_style_t;

/* UI-thread only. Applies the default main-part skin to an existing object;
 * does not allocate objects, remove styles/events, move or reparent it, or
 * change its scrolling/clicking flags. Non-positive dimensions are ignored. */
void lv_card_surface_apply(lv_obj_t *surface,
                           const lv_card_surface_style_t *style);

/* Optional decorative focus cue inside a card, never a selection command.
 * The owner creates one child object; this helper allocates nothing and
 * does not change its parent/events. The 28x3 graphite capsule is centered
 * 11px above the parent's bottom content edge (excluding its border),
 * and hidden entirely in the normal state. */
void lv_card_surface_focus_mark_apply(lv_obj_t *mark, bool focused);

#endif
