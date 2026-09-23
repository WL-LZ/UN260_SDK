#ifndef UN260_UI_SCROLLBAR_H
#define UN260_UI_SCROLLBAR_H
#include "lvgl/lvgl.h"

/* Native vertical rails occupy the last 10 px. Reserve this full gutter in
 * manually positioned content, including a gap between controls and rail. */
#define UI_SCROLLBAR_GUTTER 20
lv_coord_t ui_scrollbar_content_width(lv_obj_t *viewport);

/* Install once after registering the display. New native scroll views inherit
 * the renderer. Custom physics keep ownership of motion and supply metrics. */
void ui_scrollbar_init(lv_disp_t *display);
lv_obj_t *ui_scrollbar_create(lv_obj_t *parent, int x, int y, int w, int h);
void ui_scrollbar_update(lv_obj_t *bar, float viewport, float content, float offset);
typedef struct { int arrow, gap, thumb_start, thumb_length; } ui_scrollbar_geometry_t;
bool ui_scrollbar_measure(int length, float viewport, float content, float offset,
                          ui_scrollbar_geometry_t *out);
#endif
