/* Host replaces DMA allocation only; production skin ownership, events,
 * opacity and sibling rendering run unchanged in actual_skin_presenter.c. */
#include <assert.h>
#include <stdlib.h>
#include "un260/lv_components/lv_dma_snapshot_cache.h"
struct lv_dma_snapshot { lv_img_dsc_t *image; void *external_buffer; size_t capacity; };
unsigned host_skin_captures;
lv_dma_snapshot_t *lv_dma_snapshot_cache_acquire_or_create(lv_obj_t *obj,const char *key)
{
    (void)key;
#ifdef HOST_SKIN_FALLBACK
    (void)obj;return NULL;
#else
    lv_dma_snapshot_t *snapshot=calloc(1,sizeof(*snapshot));assert(snapshot);
    snapshot->image=lv_snapshot_take(obj,LV_IMG_CF_TRUE_COLOR_ALPHA);
    assert(snapshot->image);++host_skin_captures;return snapshot;
#endif
}
const lv_img_dsc_t *lv_dma_snapshot_image(const lv_dma_snapshot_t *snapshot)
{ return snapshot?snapshot->image:NULL; }
void lv_dma_snapshot_cache_release(lv_dma_snapshot_t *snapshot)
{
    if(!snapshot)return;
    if(snapshot->external_buffer) { free(snapshot->external_buffer);free(snapshot->image); }
    else lv_snapshot_free(snapshot->image);
    free(snapshot);
}

/* Only DMA allocation is replaced; the drawer still translates a single real
 * LVGL subtree raster, then returns to the live controls at the end. */
unsigned host_quick_captures;
bool lv_dma_transition_surface_capture(lv_dma_static_surface_t *surface,lv_obj_t *source,const char *name)
{
    (void)name;
#ifdef HOST_SKIN_FALLBACK
    (void)surface;(void)source;return false;
#else
    lv_obj_update_layout(source);
    if(!surface->snapshot) {
        surface->snapshot=calloc(1,sizeof(*surface->snapshot));assert(surface->snapshot);
        surface->snapshot->image=calloc(1,sizeof(lv_img_dsc_t));assert(surface->snapshot->image);
        surface->source=source;
        surface->image=lv_img_create(lv_obj_get_parent(source));
        lv_obj_clear_flag(surface->image,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    }
    /* Production's mutable DMA bitmap is outside LVGL's object heap and is
     * reused. Match that ownership here instead of exhausting the 4 MB test
     * object heap with a second software-only full-width pixel allocation. */
    lv_dma_snapshot_t *snapshot=surface->snapshot;
    size_t bytes=lv_snapshot_buf_size_needed(source,LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_img_cache_invalidate_src(snapshot->image);
    if(snapshot->capacity<bytes) {
        void *buffer=realloc(snapshot->external_buffer,bytes);assert(buffer);
        snapshot->external_buffer=buffer;snapshot->capacity=bytes;
    }
    assert(lv_snapshot_take_to_buf(source,LV_IMG_CF_TRUE_COLOR_ALPHA,snapshot->image,
        snapshot->external_buffer,snapshot->capacity)==LV_RES_OK);
    ++host_quick_captures;
    lv_img_set_src(surface->image,surface->snapshot->image);
    lv_obj_set_pos(surface->image,lv_obj_get_x(source),lv_obj_get_y(source));
    lv_obj_add_flag(source,LV_OBJ_FLAG_HIDDEN);return true;
#endif
}
void lv_dma_static_surface_release(lv_dma_static_surface_t *surface)
{
    if(surface->image)lv_obj_del(surface->image);
    lv_dma_snapshot_cache_release(surface->snapshot);*surface=(lv_dma_static_surface_t){0};
}
