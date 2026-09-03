#ifndef LV_DMA_SNAPSHOT_CACHE_H
#define LV_DMA_SNAPSHOT_CACHE_H

#include "lvgl/lvgl.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lv_dma_snapshot lv_dma_snapshot_t;

/* A reusable, DMA-backed replacement for a static LVGL visual object.
 * The source object remains in the tree (hidden) so its geometry and lifetime
 * stay owned by the page.  The image is inserted at the same z-order and does
 * not participate in input handling. */
typedef struct {
    lv_dma_snapshot_t *snapshot;
    lv_obj_t *source;
    lv_obj_t *image;
} lv_dma_static_surface_t;

/* Cached normal-state skin for an interactive object.  The source object is
 * kept visible and clickable; only its expensive static background, border,
 * outline and shadow are replaced by a sibling DMA image. */
typedef struct {
    lv_dma_snapshot_t *snapshot;
    lv_obj_t *source;
    lv_obj_t *image;
    lv_opa_t bg_opa;
    lv_opa_t border_opa;
    lv_opa_t outline_opa;
    lv_opa_t shadow_opa;
    bool style_mutated;
} lv_dma_static_skin_t;

typedef struct {
    uint32_t hits;
    uint32_t misses;
    uint32_t creates;
    uint32_t no_space;
    uint32_t errors;
    uint32_t capture_count;
    uint64_t capture_total_us;
    uint32_t capture_max_us;
    uint32_t item_count;
    uint32_t total_bytes;
    uint32_t max_bytes;
} lv_dma_snapshot_cache_stats_t;

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

void lv_dma_snapshot_cache_take_stats(lv_dma_snapshot_cache_stats_t *out);

/* Replace one non-interactive, visually static object with a cached DMA
 * snapshot.  On any failure the source remains visible and the function
 * returns false, so callers retain the normal LVGL software-render fallback.
 * release() restores the source when it still exists. */
bool lv_dma_static_surface_attach(lv_dma_static_surface_t *surface,
                                  lv_obj_t *source,
                                  const char *cache_key);
void lv_dma_static_surface_release(lv_dma_static_surface_t *surface);

/* Cache only the normal-state visual skin while retaining the live source
 * object for hit-testing, children and pressed-state feedback. */
bool lv_dma_static_skin_attach(lv_dma_static_skin_t *skin,
                               lv_obj_t *source,
                               const char *cache_key);
void lv_dma_static_skin_release(lv_dma_static_skin_t *skin);

const lv_img_dsc_t *lv_dma_snapshot_image(const lv_dma_snapshot_t *snapshot);
uint32_t lv_dma_snapshot_size(const lv_dma_snapshot_t *snapshot);
uint32_t lv_dma_snapshot_total_size(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_DMA_SNAPSHOT_CACHE_H */
