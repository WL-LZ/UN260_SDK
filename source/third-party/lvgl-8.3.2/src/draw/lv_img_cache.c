/**
 * @file lv_img_cache.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "../misc/lv_assert.h"
#include "lv_img_cache.h"
#include "lv_img_decoder.h"
#include "lv_draw_img.h"
#include "../hal/lv_hal_tick.h"
#include "../misc/lv_gc.h"

/*********************
 *      DEFINES
 *********************/
/*Decrement life with this value on every open*/
#define LV_IMG_CACHE_AGING 1

/*Boost life by this factor (multiply time_to_open with this value)*/
#define LV_IMG_CACHE_LIFE_GAIN 1

/*Don't let life to be greater than this limit because it would require a lot of time to
 * "die" from very high values*/
#define LV_IMG_CACHE_LIFE_LIMIT 1000

/* Reserve only a small, bounded lane for expensive page-sized images.  The
 * normal cache remains available to icons and state images. */
#define LV_IMG_CACHE_RETAINED_SLOTS 6
#define LV_IMG_CACHE_RETAINED_MAX_BYTES (12U * 1024U * 1024U)
#define LV_IMG_CACHE_RETAINED_MIN_PIXELS 200000U
#define LV_IMG_CACHE_RETAINED_MIN_OPEN_MS 8U

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
#if LV_IMG_CACHE_DEF_SIZE
    static bool lv_img_cache_match(const void * src1, const void * src2);
    static _lv_img_cache_entry_t * lv_img_cache_find_victim(
        _lv_img_cache_entry_t * cache);
    static uint32_t lv_img_cache_estimated_bytes(
        const _lv_img_cache_entry_t * entry);
    static bool lv_img_cache_should_retain(
        const _lv_img_cache_entry_t * entry);
    static void lv_img_cache_rebalance_retained(
        _lv_img_cache_entry_t * cache,
        _lv_img_cache_entry_t * newest);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/
#if LV_IMG_CACHE_DEF_SIZE
    static uint16_t entry_cnt;
#endif
static lv_img_cache_memory_cb_t memory_cb;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Open an image using the image decoder interface and cache it.
 * The image will be left open meaning if the image decoder open callback allocated memory then it will remain.
 * The image is closed if a new image is opened and the new image takes its place in the cache.
 * @param src source of the image. Path to file or pointer to an `lv_img_dsc_t` variable
 * @param color color The color of the image with `LV_IMG_CF_ALPHA_...`
 * @return pointer to the cache entry or NULL if can open the image
 */
