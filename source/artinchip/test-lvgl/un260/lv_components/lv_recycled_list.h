#ifndef LV_RECYCLED_LIST_H
#define LV_RECYCLED_LIST_H
#include "lvgl/lvgl.h"
#include "ui_list_window.h"

typedef struct lv_recycled_list lv_recycled_list_t;
typedef lv_obj_t *(*lv_recycled_list_create_row_fn)(lv_obj_t *parent,
                                                   lv_coord_t width, void *context);
typedef void (*lv_recycled_list_bind_row_fn)(lv_obj_t *row, uint32_t index,
                                            void *context);
typedef void (*lv_recycled_list_changed_fn)(const ui_list_window_t *window,
                                           void *context);
typedef struct {
    lv_coord_t x, y, width;
    uint16_t rows, row_height;
    lv_recycled_list_create_row_fn create_row;
    lv_recycled_list_bind_row_fn bind_row;
    lv_recycled_list_changed_fn changed;
    void *context;
} lv_recycled_list_config_t;

/* Generic vertical viewport. Owns only a bounded row pool and a paused-when-
 * idle motion timer. Caller owns data; callbacks never perform protocol/IO.
 * Deleting its parent destroys the viewport, timer and private allocation.
 * Fixed pixel dimensions, including one recycled row beyond the viewport,
 * must fit LV_COORD_MAX. A create_row failure returns NULL and cleans up the
 * partial viewport. Timer allocation failure only disables release inertia. */
lv_recycled_list_t *lv_recycled_list_create(lv_obj_t *parent,
                                            const lv_recycled_list_config_t *config);
lv_obj_t *lv_recycled_list_object(lv_recycled_list_t *list);
const ui_list_window_t *lv_recycled_list_window(const lv_recycled_list_t *list);
/* Data changes preserve an active press and reading position, clamping and
 * rebasing only when rows shrink. reset starts a new result and stops motion. */
void lv_recycled_list_refresh(lv_recycled_list_t *list, uint32_t count, bool reset);
void lv_recycled_list_set_paged(lv_recycled_list_t *list, bool paged);
void lv_recycled_list_page_step(lv_recycled_list_t *list, int step);
void lv_recycled_list_stop(lv_recycled_list_t *list);
#endif
