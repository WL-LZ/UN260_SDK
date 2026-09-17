#ifndef UN260_UI_SCROLLBAR_H
#define UN260_UI_SCROLLBAR_H
#include "lvgl/lvgl.h"

/* Install once after registering the display. New native scroll views inherit
 * the renderer. Custom physics keep ownership of motion and supply metrics. */
void ui_scrollbar_init(lv_disp_t *display);
lv_obj_t *ui_scrollbar_create(lv_obj_t *parent, int x, int y, int w, int h);
void ui_scrollbar_update(lv_obj_t *bar, float viewport, float content, float offset);
typedef struct { int arrow, gap, thumb_start, thumb_length; } ui_scrollbar_geometry_t;
bool ui_scrollbar_measure(int length, float viewport, float content, float offset,
                          ui_scrollbar_geometry_t *out);
#endif
