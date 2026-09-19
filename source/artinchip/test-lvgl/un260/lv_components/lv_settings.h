#ifndef LV_SETTINGS_H
#define LV_SETTINGS_H
#include "lvgl/lvgl.h"

/* Presentation only. Owners provide strings, actions and confirmed values. */
typedef struct {
    const char *title, *subtitle, *icon;
    lv_event_cb_t back;
    void *user_data;
} lv_settings_header_t;
typedef struct {
    const char *title, *hint, *value, *icon;
    lv_event_cb_t activate;
    void *user_data;
    bool disabled;
    /* Optional output; owner may refresh a value without depending on child order.
     * Pass value="" to reserve a live value label. NULL omits that field. */
    lv_obj_t **value_label;
} lv_settings_item_t;
typedef struct {
    lv_obj_t *root, *body, *footer, *back, *message;
} lv_settings_frame_t;
lv_obj_t *lv_settings_box(lv_obj_t *, int x, int y, int w, int h, uint32_t color);
lv_obj_t *lv_settings_label(lv_obj_t *, const char *, int x, int y,
                             const lv_font_t *, uint32_t color);
lv_obj_t *lv_settings_icon(lv_obj_t *, const char *name, int x, int y);
lv_obj_t *lv_settings_button(lv_obj_t *, int x, int y, int w, int h,
                             const char *, bool primary, lv_event_cb_t, void *);
/* Settings-only Back palette. Navigation marker and gesture semantics retained. */
lv_obj_t *lv_settings_back(lv_obj_t *, int x, int y, int w, int h,
                           lv_event_cb_t, void *);
/* Same surface language, available to function-specific detail compositions. */
lv_obj_t *lv_settings_panel(lv_obj_t *, int x, int y, int w, int h);
lv_obj_t *lv_settings_header(lv_obj_t *, int x, int y, int w,
                             const lv_settings_header_t *);
lv_settings_frame_t lv_settings_frame_create(lv_obj_t *, const lv_settings_header_t *);
/* Two-column native flex layout. Adding/deleting/hiding cards reflows automatically. */
lv_obj_t *lv_settings_grid(lv_obj_t *, int x, int y, int w, int h);
lv_obj_t *lv_settings_item(lv_obj_t *grid, const lv_settings_item_t *);
#endif