_lv_img_cache_entry_t * _lv_img_cache_open(const void * src, lv_color_t color, int32_t frame_id)
{
    /*Is the image cached?*/
    _lv_img_cache_entry_t * cached_src = NULL;

#if LV_IMG_CACHE_DEF_SIZE
    if(entry_cnt == 0) {
        LV_LOG_WARN("lv_img_cache_open: the cache size is 0");
        return NULL;
    }

    _lv_img_cache_entry_t * cache = LV_GC_ROOT(_lv_img_cache_array);

    /*Decrement all lifes. Make the entries older*/
    uint16_t i;
    for(i = 0; i < entry_cnt; i++) {
        if(cache[i].life > INT32_MIN + LV_IMG_CACHE_AGING) {
            cache[i].life -= LV_IMG_CACHE_AGING;
        }
    }

    for(i = 0; i < entry_cnt; i++) {
        if(color.full == cache[i].dec_dsc.color.full &&
           frame_id == cache[i].dec_dsc.frame_id &&
           lv_img_cache_match(src, cache[i].dec_dsc.src)) {
            /*If opened increment its life.
             *Image difficult to open should live longer to keep avoid frequent their recaching.
             *Therefore increase `life` with `time_to_open`*/
            cached_src = &cache[i];
            cached_src->life += cached_src->dec_dsc.time_to_open * LV_IMG_CACHE_LIFE_GAIN;
            if(cached_src->life > LV_IMG_CACHE_LIFE_LIMIT) cached_src->life = LV_IMG_CACHE_LIFE_LIMIT;
            LV_LOG_TRACE("image source found in the cache");
            break;
        }
    }

    /*The image is not cached then cache it now*/
    if(cached_src) return cached_src;

    /* Select the weakest normal entry.  Regular icon traffic must not evict
     * the bounded retained lane used by expensive page backgrounds. */
    cached_src = lv_img_cache_find_victim(cache);
    if(cached_src == NULL) return NULL;

    /*Close the decoder to reuse if it was opened (has a valid source)*/
    if(cached_src->dec_dsc.src) {
        lv_img_decoder_close(&cached_src->dec_dsc);
        LV_LOG_INFO("image draw: cache miss, close and reuse an entry");
    }
    else {
        LV_LOG_INFO("image draw: cache miss, cached to an empty entry");
    }
    lv_memset_00(cached_src, sizeof(*cached_src));
#else
    cached_src = &LV_GC_ROOT(_lv_img_cache_single);
#endif
    cached_src->retained = false;
    cached_src->pins = 1; /* A decoder may reclaim other entries while opening. */
    /*Open the image and measure the time to open*/
    uint32_t t_start  = lv_tick_get();
    lv_res_t open_res = lv_img_decoder_open(&cached_src->dec_dsc, src, color, frame_id);
    cached_src->pins = 0;
    if(open_res == LV_RES_INV) {
        LV_LOG_WARN("Image draw cannot open the image resource");
        lv_memset_00(cached_src, sizeof(_lv_img_cache_entry_t));
        cached_src->life = INT32_MIN; /*Make the empty entry very "weak" to force its us*/
        return NULL;
    }

    /*If `time_to_open` was not set in the open function set it here*/
    if(cached_src->dec_dsc.time_to_open == 0) {
        cached_src->dec_dsc.time_to_open = lv_tick_elaps(t_start);
    }

    if(cached_src->dec_dsc.time_to_open == 0) {
        cached_src->dec_dsc.time_to_open = 1;
    }
    cached_src->life = cached_src->dec_dsc.time_to_open * LV_IMG_CACHE_LIFE_GAIN;
    if(cached_src->life > LV_IMG_CACHE_LIFE_LIMIT) {
        cached_src->life = LV_IMG_CACHE_LIFE_LIMIT;
    }
#if LV_IMG_CACHE_DEF_SIZE
    cached_src->retained = lv_img_cache_should_retain(cached_src);
    lv_img_cache_rebalance_retained(cache, cached_src);
#endif

    return cached_src;
}

/**
 * Set the number of images to be cached.
 * More cached images mean more opened image at same time which might mean more memory usage.
 * E.g. if 20 PNG or JPG images are open in the RAM they consume memory while opened in the cache.
 * @param new_entry_cnt number of image to cache
 */
void lv_img_cache_set_size(uint16_t new_entry_cnt)
{
#if LV_IMG_CACHE_DEF_SIZE == 0
    LV_UNUSED(new_entry_cnt);
    LV_LOG_WARN("Can't change cache size because it's disabled by LV_IMG_CACHE_DEF_SIZE = 0");
#else
    for(uint16_t i = 0; i < entry_cnt; ++i) {
        if(LV_GC_ROOT(_lv_img_cache_array)[i].pins) {
            LV_LOG_WARN("Cannot resize image cache during drawing");
            return;
        }
    }
    if(LV_GC_ROOT(_lv_img_cache_array) != NULL) {
        /*Clean the cache before free it*/
        lv_img_cache_invalidate_src(NULL);
        lv_mem_free(LV_GC_ROOT(_lv_img_cache_array));
    }

    /*Reallocate the cache*/
    LV_GC_ROOT(_lv_img_cache_array) = lv_mem_alloc(sizeof(_lv_img_cache_entry_t) * new_entry_cnt);
    LV_ASSERT_MALLOC(LV_GC_ROOT(_lv_img_cache_array));
    if(LV_GC_ROOT(_lv_img_cache_array) == NULL) {
        entry_cnt = 0;
        return;
    }
    entry_cnt = new_entry_cnt;

    /*Clean the cache*/
    lv_memset_00(LV_GC_ROOT(_lv_img_cache_array), entry_cnt * sizeof(_lv_img_cache_entry_t));
#endif
}

