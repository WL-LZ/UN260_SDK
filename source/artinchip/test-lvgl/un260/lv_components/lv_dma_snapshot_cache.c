#include "un260/lv_components/lv_dma_snapshot_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dma_allocator.h"
#include "aic_ui/perf_stats.h"
#include "lv_ge2d.h"
#include "un260/lv_system/app_clock.h"
#include "lvgl/src/extra/others/snapshot/lv_snapshot.h"

/* The production catalog currently exposes 15 cards.  A normal+selected
 * ARGB pair needs about 548 KiB, so the former 4 MiB limit retained only
 * 18 of the 30 visual states and left most selected cards on the expensive
 * live-object draw path.  Eight MiB covers the complete current catalog
 * while staying below the measured free contiguous-memory margin.  Every
 * allocation remains fail-safe: an exhausted heap simply keeps the live
 * object fallback. */
#define DMA_SNAPSHOT_MAX_TOTAL_BYTES (8U * 1024U * 1024U)
#define DMA_SNAPSHOT_NAME_LEN 48U
#define DMA_SNAPSHOT_CACHE_CAPACITY 48U

struct lv_dma_snapshot {
    lv_img_dsc_t image;
    struct mpp_frame frame;
    uint32_t bytes;
    uint32_t last_used_tick;
    uint16_t references;
    int16_t cache_slot;
    char name[DMA_SNAPSHOT_NAME_LEN];
};

static int g_dma_snapshot_device = -1;
static uint32_t g_dma_snapshot_total_bytes;
static lv_dma_snapshot_t *g_dma_snapshot_cache[DMA_SNAPSHOT_CACHE_CAPACITY];
static lv_dma_snapshot_cache_stats_t g_dma_snapshot_stats;

static void snapshot_destroy_storage(lv_dma_snapshot_t *snapshot);

