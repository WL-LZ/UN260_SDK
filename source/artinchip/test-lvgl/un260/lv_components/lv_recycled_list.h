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

/* Generic read-only viewport: vertical scroll with bounded edge elasticity;
 * paged mode accepts a horizontal swipe on normal release, one page per drag.
 * Direction locking and PRESS_LOST cancellation leave global input ownership
 * with the platform. The scrollbar is 40% opaque inside the range, 100% at edges.
 * Owns only a bounded row pool and an idle-paused motion timer.
 * Caller owns data; callbacks never perform protocol/IO.
 * Deleting its parent destroys the viewport, timer and private allocation.
 * Fixed pixel dimensions, including one extra row and edge elasticity,
 * must fit LV_COORD_MAX. A create_row failure returns NULL and cleans up the
 * partial viewport. Timer allocation failure disables release animation and
 * settles any visual stretch immediately; logical offsets always stay valid. */
lv_recycled_list_t *lv_recycled_list_create(lv_obj_t *parent,
                                            const lv_recycled_list_config_t *config);
lv_obj_t *lv_recycled_list_object(lv_recycled_list_t *list);
const ui_list_window_t *lv_recycled_list_window(const lv_recycled_list_t *list);
/* Data changes preserve an active press and reading position, rebasing when
 * the valid range or stretched edge changes. reset stops the current motion. */
void lv_recycled_list_refresh(lv_recycled_list_t *list, uint32_t count, bool reset);
void lv_recycled_list_set_paged(lv_recycled_list_t *list, bool paged);
void lv_recycled_list_page_step(lv_recycled_list_t *list, int step);
/* Locate a zero-based data index without changing count or navigation mode.
 * Scroll mode puts it at the top where possible, clamped at the last viewport;
 * page mode selects its containing page. A valid request cancels held input,
 * inertia and edge stretch, then refreshes rows, scrollbar and range callback.
 * NULL, empty lists and out-of-range indices return false without changes. */
bool lv_recycled_list_scroll_to_index(lv_recycled_list_t *list, uint32_t index);
/* Hit-test a display-coordinate point against currently visible bound rows,
 * including edge stretch and partial rows, clipped to the viewport. Updates
 * pending LVGL layout for UI-event use; does not stop motion or change range.
 * The scrollbar gutter, blank/hidden rows and invalid arguments return false
 * and leave out_index unchanged. */
bool lv_recycled_list_index_at_point(lv_recycled_list_t *list,
                                     const lv_point_t *point, uint32_t *out_index);
/* Query after RELEASED, in a callback registered after the viewport's own
 * handler. A stationary touch is eligible only if it did not stop inertia or
 * edge return. Dragging, PRESS_LOST, stop/reset and duplicate releases cancel
 * eligibility; a fresh PRESSED starts a new contact. Caller still validates
 * the hit row and data revision. No navigation or selection is performed. */
bool lv_recycled_list_tap_allowed(const lv_recycled_list_t *list);
/* Owner hide/reset/gesture cancellation clears any stretch and pauses motion. */
void lv_recycled_list_stop(lv_recycled_list_t *list);
#endif