/**
 * Invalidate an image source in the cache.
 * Useful if the image source is updated therefore it needs to be cached again.
 * @param src an image source path to a file or pointer to an `lv_img_dsc_t` variable.
 */
void lv_img_cache_invalidate_src(const void * src)
{
    LV_UNUSED(src);
#if LV_IMG_CACHE_DEF_SIZE
    _lv_img_cache_entry_t * cache = LV_GC_ROOT(_lv_img_cache_array);

    uint16_t i;
    for(i = 0; i < entry_cnt; i++) {
        if(src == NULL || lv_img_cache_match(src, cache[i].dec_dsc.src)) {
            if(cache[i].pins) {
                cache[i].invalidate_pending = true;
                continue;
            }
            if(cache[i].dec_dsc.src != NULL) {
                lv_img_decoder_close(&cache[i].dec_dsc);
            }

            lv_memset_00(&cache[i], sizeof(_lv_img_cache_entry_t));
        }
    }
#endif
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#if LV_IMG_CACHE_DEF_SIZE
static bool lv_img_cache_match(const void * src1, const void * src2)
{
    lv_img_src_t src_type = lv_img_src_get_type(src1);
    if(src_type == LV_IMG_SRC_VARIABLE)
        return src1 == src2;
    if(src_type != LV_IMG_SRC_FILE)
        return false;
    if(lv_img_src_get_type(src2) != LV_IMG_SRC_FILE)
        return false;
    return strcmp(src1, src2) == 0;
}

static _lv_img_cache_entry_t * lv_img_cache_find_victim(
    _lv_img_cache_entry_t * cache)
{
    _lv_img_cache_entry_t * weakest = NULL;
    uint16_t i;

    for(i = 0; i < entry_cnt; i++) {
        if(cache[i].pins) continue;
        if(cache[i].dec_dsc.src == NULL) return &cache[i];
        if(cache[i].retained) continue;
        if(weakest == NULL || cache[i].life < weakest->life) {
            weakest = &cache[i];
        }
    }

    /* Defensive fallback for cache sizes smaller than the retained limit. */
    if(weakest == NULL) {
        for(i = 0; i < entry_cnt; i++) {
            if(cache[i].pins) continue;
            if(weakest == NULL || cache[i].life < weakest->life) weakest = &cache[i];
        }
    }

    return weakest;
}

static uint32_t lv_img_cache_estimated_bytes(
    const _lv_img_cache_entry_t * entry)
{
    uint64_t pixels = (uint64_t)entry->dec_dsc.header.w *
                      (uint64_t)entry->dec_dsc.header.h;
    uint64_t bytes = pixels * 4U;

    return bytes > UINT32_MAX ? UINT32_MAX : (uint32_t)bytes;
}

static bool lv_img_cache_should_retain(
    const _lv_img_cache_entry_t * entry)
{
    uint64_t pixels = (uint64_t)entry->dec_dsc.header.w *
                      (uint64_t)entry->dec_dsc.header.h;

    return entry->dec_dsc.time_to_open >= LV_IMG_CACHE_RETAINED_MIN_OPEN_MS &&
           pixels >= LV_IMG_CACHE_RETAINED_MIN_PIXELS;
}

static void lv_img_cache_rebalance_retained(_lv_img_cache_entry_t * cache,
                                            _lv_img_cache_entry_t * newest)
{
    while(true) {
        _lv_img_cache_entry_t * weakest = NULL;
        uint64_t retained_bytes = 0;
        uint16_t retained_count = 0;
        uint16_t i;

        for(i = 0; i < entry_cnt; i++) {
            if(!cache[i].retained) continue;
            retained_count++;
            retained_bytes += lv_img_cache_estimated_bytes(&cache[i]);
            /* A newly opened page-sized image belongs to the page currently
             * being drawn. Prefer demoting an older retained page so the
             * active background cannot be evicted by the next icon miss. */
            if(&cache[i] != newest &&
               (weakest == NULL || cache[i].life < weakest->life)) {
                weakest = &cache[i];
            }
        }

        if(retained_count <= LV_IMG_CACHE_RETAINED_SLOTS &&
           retained_bytes <= LV_IMG_CACHE_RETAINED_MAX_BYTES) {
            break;
        }
        /* Only possible with a one-entry cache or a retained-byte limit below
         * the newest image size. In that case the new entry cannot be kept. */
        if(weakest == NULL) weakest = newest;
        if(weakest == NULL || !weakest->retained) break;
        weakest->retained = false;
        /* A demoted page-sized image is only kept for the current draw.  Make
         * it the next normal victim so large transient images cannot occupy
         * the slots reserved for reusable icons. */
        weakest->life = INT32_MIN;
    }
}
#endif

void lv_img_cache_set_memory_cb(lv_img_cache_memory_cb_t cb) { memory_cb = cb; }

uint32_t lv_img_cache_memory_used(void)
{
    uint64_t total = 0;
#if LV_IMG_CACHE_DEF_SIZE
    if(memory_cb) for(uint16_t i = 0; i < entry_cnt; ++i)
        total += memory_cb(&LV_GC_ROOT(_lv_img_cache_array)[i].dec_dsc);
#endif
    return total > UINT32_MAX ? UINT32_MAX : (uint32_t)total;
}

uint32_t lv_img_cache_reclaim_bytes(uint32_t bytes)
{
    uint64_t freed = 0;
#if LV_IMG_CACHE_DEF_SIZE
    _lv_img_cache_entry_t *cache = LV_GC_ROOT(_lv_img_cache_array);
    /* Bounded by the cache size, including failure/zero-byte cases. */
    for(uint16_t n = 0; memory_cb && n < entry_cnt && freed < bytes; ++n) {
        _lv_img_cache_entry_t *victim = NULL;
        for(uint16_t i = 0; i < entry_cnt; ++i) {
            if(cache[i].pins || !memory_cb(&cache[i].dec_dsc)) continue;
            if(victim == NULL || cache[i].life < victim->life) victim = &cache[i];
        }
        if(victim == NULL) break;
        freed += memory_cb(&victim->dec_dsc);
        lv_img_decoder_close(&victim->dec_dsc);
        lv_memset_00(victim, sizeof(*victim));
        victim->life = INT32_MIN;
    }
#else
    LV_UNUSED(bytes);
#endif
    return freed > UINT32_MAX ? UINT32_MAX : (uint32_t)freed;
}

bool lv_img_cache_reserve_bytes(uint32_t incoming, uint32_t budget)
{
    if(incoming > budget) return false;
    uint32_t used = lv_img_cache_memory_used();
    if(used > budget - incoming) lv_img_cache_reclaim_bytes(used - (budget - incoming));
    return lv_img_cache_memory_used() <= budget - incoming;
}

void lv_img_cache_pin(_lv_img_cache_entry_t *entry) { if(entry) ++entry->pins; }
void lv_img_cache_unpin(_lv_img_cache_entry_t *entry)
{
    if(!entry || !entry->pins) return;
    if(--entry->pins == 0 && entry->invalidate_pending) {
        lv_img_decoder_close(&entry->dec_dsc);
        lv_memset_00(entry, sizeof(*entry));
    }
}
