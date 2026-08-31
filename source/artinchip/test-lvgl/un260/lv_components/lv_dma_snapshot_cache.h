#ifndef LV_DMA_SNAPSHOT_CACHE_H
#define LV_DMA_SNAPSHOT_CACHE_H

#include "lvgl/lvgl.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lv_dma_snapshot lv_dma_snapshot_t;

/* Capture one LVGL object subtree into an application-owned ARGB DMA image.
 * The cache is bounded globally and creation is fail-safe: callers keep their
 * normal object tree as the fallback whenever NULL is returned. */
lv_dma_snapshot_t *lv_dma_snapshot_create(lv_obj_t *obj,
                                          const char *debug_name);
void lv_dma_snapshot_destroy(lv_dma_snapshot_t *snapshot);

/* Shared snapshot cache.
 *
 * acquire() only returns an already cached image and never renders.  The
 * acquire_or_create() variant captures the object only on a cache miss.
 * release() drops the caller reference but keeps the DMA image available for
 * later page rebuilds.  Unreferenced least-recently-used entries are evicted
 * automatically when the global byte budget is needed by a new snapshot.
 */
lv_dma_snapshot_t *lv_dma_snapshot_cache_acquire(const char *cache_key);
lv_dma_snapshot_t *lv_dma_snapshot_cache_acquire_or_create(
    lv_obj_t *obj, const char *cache_key);
void lv_dma_snapshot_cache_release(lv_dma_snapshot_t *snapshot);
void lv_dma_snapshot_cache_trim(void);
uint32_t lv_dma_snapshot_cache_item_count(void);

const lv_img_dsc_t *lv_dma_snapshot_image(const lv_dma_snapshot_t *snapshot);
uint32_t lv_dma_snapshot_size(const lv_dma_snapshot_t *snapshot);
uint32_t lv_dma_snapshot_total_size(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_DMA_SNAPSHOT_CACHE_H */
