#include "un260/lv_system/ui_update_batch.h"

#include <string.h>

void ui_update_batch_begin(ui_update_batch_t *batch, lv_obj_t *root,
                           ui_update_batch_mode_t mode)
{
    if (batch == NULL) return;

    memset(batch, 0, sizeof(*batch));
    if (root == NULL || !lv_obj_is_valid(root)) return;

    batch->root = root;
    batch->mode = mode;
    batch->disp = lv_obj_get_disp(root);
    if (batch->disp == NULL ||
        !lv_disp_is_invalidation_enabled(batch->disp)) {
        return;
    }

    lv_disp_enable_invalidation(batch->disp, false);
    batch->owns_invalidation_pause = true;
}

void ui_update_batch_end(ui_update_batch_t *batch)
{
    lv_obj_t *root;
    ui_update_batch_mode_t mode;

    if (batch == NULL || !batch->owns_invalidation_pause) return;

    root = batch->root;
    mode = batch->mode;
    lv_disp_enable_invalidation(batch->disp, true);
    batch->owns_invalidation_pause = false;

    if (mode == UI_UPDATE_BATCH_INVALIDATE_ROOT && root != NULL &&
        lv_obj_is_valid(root)) {
        lv_obj_invalidate(root);
    }
}
