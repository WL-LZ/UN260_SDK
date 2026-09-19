/* Host replaces DMA allocation only; production skin ownership, events,
 * opacity and sibling rendering run unchanged in actual_skin_presenter.c. */
#include <assert.h>
#include <stdlib.h>
#include "un260/lv_components/lv_dma_snapshot_cache.h"
struct lv_dma_snapshot { lv_img_dsc_t *image; };
unsigned host_skin_captures;
lv_dma_snapshot_t *lv_dma_snapshot_cache_acquire_or_create(lv_obj_t *obj,const char *key)
{
    (void)key;
#ifdef HOST_SKIN_FALLBACK
    (void)obj;return NULL;
#else
    lv_dma_snapshot_t *snapshot=malloc(sizeof(*snapshot));assert(snapshot);
    snapshot->image=lv_snapshot_take(obj,LV_IMG_CF_TRUE_COLOR_ALPHA);
    assert(snapshot->image);++host_skin_captures;return snapshot;
#endif
}
const lv_img_dsc_t *lv_dma_snapshot_image(const lv_dma_snapshot_t *snapshot)
{ return snapshot?snapshot->image:NULL; }
void lv_dma_snapshot_cache_release(lv_dma_snapshot_t *snapshot)
{ if(snapshot) { lv_snapshot_free(snapshot->image);free(snapshot); } }
