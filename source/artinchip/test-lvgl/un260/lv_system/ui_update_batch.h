#ifndef UI_UPDATE_BATCH_H
#define UI_UPDATE_BATCH_H

#include <stdbool.h>

#include "lvgl/lvgl.h"

typedef enum {
    /* The owner has already invalidated the complete visual region, as LVGL
     * does before dispatching a scroll event. Child updates need no second
     * invalidation in this case. */
    UI_UPDATE_BATCH_COVERED_BY_PARENT = 0,
    /* General batch mode: invalidate root once after all child mutations. */
    UI_UPDATE_BATCH_INVALIDATE_ROOT,
} ui_update_batch_mode_t;

typedef struct {
    lv_disp_t *disp;
    lv_obj_t *root;
    ui_update_batch_mode_t mode;
    bool owns_invalidation_pause;
} ui_update_batch_t;

void ui_update_batch_begin(ui_update_batch_t *batch, lv_obj_t *root,
                           ui_update_batch_mode_t mode);
void ui_update_batch_end(ui_update_batch_t *batch);

#endif
