#ifndef LV_SETTINGS_H
#define LV_SETTINGS_H
#include "lvgl/lvgl.h"
#include "lv_settings_palette.h"
/* Install before the action callback. A blocked action remains touchable and
 * delegates its explanation to the owning UI layer. NULL reason clears it. */
void lv_settings_action_guard_init(lv_obj_t *button);
void lv_settings_action_block(lv_obj_t *button, const char *reason,
                             void (*explain)(const char *));
const char *lv_settings_action_block_reason(lv_obj_t *button);
typedef enum {
    LV_SETTINGS_ACTION_SECONDARY, LV_SETTINGS_ACTION_PRIMARY,
    LV_SETTINGS_ACTION_DESTRUCTIVE
} lv_settings_action_role_t;
/* Semantic action colors, including pressed/disabled states. */
void lv_settings_action_style(lv_obj_t *button, lv_settings_action_role_t role);

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
    /* Full-width grouped row; default preserves independent card callers. */
    bool grouped;
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
/* Shared gray base / white selection. Pending input never changes selection paint. */
lv_obj_t *lv_settings_segment_base(lv_obj_t *, int x, int y, int w, int h);
lv_obj_t *lv_settings_segment(lv_obj_t *, unsigned index, unsigned count,
                            const char *, lv_event_cb_t, void *);
/* Same surface language, available to function-specific detail compositions. */
lv_obj_t *lv_settings_panel(lv_obj_t *, int x, int y, int w, int h);
lv_obj_t *lv_settings_header(lv_obj_t *, int x, int y, int w,
                             const lv_settings_header_t *);
lv_settings_frame_t lv_settings_frame_create(lv_obj_t *, const lv_settings_header_t *);
/* Covers the current body size, including after later layout changes. */
lv_obj_t *lv_settings_body_overlay(lv_obj_t *body);
/* Standard frame action bar, or NULL for a custom page. */
lv_obj_t *lv_settings_actions(lv_obj_t *root);
/* Two-column native flex layout. Adding/deleting/hiding cards reflows automatically. */
lv_obj_t *lv_settings_grid(lv_obj_t *, int x, int y, int w, int h);
lv_obj_t *lv_settings_item(lv_obj_t *grid, const lv_settings_item_t *);
lv_obj_t *lv_settings_list(lv_obj_t *, int x, int y, int w, int h);
lv_obj_t *lv_settings_group(lv_obj_t *list);
/* Call after changing row visibility; deletion/layout changes also refresh edges. */
void lv_settings_group_refresh(lv_obj_t *group);
/* A separated detail row reserves 8px above/below its control, plus its rule. */
lv_obj_t *lv_settings_control_row(lv_obj_t *, int x, int y, int w,
                                 int control_height, bool separator);
#endif