static int snapshot_cache_find_key(const char *cache_key)
{
    if (cache_key == NULL || cache_key[0] == '\0') return -1;

    for (uint32_t i = 0; i < DMA_SNAPSHOT_CACHE_CAPACITY; i++) {
        lv_dma_snapshot_t *snapshot = g_dma_snapshot_cache[i];

        if (snapshot != NULL && strcmp(snapshot->name, cache_key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int snapshot_cache_find_free_slot(void)
{
    for (uint32_t i = 0; i < DMA_SNAPSHOT_CACHE_CAPACITY; i++) {
        if (g_dma_snapshot_cache[i] == NULL) return (int)i;
    }
    return -1;
}

static int snapshot_cache_find_lru_unused(void)
{
    int victim = -1;
    uint32_t oldest_age = 0;

    for (uint32_t i = 0; i < DMA_SNAPSHOT_CACHE_CAPACITY; i++) {
        lv_dma_snapshot_t *snapshot = g_dma_snapshot_cache[i];
        uint32_t age;

        if (snapshot == NULL || snapshot->references != 0) continue;
        age = lv_tick_elaps(snapshot->last_used_tick);
        if (victim < 0 || age >= oldest_age) {
            victim = (int)i;
            oldest_age = age;
        }
    }
    return victim;
}

static bool snapshot_cache_evict_one(void)
{
    int slot = snapshot_cache_find_lru_unused();
    lv_dma_snapshot_t *snapshot;

    if (slot < 0) return false;
    snapshot = g_dma_snapshot_cache[slot];
    g_dma_snapshot_cache[slot] = NULL;
    snapshot->cache_slot = -1;
    if (perf_profile_is_enabled()) {
        printf("DMA_SNAPSHOT evict name=%s bytes=%u total_before=%u\n",
               snapshot->name, snapshot->bytes, g_dma_snapshot_total_bytes);
    }
    snapshot_destroy_storage(snapshot);
    return true;
}

static bool snapshot_cache_reserve(uint32_t bytes)
{
    while (g_dma_snapshot_total_bytes + bytes >
           DMA_SNAPSHOT_MAX_TOTAL_BYTES) {
        if (!snapshot_cache_evict_one()) return false;
    }

    while (snapshot_cache_find_free_slot() < 0) {
        if (!snapshot_cache_evict_one()) return false;
    }
    return true;
}

static void snapshot_release_frame(lv_dma_snapshot_t *snapshot)
{
    if (snapshot == NULL || snapshot->frame.buf.fd[0] < 0) return;

    mpp_buf_free(&snapshot->frame.buf);
    snapshot->frame.buf.fd[0] = -1;
}

lv_dma_snapshot_t *lv_dma_snapshot_create(lv_obj_t *obj,
                                          const char *debug_name)
{
    lv_dma_snapshot_t *snapshot = NULL;
    lv_img_dsc_t cpu_image;
    unsigned char *cpu_pixels = NULL;
    unsigned char *dma_pixels = NULL;
    uint32_t cpu_bytes;
    uint32_t row_bytes;
    uint32_t stride;
    uint32_t dma_bytes;
    uint32_t captured_stride;
    uint32_t captured_dma_bytes;
    bool capture_ok;
    lv_coord_t snapshot_w;
    lv_coord_t snapshot_h;
    lv_coord_t ext_size;
    uint64_t capture_started_us;
    uint32_t capture_us;

    if (obj == NULL || !lv_obj_is_valid(obj)) return NULL;

    cpu_bytes = lv_snapshot_buf_size_needed(obj,
                                            LV_IMG_CF_TRUE_COLOR_ALPHA);
    if (cpu_bytes == 0) return NULL;

    /* Capacity must be checked before the expensive off-screen render.  The
     * old order rendered a full card and only then discovered that the
     * bounded cache had no evictable entry, so a hidden-page prewarmer could
     * repeat the same discarded render indefinitely. */
    ext_size = _lv_obj_get_ext_draw_size(obj);
    snapshot_w = lv_obj_get_width(obj) + ext_size * 2;
    snapshot_h = lv_obj_get_height(obj) + ext_size * 2;
    if (snapshot_w <= 0 || snapshot_h <= 0) {
        g_dma_snapshot_stats.errors++;
        return NULL;
    }
    row_bytes = (uint32_t)snapshot_w * 4U;
    if (row_bytes * (uint32_t)snapshot_h != cpu_bytes) {
        g_dma_snapshot_stats.errors++;
        return NULL;
    }
    stride = (row_bytes + 15U) & ~15U;
    dma_bytes = stride * (uint32_t)snapshot_h;
    if (dma_bytes == 0 || !snapshot_cache_reserve(dma_bytes)) {
        g_dma_snapshot_stats.no_space++;
        return NULL;
    }

    cpu_pixels = lv_mem_alloc(cpu_bytes);
    if (cpu_pixels == NULL) {
        g_dma_snapshot_stats.errors++;
        return NULL;
    }

    capture_started_us = app_clock_monotonic_us();
    lv_ge2d_offscreen_capture_begin();
    if (lv_snapshot_take_to_buf(obj, LV_IMG_CF_TRUE_COLOR_ALPHA,
                                &cpu_image, cpu_pixels,
                                cpu_bytes) != LV_RES_OK) {
        (void)lv_ge2d_offscreen_capture_end();
        g_dma_snapshot_stats.errors++;
        lv_mem_free(cpu_pixels);
        return NULL;
    }
    capture_ok = lv_ge2d_offscreen_capture_end();
    capture_us = app_clock_elapsed_us32(capture_started_us,
                                        app_clock_monotonic_us());
    g_dma_snapshot_stats.capture_count++;
    g_dma_snapshot_stats.capture_total_us += capture_us;
    if (capture_us > g_dma_snapshot_stats.capture_max_us) {
        g_dma_snapshot_stats.capture_max_us = capture_us;
    }
    if (!capture_ok || cpu_image.header.w <= 0 || cpu_image.header.h <= 0) {
        g_dma_snapshot_stats.errors++;
        lv_mem_free(cpu_pixels);
        return NULL;
    }

    row_bytes = (uint32_t)cpu_image.header.w * 4U;
    captured_stride = (row_bytes + 15U) & ~15U;
    captured_dma_bytes = captured_stride * (uint32_t)cpu_image.header.h;
    if (cpu_image.header.w != snapshot_w ||
        cpu_image.header.h != snapshot_h ||
        captured_stride != stride || captured_dma_bytes != dma_bytes) {
        g_dma_snapshot_stats.errors++;
        lv_mem_free(cpu_pixels);
        return NULL;
    }

    if (g_dma_snapshot_device < 0) {
        g_dma_snapshot_device = dmabuf_device_open();
        if (g_dma_snapshot_device < 0) {
            g_dma_snapshot_stats.errors++;
            lv_mem_free(cpu_pixels);
            return NULL;
        }
    }

    snapshot = calloc(1, sizeof(*snapshot));
    if (snapshot == NULL) {
        g_dma_snapshot_stats.errors++;
        lv_mem_free(cpu_pixels);
        return NULL;
    }
    snapshot->frame.buf.fd[0] = -1;
    snapshot->cache_slot = -1;
    snapshot->frame.buf.size.width = cpu_image.header.w;
    snapshot->frame.buf.size.height = cpu_image.header.h;
    snapshot->frame.buf.stride[0] = stride;
    snapshot->frame.buf.format = MPP_FMT_ARGB_8888;
    if (mpp_buf_alloc(g_dma_snapshot_device, &snapshot->frame.buf) < 0) {
        g_dma_snapshot_stats.errors++;
        free(snapshot);
        lv_mem_free(cpu_pixels);
        return NULL;
    }

    dma_pixels = dmabuf_mmap(snapshot->frame.buf.fd[0], (int)dma_bytes);
    if (dma_pixels == NULL) {
        g_dma_snapshot_stats.errors++;
        snapshot_release_frame(snapshot);
        free(snapshot);
        lv_mem_free(cpu_pixels);
        return NULL;
    }

    for (int y = 0; y < cpu_image.header.h; y++) {
        memcpy(dma_pixels + (uint32_t)y * stride,
               cpu_pixels + (uint32_t)y * row_bytes, row_bytes);
        if (stride > row_bytes) {
            memset(dma_pixels + (uint32_t)y * stride + row_bytes, 0,
                   stride - row_bytes);
        }
    }
    dmabuf_sync(snapshot->frame.buf.fd[0], CACHE_CLEAN);
    dmabuf_munmap(dma_pixels, (int)dma_bytes);
    lv_mem_free(cpu_pixels);

    snapshot->bytes = dma_bytes;
    snapshot->last_used_tick = lv_tick_get();
    if (debug_name != NULL) {
        snprintf(snapshot->name, sizeof(snapshot->name), "%s", debug_name);
    } else {
        snprintf(snapshot->name, sizeof(snapshot->name), "DMA_SNAPSHOT");
    }

    snapshot->image.header.always_zero = 0;
    snapshot->image.header.w = cpu_image.header.w;
    snapshot->image.header.h = cpu_image.header.h;
    snapshot->image.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    snapshot->image.data_size = dma_bytes;
    snapshot->image.data = (const uint8_t *)snapshot;

    if (!lv_ge2d_register_dma_image(snapshot->image.data,
                                    &snapshot->frame,
                                    snapshot->name)) {
        g_dma_snapshot_stats.errors++;
        snapshot_release_frame(snapshot);
        free(snapshot);
        return NULL;
    }

    g_dma_snapshot_total_bytes += dma_bytes;
    g_dma_snapshot_stats.creates++;
    if (perf_profile_is_enabled()) {
        printf("DMA_SNAPSHOT create name=%s size=%dx%d bytes=%u total=%u\n",
               snapshot->name, snapshot->image.header.w,
               snapshot->image.header.h, snapshot->bytes,
               g_dma_snapshot_total_bytes);
    }
    return snapshot;
}

static void snapshot_destroy_storage(lv_dma_snapshot_t *snapshot)
{
    if (snapshot == NULL) return;

    lv_ge2d_unregister_dma_image(snapshot->image.data);
    snapshot_release_frame(snapshot);
    if (g_dma_snapshot_total_bytes >= snapshot->bytes) {
        g_dma_snapshot_total_bytes -= snapshot->bytes;
    } else {
        g_dma_snapshot_total_bytes = 0;
    }
    free(snapshot);
}

void lv_dma_snapshot_destroy(lv_dma_snapshot_t *snapshot)
{
    if (snapshot == NULL) return;

    if (snapshot->cache_slot >= 0 &&
        snapshot->cache_slot < (int)DMA_SNAPSHOT_CACHE_CAPACITY &&
        g_dma_snapshot_cache[snapshot->cache_slot] == snapshot) {
        g_dma_snapshot_cache[snapshot->cache_slot] = NULL;
    }
    snapshot->cache_slot = -1;
    snapshot_destroy_storage(snapshot);
}

lv_dma_snapshot_t *lv_dma_snapshot_cache_acquire(const char *cache_key)
{
    int slot = snapshot_cache_find_key(cache_key);
    lv_dma_snapshot_t *snapshot;

    if (slot < 0) {
        g_dma_snapshot_stats.misses++;
        return NULL;
    }
    g_dma_snapshot_stats.hits++;
    snapshot = g_dma_snapshot_cache[slot];
    if (snapshot->references < UINT16_MAX) snapshot->references++;
    snapshot->last_used_tick = lv_tick_get();
    if (perf_profile_is_enabled()) {
        printf("DMA_SNAPSHOT hit name=%s refs=%u total=%u\n",
               snapshot->name, snapshot->references,
               g_dma_snapshot_total_bytes);
    }
    return snapshot;
}

lv_dma_snapshot_t *lv_dma_snapshot_cache_acquire_or_create(
    lv_obj_t *obj, const char *cache_key)
{
    lv_dma_snapshot_t *snapshot;
    int slot;

    snapshot = lv_dma_snapshot_cache_acquire(cache_key);
    if (snapshot != NULL) return snapshot;

    snapshot = lv_dma_snapshot_create(obj, cache_key);
    if (snapshot == NULL) return NULL;

    slot = snapshot_cache_find_free_slot();
    if (slot < 0) {
        lv_dma_snapshot_destroy(snapshot);
        return NULL;
    }
    snapshot->cache_slot = (int16_t)slot;
    snapshot->references = 1;
    snapshot->last_used_tick = lv_tick_get();
    g_dma_snapshot_cache[slot] = snapshot;
    return snapshot;
}

void lv_dma_snapshot_cache_release(lv_dma_snapshot_t *snapshot)
{
    if (snapshot == NULL) return;
    if (snapshot->cache_slot < 0 ||
        snapshot->cache_slot >= (int)DMA_SNAPSHOT_CACHE_CAPACITY ||
        g_dma_snapshot_cache[snapshot->cache_slot] != snapshot) {
        lv_dma_snapshot_destroy(snapshot);
        return;
    }

    if (snapshot->references > 0) snapshot->references--;
    snapshot->last_used_tick = lv_tick_get();
}

void lv_dma_snapshot_cache_trim(void)
{
    while (snapshot_cache_evict_one()) {
    }
}

uint32_t lv_dma_snapshot_cache_item_count(void)
{
    uint32_t count = 0;

    for (uint32_t i = 0; i < DMA_SNAPSHOT_CACHE_CAPACITY; i++) {
        if (g_dma_snapshot_cache[i] != NULL) count++;
    }
    return count;
}

void lv_dma_snapshot_cache_take_stats(lv_dma_snapshot_cache_stats_t *out)
{
    if (out == NULL) return;

    *out = g_dma_snapshot_stats;
    out->item_count = lv_dma_snapshot_cache_item_count();
    out->total_bytes = g_dma_snapshot_total_bytes;
    out->max_bytes = DMA_SNAPSHOT_MAX_TOTAL_BYTES;
    g_dma_snapshot_stats = (lv_dma_snapshot_cache_stats_t){0};
}

bool lv_dma_static_surface_attach(lv_dma_static_surface_t *surface,
                                  lv_obj_t *source,
                                  const char *cache_key)
{
    lv_dma_snapshot_t *snapshot;
    const lv_img_dsc_t *image_dsc;
    lv_obj_t *parent;
    lv_obj_t *image;
    lv_coord_t ext_size;
    lv_coord_t x;
    lv_coord_t y;
    uint32_t source_index;

    if (surface == NULL || source == NULL ||
        !lv_obj_is_valid(source) || cache_key == NULL ||
        cache_key[0] == '\0') {
        return false;
    }
    if (surface->snapshot != NULL && surface->image != NULL &&
        lv_obj_is_valid(surface->image)) {
        return true;
    }

    parent = lv_obj_get_parent(source);
    if (parent == NULL || !lv_obj_is_valid(parent)) return false;

    /* Snapshot geometry includes the external shadow area.  Position the
     * replacement image by that same expansion so its pixels land exactly
     * where the original object was rendered. */
    lv_obj_update_layout(source);
    ext_size = _lv_obj_get_ext_draw_size(source);
    x = lv_obj_get_x(source) - ext_size;
    y = lv_obj_get_y(source) - ext_size;
    source_index = lv_obj_get_index(source);

    snapshot = lv_dma_snapshot_cache_acquire_or_create(source, cache_key);
    if (snapshot == NULL) return false;
    image_dsc = lv_dma_snapshot_image(snapshot);
    if (image_dsc == NULL) {
        lv_dma_snapshot_cache_release(snapshot);
        return false;
    }

    image = lv_img_create(parent);
    if (image == NULL) {
        lv_dma_snapshot_cache_release(snapshot);
        return false;
    }
    lv_img_set_src(image, image_dsc);
    lv_img_set_zoom(image, LV_IMG_ZOOM_NONE);
    lv_obj_set_pos(image, x, y);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_to_index(image, (int32_t)source_index);

    surface->snapshot = snapshot;
    surface->source = source;
    surface->image = image;
    lv_obj_add_flag(source, LV_OBJ_FLAG_HIDDEN);
    return true;
}

void lv_dma_static_surface_release(lv_dma_static_surface_t *surface)
{
    if (surface == NULL) return;

    if (surface->source != NULL && lv_obj_is_valid(surface->source)) {
        lv_obj_clear_flag(surface->source, LV_OBJ_FLAG_HIDDEN);
    }
    if (surface->image != NULL && lv_obj_is_valid(surface->image)) {
        lv_obj_del(surface->image);
    }
    lv_dma_snapshot_cache_release(surface->snapshot);
    *surface = (lv_dma_static_surface_t){0};
}

static void static_skin_set_live_visual(lv_dma_static_skin_t *skin,
                                        bool live)
{
    if (skin == NULL || skin->source == NULL ||
        !lv_obj_is_valid(skin->source)) {
        return;
    }

    if (live) {
        /* Keep the cached normal skin visible below the live button.  The
         * former implementation hid it before LVGL had rendered the first
         * live pressed frame.  On a light page that one-frame hand-off made
         * dark buttons flash white.  The live background now fades above an
         * uninterrupted cached base instead, so translation can expose the
         * normal button skin but never the page background. */
        lv_obj_set_style_bg_opa(skin->source, skin->bg_opa,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
        /* Border, outline and shadow already exist in the cached skin.  Do
         * not draw a second copy while pressed; duplicated shadows were the
         * source of the overly dark press edge. */
        lv_obj_set_style_border_opa(skin->source, LV_OPA_TRANSP,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_opa(skin->source, LV_OPA_TRANSP,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(skin->source, LV_OPA_TRANSP,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        if (skin->image != NULL && lv_obj_is_valid(skin->image)) {
            lv_obj_clear_flag(skin->image, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_set_style_bg_opa(skin->source, LV_OPA_TRANSP,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(skin->source, LV_OPA_TRANSP,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_opa(skin->source, LV_OPA_TRANSP,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(skin->source, LV_OPA_TRANSP,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        if (skin->image != NULL && lv_obj_is_valid(skin->image)) {
            lv_obj_clear_flag(skin->image, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void static_skin_restore_source_visual(lv_dma_static_skin_t *skin)
{
    if (skin == NULL || skin->source == NULL ||
        !lv_obj_is_valid(skin->source)) {
        return;
    }

    lv_obj_set_style_bg_opa(skin->source, skin->bg_opa,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(skin->source, skin->border_opa,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(skin->source, skin->outline_opa,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(skin->source, skin->shadow_opa,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
}

/* Keep the live button above its permanently visible cached normal-state skin
 * until the shared press/release color transition has completed. */
#define STATIC_SKIN_RELEASE_HOLD_MS 214U

static void static_skin_release_hold_anim_cb(void *var, int32_t value)
{
    LV_UNUSED(var);
    LV_UNUSED(value);
}

static void static_skin_release_hold_ready_cb(lv_anim_t *animation)
{
    lv_dma_static_skin_t *skin = (lv_dma_static_skin_t *)animation->var;

    static_skin_set_live_visual(skin, false);
}

static void static_skin_release_hold_start(lv_dma_static_skin_t *skin)
{
    lv_anim_t animation;

    if (skin == NULL) return;
    lv_anim_del(skin, static_skin_release_hold_anim_cb);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, skin);
    lv_anim_set_exec_cb(&animation, static_skin_release_hold_anim_cb);
    lv_anim_set_values(&animation, 0, 1);
    lv_anim_set_time(&animation, STATIC_SKIN_RELEASE_HOLD_MS);
    lv_anim_set_ready_cb(&animation, static_skin_release_hold_ready_cb);
    lv_anim_start(&animation);
}

static void static_skin_event_cb(lv_event_t *event)
{
    lv_dma_static_skin_t *skin = lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_PRESSED) {
        /* Draw the original live object only while it is pressed.  This keeps
         * every existing pressed-state style and transition intact. */
        lv_anim_del(skin, static_skin_release_hold_anim_cb);
        static_skin_set_live_visual(skin, true);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        static_skin_release_hold_start(skin);
    } else if (code == LV_EVENT_DELETE) {
        lv_anim_del(skin, static_skin_release_hold_anim_cb);
    }
}

bool lv_dma_static_skin_attach(lv_dma_static_skin_t *skin,
                               lv_obj_t *source,
                               const char *cache_key)
{
    lv_dma_snapshot_t *snapshot;
    const lv_img_dsc_t *image_dsc;
    lv_obj_t *parent;
    lv_obj_t *image;
    lv_coord_t ext_size;
    uint32_t source_index;
    uint32_t child_count;
    uint8_t *child_was_visible = NULL;

    if (skin == NULL || source == NULL || !lv_obj_is_valid(source) ||
        cache_key == NULL || cache_key[0] == '\0') {
        return false;
    }
    if (skin->snapshot != NULL && skin->image != NULL &&
        lv_obj_is_valid(skin->image)) {
        return true;
    }

    parent = lv_obj_get_parent(source);
    if (parent == NULL || !lv_obj_is_valid(parent)) return false;

    lv_obj_update_layout(source);
    ext_size = _lv_obj_get_ext_draw_size(source);
    source_index = lv_obj_get_index(source);

    /* A skin is only the object's own normal-state decoration.  Temporarily
     * exclude children (for example a translated label) from a cache-miss
     * capture so live child content is neither duplicated nor frozen. */
    child_count = lv_obj_get_child_cnt(source);
    if (child_count > 0) {
        child_was_visible = lv_mem_alloc(child_count);
        if (child_was_visible == NULL) return false;
        memset(child_was_visible, 0, child_count);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t *child = lv_obj_get_child(source, (int32_t)i);

            if (child != NULL && !lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) {
                child_was_visible[i] = 1;
                lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    snapshot = lv_dma_snapshot_cache_acquire_or_create(source, cache_key);
    if (child_was_visible != NULL) {
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t *child = lv_obj_get_child(source, (int32_t)i);

            if (child != NULL && child_was_visible[i]) {
                lv_obj_clear_flag(child, LV_OBJ_FLAG_HIDDEN);
            }
        }
        lv_mem_free(child_was_visible);
    }
    if (snapshot == NULL) return false;
    image_dsc = lv_dma_snapshot_image(snapshot);
    if (image_dsc == NULL) {
        lv_dma_snapshot_cache_release(snapshot);
        return false;
    }

    image = lv_img_create(parent);
    if (image == NULL) {
        lv_dma_snapshot_cache_release(snapshot);
        return false;
    }
    lv_img_set_src(image, image_dsc);
    lv_img_set_zoom(image, LV_IMG_ZOOM_NONE);
    lv_obj_set_pos(image, lv_obj_get_x(source) - ext_size,
                   lv_obj_get_y(source) - ext_size);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_to_index(image, (int32_t)source_index);

    skin->snapshot = snapshot;
    skin->source = source;
    skin->image = image;
    skin->bg_opa = lv_obj_get_style_bg_opa(source, LV_PART_MAIN);
    skin->border_opa = lv_obj_get_style_border_opa(source, LV_PART_MAIN);
    skin->outline_opa = lv_obj_get_style_outline_opa(source, LV_PART_MAIN);
    skin->shadow_opa = lv_obj_get_style_shadow_opa(source, LV_PART_MAIN);
    skin->style_mutated = true;

    /* The object itself remains above the cached image, so hit-testing and
     * child rendering are unchanged.  State-specific pressed styles retain
     * their higher selector specificity and can still draw feedback. */
    static_skin_set_live_visual(skin, false);
    lv_obj_add_event_cb(source, static_skin_event_cb, LV_EVENT_ALL, skin);
    return true;
}

void lv_dma_static_skin_release(lv_dma_static_skin_t *skin)
{
    if (skin == NULL) return;

    lv_anim_del(skin, static_skin_release_hold_anim_cb);

    if (skin->source != NULL && lv_obj_is_valid(skin->source) &&
        skin->style_mutated) {
        lv_obj_remove_event_cb_with_user_data(skin->source,
                                              static_skin_event_cb, skin);
        static_skin_restore_source_visual(skin);
    }
    if (skin->image != NULL && lv_obj_is_valid(skin->image)) {
        lv_obj_del(skin->image);
    }
    lv_dma_snapshot_cache_release(skin->snapshot);
    *skin = (lv_dma_static_skin_t){0};
}

const lv_img_dsc_t *lv_dma_snapshot_image(const lv_dma_snapshot_t *snapshot)
{
    return snapshot != NULL ? &snapshot->image : NULL;
}

uint32_t lv_dma_snapshot_size(const lv_dma_snapshot_t *snapshot)
{
    return snapshot != NULL ? snapshot->bytes : 0;
}

uint32_t lv_dma_snapshot_total_size(void)
{
    return g_dma_snapshot_total_bytes;
}
