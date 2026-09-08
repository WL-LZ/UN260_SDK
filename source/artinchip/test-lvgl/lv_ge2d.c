/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <signal.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include "lvgl/src/core/lv_refr.h"
#include "lv_ge2d.h"
#include "mpp_ge.h"
#include "mpp_decoder.h"
#include "dma_allocator.h"
#include "aic_ui/image_memory.h"
#include "lv_fbdev.h"
#include "aic_ui/perf_stats.h"
#include "un260/lv_drivers/uart_io.h"
#include "un260/lv_system/app_clock.h"

#define PI 3.141592653589
#define SIN(x) (sin((x)* PI / 180.0))
#define COS(x) (cos((x)* PI / 180.0))

/* In normal GE mode every operation is a blocking ioctl.  For a small,
 * opaque solid fill the ioctl overhead is greater than LVGL's direct linear
 * write.  Keep large fills and every alpha blend on GE, where the hardware
 * still has a clear advantage. */
#define GE_FILL_CPU_THRESHOLD_PIXELS 8192U

/* Runtime image scaling is substantially more expensive than 1:1 alpha
 * blending on D21x GE. Keep a bounded cache of stable down-scaled DMA images.
 * The source decoder owns the original frame and explicitly drops all related
 * variants before closing its dma-buf, so an fd can never be reused stale. */
#define GE_SCALE_CACHE_CAPACITY 24U
#define GE_SCALE_CACHE_MAX_BYTES (2U * 1024U * 1024U)
#define GE_SCALE_CACHE_MAX_ENTRY_BYTES (512U * 1024U)
#define GE_DMA_IMAGE_REGISTRY_CAPACITY 48U

typedef struct _img_info {
    unsigned int img_size;
    int type;
    lv_fs_file_t fp;
} img_info;

static struct mpp_ge *g_ge = NULL;
/* LVGL opens and draws an image synchronously on the UI thread. Keep the
 * current file source only long enough to attribute the following GE blit in
 * performance diagnostics. It is never used by the rendering decision. */
static const char *g_profile_image_source;

typedef struct {
    bool used;
    int source_fd;
    int source_width;
    int source_height;
    int target_width;
    int target_height;
    uint32_t bytes;
    uint32_t last_use;
    struct mpp_frame frame;
} ge_scale_cache_entry_t;

static ge_scale_cache_entry_t g_scale_cache[GE_SCALE_CACHE_CAPACITY];
static uint32_t g_scale_cache_bytes;
static uint32_t g_scale_cache_clock;
static int g_scale_cache_dma_device = -1;

typedef struct {
    const void *data_key;
    const struct mpp_frame *frame;
    const char *profile_name;
} ge_dma_image_entry_t;

static ge_dma_image_entry_t g_dma_image_registry[GE_DMA_IMAGE_REGISTRY_CAPACITY];
static bool g_offscreen_capture_active;
static bool g_offscreen_capture_failed;

static void ge_offscreen_fail(const char *stage)
{
    if (!g_offscreen_capture_active) return;
    g_offscreen_capture_failed = true;
    if (perf_profile_is_enabled()) {
        uart_debug_printf("GE_OFFSCREEN_FAIL stage=%s\n", stage != NULL ? stage : "?");
    }
}

void lv_draw_aic_blend(lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc);
lv_res_t lv_draw_aic_draw_img(lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
                              const lv_area_t * coords, const void *src);
LV_ATTRIBUTE_FAST_MEM void lv_draw_aic_img_decoded(struct _lv_draw_ctx_t * draw_ctx,
                                                   const lv_draw_img_dsc_t * draw_dsc,
                                                   const lv_area_t * coords,
                                                   const uint8_t *src_buf, lv_img_cf_t cf);

static const struct mpp_frame *ge_dma_image_lookup(const void *data_key,
                                                   const char **profile_name)
{
    if (data_key == NULL) return NULL;

    for (uint32_t i = 0; i < GE_DMA_IMAGE_REGISTRY_CAPACITY; i++) {
        if (g_dma_image_registry[i].data_key == data_key) {
            if (profile_name != NULL) {
                *profile_name = g_dma_image_registry[i].profile_name;
            }
            return g_dma_image_registry[i].frame;
        }
    }
    return NULL;
}

bool lv_ge2d_register_dma_image(const void *data_key,
                                const struct mpp_frame *frame,
                                const char *profile_name)
{
    ge_dma_image_entry_t *free_entry = NULL;

    if (data_key == NULL || frame == NULL || frame->buf.fd[0] < 0) {
        return false;
    }

    for (uint32_t i = 0; i < GE_DMA_IMAGE_REGISTRY_CAPACITY; i++) {
        ge_dma_image_entry_t *entry = &g_dma_image_registry[i];

        if (entry->data_key == data_key) {
            entry->frame = frame;
            entry->profile_name = profile_name;
            return true;
        }
        if (free_entry == NULL && entry->data_key == NULL) {
            free_entry = entry;
        }
    }

    if (free_entry == NULL) return false;
    free_entry->data_key = data_key;
    free_entry->frame = frame;
    free_entry->profile_name = profile_name;
    return true;
}

void lv_ge2d_unregister_dma_image(const void *data_key)
{
    if (data_key == NULL) return;

    for (uint32_t i = 0; i < GE_DMA_IMAGE_REGISTRY_CAPACITY; i++) {
        if (g_dma_image_registry[i].data_key == data_key) {
            /* Derivatives are keyed by DMA fd. Drop before that fd can be
             * closed/reused for another snapshot, or stale pixels can match. */
            lv_ge2d_scaled_cache_drop_source(g_dma_image_registry[i].frame);
            memset(&g_dma_image_registry[i], 0,
                   sizeof(g_dma_image_registry[i]));
            return;
        }
    }
}

void lv_ge2d_offscreen_capture_begin(void)
{
    g_offscreen_capture_active = true;
    g_offscreen_capture_failed = false;
}

bool lv_ge2d_offscreen_capture_end(void)
{
    bool ok = !g_offscreen_capture_failed;

    g_offscreen_capture_active = false;
    g_offscreen_capture_failed = false;
    return ok;
}

static bool draw_target_is_display_buffer(const lv_draw_ctx_t *draw_ctx)
{
    lv_disp_t *disp = _lv_refr_get_disp_refreshing();

    return draw_ctx != NULL && disp != NULL && disp->driver != NULL &&
           disp->driver->draw_buf != NULL &&
           (draw_ctx->buf == disp->driver->draw_buf->buf1 ||
            draw_ctx->buf == disp->driver->draw_buf->buf2) &&
           disp->driver->set_px_cb == NULL;
}

static bool draw_dma_frame_software(lv_draw_ctx_t *draw_ctx,
                                    const lv_draw_img_dsc_t *draw_dsc,
                                    const lv_area_t *coords,
                                    const struct mpp_frame *frame,
                                    lv_img_cf_t cf)
{
    unsigned char *mapped = NULL;
    unsigned char *tight = NULL;
    const uint8_t *pixels;
    uint32_t row_bytes;
    uint32_t stride;
    uint32_t bytes;

    if (frame == NULL || frame->buf.buf_type != MPP_DMA_BUF_FD ||
        frame->buf.fd[0] < 0 || frame->buf.size.width <= 0 ||
        frame->buf.size.height <= 0 ||
        frame->buf.format != MPP_FMT_ARGB_8888) {
        ge_offscreen_fail("invalid_dma_frame");
        return false;
    }

    row_bytes = (uint32_t)frame->buf.size.width * 4U;
    stride = frame->buf.stride[0];
    if (stride < row_bytes) {
        ge_offscreen_fail("stride");
        return false;
    }
    bytes = stride * (uint32_t)frame->buf.size.height;
    mapped = dmabuf_mmap(frame->buf.fd[0], (int)bytes);
    if (mapped == NULL) {
        ge_offscreen_fail("dmabuf_mmap");
        return false;
    }
    dmabuf_sync(frame->buf.fd[0], CACHE_INVALID);

    pixels = mapped;
    if (stride != row_bytes) {
        if(!image_mem_acquire(IMAGE_MEM_CPU, row_bytes * (uint32_t)frame->buf.size.height)) {
            dmabuf_munmap(mapped,(int)bytes);ge_offscreen_fail("tight_budget");return false;
        }
        tight = lv_mem_alloc(row_bytes * (uint32_t)frame->buf.size.height);
        if (tight == NULL) {
            image_mem_release(IMAGE_MEM_CPU, row_bytes * (uint32_t)frame->buf.size.height);
            dmabuf_munmap(mapped, (int)bytes);
            ge_offscreen_fail("tight_alloc");
            return false;
        }
        for (int y = 0; y < frame->buf.size.height; y++) {
            memcpy(tight + (uint32_t)y * row_bytes,
                   mapped + (uint32_t)y * stride, row_bytes);
        }
        pixels = tight;
    }

    lv_draw_sw_img_decoded(draw_ctx, draw_dsc, coords, pixels, cf);
    if (tight != NULL) {
        lv_mem_free(tight);
        image_mem_release(IMAGE_MEM_CPU, row_bytes * (uint32_t)frame->buf.size.height);
    }
    dmabuf_munmap(mapped, (int)bytes);
    return true;
}

static img_info *img_info_init(const char *src)
{
    char *ptr;
    lv_fs_res_t res;

    ptr = strrchr(src, '.');
    if (!ptr) {
        LV_LOG_ERROR("unknow img format:%s", src);
        return NULL;
    }

    img_info *imginfo = (img_info *)malloc(sizeof(img_info));
    if (!imginfo) {
        LV_LOG_ERROR("src ops malloc fail");
        return NULL;
    }

    if (!strcasecmp(ptr, ".jpg") || !strcasecmp(ptr, ".jpeg"))
        imginfo->type = 0;
    else if (!strcasecmp(ptr, ".png"))
        imginfo->type = 1;
    else
        imginfo->type = -1;

    if (imginfo->type == -1) {
        LV_LOG_ERROR("src path%s\n", src);
        free(imginfo);
        return NULL;
    }

    res = lv_fs_open(&(imginfo->fp), src, LV_FS_MODE_RD);
    if(res != LV_FS_RES_OK) {
        LV_LOG_ERROR("img fs open fail");
        free(imginfo);
        return NULL;
    }

    lv_fs_seek(&(imginfo->fp), 0, SEEK_END);
    lv_fs_tell(&(imginfo->fp), &(imginfo->img_size));
    lv_fs_seek(&(imginfo->fp), 0, SEEK_SET);

    return imginfo;
}

static int img_info_deinit(img_info *imginfo)
{
    if (!imginfo)
        return -1;

    lv_fs_close(&(imginfo->fp));
    free(imginfo);

    return 0;
}

void lv_draw_aic_ctx_init(lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx)
{
    lv_draw_sw_init_ctx(drv, draw_ctx);
    lv_draw_aic_ctx_t * aic_draw_ctx = (lv_draw_aic_ctx_t *)draw_ctx;

    g_ge = get_ge();
    if (!g_ge) {
        LV_LOG_WARN("open ge device fail");
    }

    aic_draw_ctx->blend = lv_draw_aic_blend;
    aic_draw_ctx->base_draw.draw_img = lv_draw_aic_draw_img;
    aic_draw_ctx->base_draw.draw_img_decoded = lv_draw_aic_img_decoded;

    return;
}

void lv_draw_aic_ctx_deinit(lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx)
{
    LV_UNUSED(drv);
    LV_UNUSED(draw_ctx);
    lv_ge2d_scaled_cache_drop_source(NULL);
}

static inline bool is_rgb(enum mpp_pixel_format format)
{
    switch (format) {
    case MPP_FMT_ARGB_8888:
    case MPP_FMT_ABGR_8888:
    case MPP_FMT_RGBA_8888:
    case MPP_FMT_BGRA_8888:
    case MPP_FMT_XRGB_8888:
    case MPP_FMT_XBGR_8888:
    case MPP_FMT_RGBX_8888:
    case MPP_FMT_BGRX_8888:
    case MPP_FMT_RGB_888:
    case MPP_FMT_BGR_888:
    case MPP_FMT_ARGB_1555:
    case MPP_FMT_ABGR_1555:
    case MPP_FMT_RGBA_5551:
    case MPP_FMT_BGRA_5551:
    case MPP_FMT_RGB_565:
    case MPP_FMT_BGR_565:
    case MPP_FMT_ARGB_4444:
    case MPP_FMT_ABGR_4444:
    case MPP_FMT_RGBA_4444:
    case MPP_FMT_BGRA_4444:
        return true;
    default:
        break;
    }
    return false;
}

static int ge_scale_cache_bytes_per_pixel(enum mpp_pixel_format format)
{
    switch (format) {
    case MPP_FMT_ARGB_8888:
    case MPP_FMT_ABGR_8888:
    case MPP_FMT_RGBA_8888:
    case MPP_FMT_BGRA_8888:
    case MPP_FMT_XRGB_8888:
    case MPP_FMT_XBGR_8888:
    case MPP_FMT_RGBX_8888:
    case MPP_FMT_BGRX_8888:
        return 4;
    case MPP_FMT_RGB_888:
    case MPP_FMT_BGR_888:
        return 3;
    case MPP_FMT_ARGB_1555:
    case MPP_FMT_ABGR_1555:
    case MPP_FMT_RGBA_5551:
    case MPP_FMT_BGRA_5551:
    case MPP_FMT_RGB_565:
    case MPP_FMT_BGR_565:
    case MPP_FMT_ARGB_4444:
    case MPP_FMT_ABGR_4444:
    case MPP_FMT_RGBA_4444:
    case MPP_FMT_BGRA_4444:
        return 2;
    default:
        return 0;
    }
}

static void ge_scale_cache_release(ge_scale_cache_entry_t *entry)
{
    if (entry == NULL || !entry->used) {
        return;
    }

    mpp_buf_free(&entry->frame.buf);
    image_mem_release(IMAGE_MEM_SCALE, entry->bytes);
    if (g_scale_cache_bytes >= entry->bytes) {
        g_scale_cache_bytes -= entry->bytes;
    } else {
        g_scale_cache_bytes = 0;
    }
    memset(entry, 0, sizeof(*entry));
}

void lv_ge2d_scaled_cache_drop_source(const void *source_frame)
{
    const struct mpp_frame *source = source_frame;
    int source_fd = source != NULL ? source->buf.fd[0] : -1;

    for (uint32_t i = 0; i < GE_SCALE_CACHE_CAPACITY; i++) {
        if (!g_scale_cache[i].used) {
            continue;
        }
        if (source == NULL || g_scale_cache[i].source_fd == source_fd) {
            ge_scale_cache_release(&g_scale_cache[i]);
        }
    }

    if (source == NULL && g_scale_cache_dma_device >= 0) {
        dmabuf_device_close(g_scale_cache_dma_device);
        g_scale_cache_dma_device = -1;
    }
}

static void ge_scale_cache_reclaim(uint32_t bytes)
{
    /* Normal GE is synchronized before cached frame pointers escape a draw.
     * Never enable this reclamation for asynchronous/CMDQ operation. */
    if(!g_ge || mpp_ge_get_mode(g_ge) != GE_MODE_NORMAL) return;
    uint32_t freed=0;
    for(unsigned i=0;i<GE_SCALE_CACHE_CAPACITY && freed<bytes;i++) {
        if(g_scale_cache[i].used) { freed+=g_scale_cache[i].bytes; ge_scale_cache_release(&g_scale_cache[i]); }
    }
}
static ge_scale_cache_entry_t *ge_scale_cache_find_slot(uint32_t bytes)
{
    ge_scale_cache_entry_t *slot = NULL;

    while (g_scale_cache_bytes + bytes > GE_SCALE_CACHE_MAX_BYTES) {
        ge_scale_cache_entry_t *oldest = NULL;

        for (uint32_t i = 0; i < GE_SCALE_CACHE_CAPACITY; i++) {
            if (g_scale_cache[i].used &&
                (oldest == NULL ||
                 g_scale_cache[i].last_use < oldest->last_use)) {
                oldest = &g_scale_cache[i];
            }
        }
        if (oldest == NULL) {
            return NULL;
        }
        ge_scale_cache_release(oldest);
    }

    for (uint32_t i = 0; i < GE_SCALE_CACHE_CAPACITY; i++) {
        if (!g_scale_cache[i].used) {
            return &g_scale_cache[i];
        }
        if (slot == NULL || g_scale_cache[i].last_use < slot->last_use) {
            slot = &g_scale_cache[i];
        }
    }

    ge_scale_cache_release(slot);
    return slot;
}

static struct mpp_frame *ge_scale_cache_get(struct mpp_frame *source,
                                            int target_width,
                                            int target_height)
{
    ge_scale_cache_entry_t *slot;
    struct ge_bitblt blt = {0};
    int bytes_per_pixel;
    uint32_t stride;
    uint32_t bytes;

    if (source == NULL || g_ge == NULL ||
        mpp_ge_get_mode(g_ge) != GE_MODE_NORMAL ||
        source->buf.buf_type != MPP_DMA_BUF_FD ||
        source->buf.fd[0] < 0 || target_width <= 0 || target_height <= 0) {
        return NULL;
    }

    bytes_per_pixel = ge_scale_cache_bytes_per_pixel(source->buf.format);
    if (bytes_per_pixel == 0) {
        return NULL;
    }

    stride = ((uint32_t)target_width * (uint32_t)bytes_per_pixel + 15U) & ~15U;
    bytes = (stride * (uint32_t)target_height + 4095U) & ~4095U;
    if (bytes == 0 || bytes > GE_SCALE_CACHE_MAX_ENTRY_BYTES) {
        return NULL;
    }

    g_scale_cache_clock++;
    for (uint32_t i = 0; i < GE_SCALE_CACHE_CAPACITY; i++) {
        ge_scale_cache_entry_t *entry = &g_scale_cache[i];

        if (entry->used && entry->source_fd == source->buf.fd[0] &&
            entry->source_width == source->buf.size.width &&
            entry->source_height == source->buf.size.height &&
            entry->target_width == target_width &&
            entry->target_height == target_height) {
            entry->last_use = g_scale_cache_clock;
            return &entry->frame;
        }
    }

    if (g_scale_cache_dma_device < 0) {
        g_scale_cache_dma_device = dmabuf_device_open();
        if (g_scale_cache_dma_device < 0) {
            return NULL;
        }
    }

    image_mem_register(IMAGE_MEM_SCALE, ge_scale_cache_reclaim);
    if(!image_mem_acquire(IMAGE_MEM_SCALE, bytes)) return NULL;
    slot = ge_scale_cache_find_slot(bytes);
    if (slot == NULL) {
        image_mem_release(IMAGE_MEM_SCALE, bytes);
        return NULL;
    }

    memset(slot, 0, sizeof(*slot));
    slot->frame.buf.size.width = target_width;
    slot->frame.buf.size.height = target_height;
    slot->frame.buf.format = source->buf.format;
    slot->frame.buf.stride[0] = (int)stride;
    if (mpp_buf_alloc(g_scale_cache_dma_device, &slot->frame.buf) < 0) {
        image_mem_release(IMAGE_MEM_SCALE, bytes);
        memset(slot, 0, sizeof(*slot));
        return NULL;
    }

    blt.src_buf = source->buf;
    blt.src_buf.crop_en = 1;
    blt.src_buf.crop.x = 0;
    blt.src_buf.crop.y = 0;
    blt.src_buf.crop.width = source->buf.size.width;
    blt.src_buf.crop.height = source->buf.size.height;
    blt.dst_buf = slot->frame.buf;
    blt.dst_buf.crop_en = 1;
    blt.dst_buf.crop.x = 0;
    blt.dst_buf.crop.y = 0;
    blt.dst_buf.crop.width = target_width;
    blt.dst_buf.crop.height = target_height;
    /* Preserve the source alpha channel in the generated pixels. Do not blend
     * against the uninitialized cache destination. */
    blt.ctrl.alpha_en = 0;
    blt.ctrl.flags = MPP_ROTATION_0;

    if (mpp_ge_bitblt(g_ge, &blt) < 0 ||
        mpp_ge_emit(g_ge) < 0 || mpp_ge_sync(g_ge) < 0) {
        mpp_buf_free(&slot->frame.buf);
        image_mem_release(IMAGE_MEM_SCALE, bytes);
        memset(slot, 0, sizeof(*slot));
        return NULL;
    }

    slot->used = true;
    slot->source_fd = source->buf.fd[0];
    slot->source_width = source->buf.size.width;
    slot->source_height = source->buf.size.height;
    slot->target_width = target_width;
    slot->target_height = target_height;
    slot->bytes = bytes;
    slot->last_use = g_scale_cache_clock;
    g_scale_cache_bytes += bytes;
    return &slot->frame;
}

static void transform_upscaled(const lv_draw_img_dsc_t *draw_dsc, int32_t xin,
                               int32_t yin, int32_t * xout, int32_t * yout)
{
    int32_t pivot_x_256 = draw_dsc->pivot.x * 256;
    int32_t pivot_y_256 = draw_dsc->pivot.y * 256;
    int32_t zoom = (256 * 256) / draw_dsc->zoom;

    xin -= draw_dsc->pivot.x;
    yin -= draw_dsc->pivot.y;

    if (draw_dsc->angle == 0) {
        *xout = (int32_t)(xin * zoom) + pivot_x_256;
        *yout = (int32_t)(yin * zoom) + pivot_y_256;
    } else {
        int32_t sinma;
        int32_t cosma;
        int32_t angle = -draw_dsc->angle;
        int32_t angle_low = angle / 10;
        int32_t angle_high = angle_low + 1;
        int32_t angle_rem = angle - (angle_low * 10);
        int32_t s1 = lv_trigo_sin(angle_low);
        int32_t s2 = lv_trigo_sin(angle_high);
        int32_t c1 = lv_trigo_sin(angle_low + 90);
        int32_t c2 = lv_trigo_sin(angle_high + 90);

        sinma = (s1 * (10 - angle_rem) + s2 * angle_rem) / 10;
        cosma = (c1 * (10 - angle_rem) + c2 * angle_rem) / 10;
        sinma = sinma >> (LV_TRIGO_SHIFT - 10);
        cosma = cosma >> (LV_TRIGO_SHIFT - 10);

        if (zoom == LV_IMG_ZOOM_NONE) {
            *xout = ((cosma * xin - sinma * yin) >> 2) + (pivot_x_256);
            *yout = ((sinma * xin + cosma * yin) >> 2) + (pivot_y_256);
        } else {
            *xout = (((cosma * xin - sinma * yin) * zoom) >> 10) + (pivot_x_256);
            *yout = (((sinma * xin + cosma * yin) * zoom) >> 10) + (pivot_y_256);
        }
    }
}

static int ge_run_blit(lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t *draw_dsc,
                    struct mpp_frame *frame, const lv_area_t *clip_area, const lv_area_t *coords)
{
    int ret;
    bool profile_enabled = perf_profile_is_enabled();
    uint64_t profile_started_us = 0;
    uint64_t profile_phase_started_us = 0;
    uint32_t profile_submit_us = 0;
    uint32_t profile_emit_us = 0;
    uint32_t profile_sync_us = 0;
    uint32_t profile_elapsed_us = 0;
    lv_coord_t blend_w;
    lv_coord_t blend_h;
    int src_crop_x;
    int src_crop_y;
    int src_crop_w;
    int src_crop_h;
    int dst_crop_x;
    int dst_crop_y;
    int dst_crop_w;
    int dst_crop_h;
    lv_area_t blend_area;
    struct ge_bitblt blt = { 0 };
    lv_draw_img_dsc_t cached_draw_dsc;
    lv_area_t cached_coords;
    lv_color_t * dest_buf = draw_ctx->buf;
    lv_coord_t dest_width = lv_area_get_width(draw_ctx->buf_area);
    lv_coord_t dest_height = lv_area_get_height(draw_ctx->buf_area);
    int line_length = draw_buf_pitch();
    enum mpp_pixel_format fmt = draw_buf_fmt();

    if (draw_dsc->angle == 0 &&
        draw_dsc->zoom >= 64 && draw_dsc->zoom < LV_IMG_ZOOM_NONE) {
        struct mpp_frame *cached_frame;

        _lv_img_buf_get_transformed_area(
            &cached_coords, frame->buf.size.width, frame->buf.size.height,
            0, draw_dsc->zoom, &draw_dsc->pivot);
        lv_area_move(&cached_coords, coords->x1, coords->y1);
        cached_frame = ge_scale_cache_get(
            frame, lv_area_get_width(&cached_coords),
            lv_area_get_height(&cached_coords));
        if (cached_frame != NULL) {
            cached_draw_dsc = *draw_dsc;
            cached_draw_dsc.zoom = LV_IMG_ZOOM_NONE;
            cached_draw_dsc.pivot.x = 0;
            cached_draw_dsc.pivot.y = 0;
            draw_dsc = &cached_draw_dsc;
            frame = cached_frame;
            coords = &cached_coords;
        }
    }

    if (draw_dsc->zoom == LV_IMG_ZOOM_NONE && draw_dsc->angle == 0) {
        if(!_lv_area_intersect(&blend_area, coords, clip_area))
            return LV_RES_OK;

        blend_w = lv_area_get_width(&blend_area);
        blend_h = lv_area_get_height(&blend_area);
        src_crop_x = blend_area.x1 - coords->x1;
        src_crop_y = blend_area.y1 - coords->y1;
        src_crop_w = blend_w;
        src_crop_h = blend_h;
        dst_crop_x = blend_area.x1 - draw_ctx->buf_area->x1;
        dst_crop_y = blend_area.y1 - draw_ctx->buf_area->y1;
        dst_crop_w = blend_w;
        dst_crop_h = blend_h;
    } else {
        lv_area_t trans_area;
        lv_coord_t src_w = lv_area_get_width(coords);
        lv_coord_t src_h = lv_area_get_height(coords);

        blend_w = lv_area_get_width(clip_area);
        blend_h = lv_area_get_height(clip_area);
        lv_area_copy(&trans_area, clip_area);
        lv_area_move(&trans_area, -coords->x1, -coords->y1);

        int32_t xs1_ups, ys1_ups, xs2_ups, ys2_ups;
        transform_upscaled(draw_dsc, trans_area.x1, trans_area.y1,
                           &xs1_ups, &ys1_ups);
        transform_upscaled(draw_dsc, trans_area.x2, trans_area.y2,
                           &xs2_ups, &ys2_ups);

        int32_t x1_int = LV_CLAMP(0, LV_MIN(xs1_ups, xs2_ups) >> 8, src_w - 1);
        int32_t x2_int = LV_CLAMP(0, LV_MAX(xs1_ups, xs2_ups) >> 8, src_w - 1);
        int32_t y1_int = LV_CLAMP(0, LV_MIN(ys1_ups, ys2_ups) >> 8, src_h - 1);
        int32_t y2_int = LV_CLAMP(0, LV_MAX(ys1_ups, ys2_ups) >> 8, src_h - 1);

        src_crop_x = x1_int;
        src_crop_y = y1_int;
        src_crop_w = x2_int - x1_int + 1;
        src_crop_h = y2_int - y1_int + 1;
        dst_crop_x = clip_area->x1 - draw_ctx->buf_area->x1;
        dst_crop_y = clip_area->y1 - draw_ctx->buf_area->y1;
        dst_crop_w = blend_w;
        dst_crop_h = blend_h;

        if (src_crop_w <= 4 || src_crop_h <= 4 ||
            dst_crop_w <= 4 || dst_crop_h <= 4) {
            LV_LOG_ERROR("invalid size: src w:%d src h:%d, dst w:%d dst h:%d\n",
                         src_crop_w, src_crop_h,
                         dst_crop_w, dst_crop_h);
            return LV_RES_INV;
        }
    }

    /* src buf */
    blt.src_buf.buf_type = MPP_DMA_BUF_FD;
    if (frame->buf.format == MPP_FMT_ARGB_8888
        || frame->buf.format == MPP_FMT_RGBA_8888
        || frame->buf.format == MPP_FMT_RGB_888) {
        mpp_ge_add_dmabuf(g_ge, frame->buf.fd[0]);
        blt.src_buf.fd[0] = frame->buf.fd[0];
        blt.src_buf.stride[0] = frame->buf.stride[0];
        blt.src_buf.format = frame->buf.format;
    } else if (frame->buf.format == MPP_FMT_YUV420P
        || frame->buf.format == MPP_FMT_YUV444P
        || frame->buf.format == MPP_FMT_YUV422P) {
        mpp_ge_add_dmabuf(g_ge, frame->buf.fd[0]);
        mpp_ge_add_dmabuf(g_ge, frame->buf.fd[1]);
        mpp_ge_add_dmabuf(g_ge, frame->buf.fd[2]);
        blt.src_buf.fd[0] = frame->buf.fd[0];
        blt.src_buf.fd[1] = frame->buf.fd[1];
        blt.src_buf.fd[2] = frame->buf.fd[2];
        blt.src_buf.stride[0] = frame->buf.stride[0];
        blt.src_buf.stride[1] = frame->buf.stride[1];
        blt.src_buf.stride[2] = frame->buf.stride[2];
        blt.src_buf.format = frame->buf.format;
    } else if (frame->buf.format == MPP_FMT_NV12
        || frame->buf.format == MPP_FMT_NV21
        || frame->buf.format == MPP_FMT_NV16
        || frame->buf.format == MPP_FMT_NV61) {
        mpp_ge_add_dmabuf(g_ge, frame->buf.fd[0]);
        mpp_ge_add_dmabuf(g_ge, frame->buf.fd[1]);
        blt.src_buf.fd[0] = frame->buf.fd[0];
        blt.src_buf.fd[1] = frame->buf.fd[1];
        blt.src_buf.stride[0] = frame->buf.stride[0];
        blt.src_buf.stride[1] = frame->buf.stride[1];
        blt.src_buf.format = frame->buf.format;
    }

    blt.src_buf.size.width = frame->buf.size.width;
    blt.src_buf.size.height = frame->buf.size.height;
    blt.src_buf.crop_en = 1;
    blt.src_buf.crop.x = src_crop_x;
    blt.src_buf.crop.y = src_crop_y;
    blt.src_buf.crop.width = src_crop_w;
    blt.src_buf.crop.height = src_crop_h;

    /* dst buf */
#ifdef USE_DRAW_BUF
    blt.dst_buf.buf_type = MPP_DMA_BUF_FD;
    if (dest_buf == (lv_color_t *)g_draw_buf[0])
        blt.dst_buf.fd[0] = g_draw_buf_fd[0];
    else
        blt.dst_buf.fd[0] = g_draw_buf_fd[1];
#else
    blt.dst_buf.buf_type = MPP_PHY_ADDR;
    if (dest_buf == (lv_color_t *)g_frame_buf[0]) {
        blt.dst_buf.phy_addr[0] = g_frame_phy[0];
    } else if (dest_buf == (lv_color_t *)g_frame_buf[1]) {
        blt.dst_buf.phy_addr[0] = g_frame_phy[1];
    } else {
        blt.dst_buf.phy_addr[0] = g_frame_phy[2];
    }
#endif
    blt.dst_buf.stride[0] = line_length;
    blt.dst_buf.size.width = dest_width;
    blt.dst_buf.size.height = dest_height;
    blt.dst_buf.format = fmt;
    blt.dst_buf.crop_en = 1;
    blt.dst_buf.crop.x = dst_crop_x;
    blt.dst_buf.crop.y = dst_crop_y;
    blt.dst_buf.crop.width = dst_crop_w;
    blt.dst_buf.crop.height = dst_crop_h;

    if (!is_rgb(blt.src_buf.format) &&
        (blt.src_buf.crop.width < 8 || blt.src_buf.crop.height < 8)) {
        return LV_RES_INV;
    }

    if (!is_rgb(blt.dst_buf.format) &&
        (blt.dst_buf.crop.width < 8 || blt.dst_buf.crop.height < 8)) {
        return LV_RES_INV;
    }

    /* ctrl */
    blt.ctrl.flags = draw_dsc->angle / 900;
    /* Opaque sources without a per-pixel alpha channel can use a plain copy.
     * The old condition enabled GE alpha blending for this common case. */
    if(draw_dsc->opa >= LV_OPA_MAX && frame->buf.format != MPP_FMT_ARGB_8888)
        blt.ctrl.alpha_en = 0;
    else
        blt.ctrl.alpha_en = 1;

    blt.ctrl.src_alpha_mode = 2;
    blt.ctrl.src_global_alpha = draw_dsc->opa;

    if (profile_enabled) {
        profile_started_us = app_clock_monotonic_us();
        profile_phase_started_us = profile_started_us;
    }
    ret = mpp_ge_bitblt(g_ge, &blt);
    if (ret < 0) {
        LV_LOG_ERROR("bitblt fail");
        return LV_RES_INV;
    }
    if (profile_enabled) {
        profile_submit_us = app_clock_elapsed_us32(
            profile_phase_started_us, app_clock_monotonic_us());
        profile_phase_started_us = app_clock_monotonic_us();
    }
    ret = mpp_ge_emit(g_ge);
    if (ret < 0) {
        LV_LOG_ERROR("emit fail");
        return LV_RES_INV;
    }
    if (profile_enabled) {
        profile_emit_us = app_clock_elapsed_us32(
            profile_phase_started_us, app_clock_monotonic_us());
        profile_phase_started_us = app_clock_monotonic_us();
    }
    ret = mpp_ge_sync(g_ge);
    if (ret < 0) {
        LV_LOG_ERROR("sync fail");
        return LV_RES_INV;
    }
    if (profile_enabled) {
        profile_sync_us = app_clock_elapsed_us32(
            profile_phase_started_us, app_clock_monotonic_us());
    }

    if (profile_enabled) {
        profile_elapsed_us = app_clock_elapsed_us32(
            profile_started_us, app_clock_monotonic_us());
        perf_profile_report_ge(
            PERF_PROFILE_GE_BLIT,
            (uint64_t)dst_crop_w * (uint64_t)dst_crop_h,
            profile_elapsed_us,
            profile_submit_us, profile_emit_us, profile_sync_us);
        perf_profile_report_ge_blit_path(
            blt.ctrl.alpha_en != 0,
            src_crop_w != dst_crop_w || src_crop_h != dst_crop_h,
            (uint64_t)dst_crop_w * (uint64_t)dst_crop_h,
            profile_elapsed_us);
        perf_profile_report_ge_image_source(
            g_profile_image_source,
            blt.ctrl.alpha_en != 0,
            src_crop_w != dst_crop_w || src_crop_h != dst_crop_h,
            (uint64_t)dst_crop_w * (uint64_t)dst_crop_h,
            profile_elapsed_us);
    }

    return LV_RES_OK;
}

static int ge_run_rotate(lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t *draw_dsc,
                    struct mpp_frame *frame, const lv_area_t *blend_area, const lv_area_t *coords)

{
    int ret;
    bool profile_enabled = perf_profile_is_enabled();
    uint64_t profile_started_us = 0;
    uint64_t profile_phase_started_us = 0;
    uint32_t profile_submit_us = 0;
    uint32_t profile_emit_us = 0;
    uint32_t profile_sync_us = 0;
    struct ge_rotation rot = { 0 };
    lv_color_t * dest_buf = draw_ctx->buf;
    lv_coord_t dest_width = lv_area_get_width(draw_ctx->buf_area);
    lv_coord_t dest_height = lv_area_get_height(draw_ctx->buf_area);
    lv_coord_t blend_width = lv_area_get_width(blend_area);
    lv_coord_t blend_height = lv_area_get_height(blend_area);
    int line_length = draw_buf_pitch();
    enum mpp_pixel_format fmt = draw_buf_fmt();

    /* src buf */
    rot.src_buf.buf_type = MPP_DMA_BUF_FD;
    if (frame->buf.format == MPP_FMT_ARGB_8888
        || frame->buf.format == MPP_FMT_RGBA_8888
        || frame->buf.format == MPP_FMT_RGB_888) {
        mpp_ge_add_dmabuf(g_ge, frame->buf.fd[0]);
        rot.src_buf.fd[0] = frame->buf.fd[0];
        rot.src_buf.stride[0] = frame->buf.stride[0];
        rot.src_buf.format = frame->buf.format;
    } else {
        LV_LOG_ERROR("frame unsupport format:%d\n", frame->buf.format);
        return LV_RES_INV;
    }
    rot.src_buf.crop_en = 0;
    rot.src_buf.size.width = frame->buf.size.width;
    rot.src_buf.size.height = frame->buf.size.height;
    rot.src_rot_center.x = draw_dsc->pivot.x;
    rot.src_rot_center.y = draw_dsc->pivot.y;

    /* dst buf */
#ifdef USE_DRAW_BUF
    rot.dst_buf.buf_type = MPP_DMA_BUF_FD;
    if (dest_buf == (lv_color_t *)g_draw_buf[0])
        rot.dst_buf.fd[0] = g_draw_buf_fd[0];
    else
        rot.dst_buf.fd[0] = g_draw_buf_fd[1];
#else
    rot.dst_buf.buf_type = MPP_PHY_ADDR;
    if (dest_buf == (lv_color_t *)g_frame_buf[0]) {
        rot.dst_buf.phy_addr[0] = g_frame_phy[0];
    } else if (dest_buf == (lv_color_t *)g_frame_buf[1]) {
        rot.dst_buf.phy_addr[0] = g_frame_phy[1];
    } else {
        rot.dst_buf.phy_addr[0] = g_frame_phy[2];
    }
#endif
    rot.dst_buf.stride[0] = line_length;
    rot.dst_buf.size.width = dest_width;
    rot.dst_buf.size.height = dest_height;
    rot.dst_buf.format = fmt;
    rot.dst_buf.crop_en = 0;
    rot.dst_buf.crop.x = blend_area->x1;
    rot.dst_buf.crop.y = blend_area->y1;
    rot.dst_buf.crop.width = blend_width;
    rot.dst_buf.crop.height = blend_height;
    rot.dst_rot_center.x = coords->x1 + draw_dsc->pivot.x;
    rot.dst_rot_center.y = coords->y1 + draw_dsc->pivot.y;

    /* angle */
    rot.angle_sin = SIN((double)draw_dsc->angle / 10) * 4096;
    rot.angle_cos = COS((double)draw_dsc->angle / 10) * 4096;

    /* ctrl */
    rot.ctrl.flags = 0;
    rot.ctrl.alpha_en = 1;
    rot.ctrl.src_alpha_mode = 2;
    rot.ctrl.src_global_alpha = draw_dsc->opa;

    if (profile_enabled) {
        profile_started_us = app_clock_monotonic_us();
        profile_phase_started_us = profile_started_us;
    }
    ret = mpp_ge_rotate(g_ge, &rot);
    if (ret < 0) {
        LV_LOG_ERROR("rotate fail");
        return LV_RES_INV;
    }
    if (profile_enabled) {
        profile_submit_us = app_clock_elapsed_us32(
            profile_phase_started_us, app_clock_monotonic_us());
        profile_phase_started_us = app_clock_monotonic_us();
    }
    ret = mpp_ge_emit(g_ge);
    if (ret < 0) {
        LV_LOG_ERROR("emit fail");
        return LV_RES_INV;
    }
    if (profile_enabled) {
        profile_emit_us = app_clock_elapsed_us32(
            profile_phase_started_us, app_clock_monotonic_us());
        profile_phase_started_us = app_clock_monotonic_us();
    }
    ret = mpp_ge_sync(g_ge);
    if (ret < 0) {
        LV_LOG_ERROR("sync fail");
        return LV_RES_INV;
    }
    if (profile_enabled) {
        profile_sync_us = app_clock_elapsed_us32(
            profile_phase_started_us, app_clock_monotonic_us());
    }

    if (profile_enabled) {
        perf_profile_report_ge(
            PERF_PROFILE_GE_ROTATE,
            (uint64_t)blend_width * (uint64_t)blend_height,
            app_clock_elapsed_us32(profile_started_us,
                                   app_clock_monotonic_us()),
            profile_submit_us, profile_emit_us, profile_sync_us);
    }

    return LV_RES_OK;
}

static inline void RGB565_To_ARGB(unsigned short src_pixel, unsigned int* dst_color)
{
    unsigned int a, r, g, b;

    a = 0xff;
    r = ((src_pixel & 0xf800) >> 8);
    g = (src_pixel & 0x07e0) >> 3;
    b = (src_pixel & 0x1f) << 3;

    *dst_color = (a << 24) | ((r | (r >> 5)) << 16) |
        ((g | (g >> 6)) << 8) | (b | (b >> 5));

}

static int ge_run_fill(lv_draw_ctx_t * draw_ctx, unsigned int color, unsigned char opa,
                       int blend_enable, const lv_area_t *blend_area)
{
    int ret;
    bool profile_enabled = perf_profile_is_enabled();
    uint64_t profile_started_us = 0;
    uint64_t profile_phase_started_us = 0;
    uint32_t profile_submit_us = 0;
    uint32_t profile_emit_us = 0;
    uint32_t profile_sync_us = 0;
    struct ge_fillrect fill = { 0 };
    lv_color_t * dest_buf = draw_ctx->buf;
    lv_coord_t dest_width = lv_area_get_width(draw_ctx->buf_area);
    lv_coord_t dest_height = lv_area_get_height(draw_ctx->buf_area);
    lv_coord_t blend_width = lv_area_get_width(blend_area);
    lv_coord_t blend_height = lv_area_get_height(blend_area);
    int bpp = draw_buf_bpp();
    int line_length = draw_buf_pitch();
    enum mpp_pixel_format fmt = draw_buf_fmt();

    if (bpp == 16) {
        RGB565_To_ARGB((unsigned short)color, &color);
    }

    /* fill info */
    fill.type = GE_NO_GRADIENT;
    fill.start_color = color;
    fill.end_color = 0;

    /* dst buf */
#ifdef USE_DRAW_BUF
    fill.dst_buf.buf_type = MPP_DMA_BUF_FD;
    if (dest_buf == (lv_color_t *)g_draw_buf[0])
        fill.dst_buf.fd[0] = g_draw_buf_fd[0];
    else
        fill.dst_buf.fd[0] = g_draw_buf_fd[1];
#else
    fill.dst_buf.buf_type = MPP_PHY_ADDR;
    if (dest_buf == (lv_color_t *)g_frame_buf[0]) {
        fill.dst_buf.phy_addr[0] = g_frame_phy[0];
    } else if (dest_buf == (lv_color_t *)g_frame_buf[1]) {
        fill.dst_buf.phy_addr[0] = g_frame_phy[1];
    } else {
        fill.dst_buf.phy_addr[0] = g_frame_phy[2];
    }
#endif
    fill.dst_buf.stride[0] = line_length;
    fill.dst_buf.size.width = dest_width;
    fill.dst_buf.size.height = dest_height;
    fill.dst_buf.format = fmt;
    fill.dst_buf.crop_en = 1;
    fill.dst_buf.crop.x = blend_area->x1;
    fill.dst_buf.crop.y = blend_area->y1;
    fill.dst_buf.crop.width = blend_width;
    fill.dst_buf.crop.height = blend_height;

    /* ctrl */
    fill.ctrl.flags = 0;
    if(opa < LV_OPA_MAX && blend_enable)
        fill.ctrl.alpha_en = 1;
    else
        fill.ctrl.alpha_en = 0;

    fill.ctrl.src_alpha_mode = 1;
    fill.ctrl.src_global_alpha = opa;

    if (profile_enabled) {
        profile_started_us = app_clock_monotonic_us();
        profile_phase_started_us = profile_started_us;
    }
    ret = mpp_ge_fillrect(g_ge, &fill);
    if (ret < 0) {
        LV_LOG_ERROR("fillrect1 fail");
        return LV_RES_INV;
    }
    if (profile_enabled) {
        profile_submit_us = app_clock_elapsed_us32(
            profile_phase_started_us, app_clock_monotonic_us());
        profile_phase_started_us = app_clock_monotonic_us();
    }
    ret = mpp_ge_emit(g_ge);
    if (ret < 0) {
        LV_LOG_ERROR("emit fail");
        return LV_RES_INV;
    }
    if (profile_enabled) {
        profile_emit_us = app_clock_elapsed_us32(
            profile_phase_started_us, app_clock_monotonic_us());
        profile_phase_started_us = app_clock_monotonic_us();
    }
    ret = mpp_ge_sync(g_ge);
    if (ret < 0) {
        LV_LOG_ERROR("sync fail");
        return LV_RES_INV;
    }
    if (profile_enabled) {
        profile_sync_us = app_clock_elapsed_us32(
            profile_phase_started_us, app_clock_monotonic_us());
    }

    if (profile_enabled) {
        perf_profile_report_ge(
            PERF_PROFILE_GE_FILL,
            (uint64_t)blend_width * (uint64_t)blend_height,
            app_clock_elapsed_us32(profile_started_us,
                                   app_clock_monotonic_us()),
            profile_submit_us, profile_emit_us, profile_sync_us);
    }

    return LV_RES_OK;
}

bool is_fix_angle(int angle) {
    if (angle == 0 || angle == 900 || angle == 1800 ||angle == 2700)
        return true;
    else
        return false;
}

static lv_res_t hw_decode(lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t *draw_dsc,
                    const void *src, const lv_area_t *blend_area, const lv_area_t *coords)
{
    struct decode_config config = { 0 };
    int ret;
    struct mpp_decoder* dec;
    struct mpp_frame frame;
    img_info *info;
    int type;
    int lv_ret = LV_RES_OK;

    info = img_info_init(src);
    if(!info) return LV_RES_INV;

    type = info->type ?
        MPP_CODEC_VIDEO_DECODER_PNG:MPP_CODEC_VIDEO_DECODER_MJPEG;

    config.bitstream_buffer_size = (info->img_size + 1023) & (~1023);
    config.extra_frame_num = 0;
    config.packet_count = 1;
    if(type == MPP_CODEC_VIDEO_DECODER_MJPEG)
        config.pix_fmt = MPP_FMT_YUV420P;
    else if(type == MPP_CODEC_VIDEO_DECODER_PNG)
        config.pix_fmt = MPP_FMT_ARGB_8888;

    dec = mpp_decoder_create(type);

    if (!dec) {
        lv_ret = LV_RES_INV;
        LV_LOG_ERROR("mpp_decoder_create failed\n");
        goto free_img;
    }
    mpp_decoder_init(dec, &config);

    struct mpp_packet packet;
    mpp_decoder_get_packet(dec, &packet, info->img_size);
    uint32_t len = 0;
    lv_fs_read(&(info->fp), packet.data, info->img_size, &len);
    packet.size = info->img_size;
    packet.flag = PACKET_FLAG_EOS;

    mpp_decoder_put_packet(dec, &packet);
    mpp_decoder_decode(dec);
    ret = mpp_decoder_get_frame(dec, &frame);
    if (ret < 0) {
        lv_ret = LV_RES_INV;
        LV_LOG_ERROR("mpp decoder get frame failed:%d\n", ret);
        goto free_dec;
    }

    if (is_fix_angle(draw_dsc->angle))
        lv_ret = ge_run_blit(draw_ctx, draw_dsc, &frame, blend_area, coords);
    else if (draw_dsc->angle > 0 && draw_dsc->zoom == LV_IMG_ZOOM_NONE)
        lv_ret = ge_run_rotate(draw_ctx, draw_dsc, &frame, blend_area, coords);
    else
        lv_ret = LV_RES_INV;

    mpp_decoder_put_frame(dec, &frame);
free_dec:
    mpp_decoder_destory(dec);
free_img:
    img_info_deinit(info);

    return lv_ret;
}

static bool file_type = false;

lv_res_t lv_draw_aic_draw_img(lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
                              const lv_area_t * coords, const void *src)
{
    /*Use the clip area as draw area*/
    lv_area_t draw_area;
    bool fake_image = false;
    char* ptr = NULL;
    lv_img_src_t src_type;
    const char *dma_profile_name = NULL;

    src_type = lv_img_src_get_type(src);
    if (src_type == LV_IMG_SRC_VARIABLE) {
        const lv_img_dsc_t *img_dsc = src;

        file_type = false;
        if (img_dsc != NULL &&
            ge_dma_image_lookup(img_dsc->data, &dma_profile_name) != NULL) {
            g_profile_image_source = dma_profile_name;
        } else {
            g_profile_image_source = NULL;
        }
        return LV_RES_INV;
    }

    if (src_type != LV_IMG_SRC_FILE) {
        file_type = false;
        g_profile_image_source = NULL;
        return LV_RES_INV;
    } else {
        file_type = true;
        g_profile_image_source = src;
    }

    ptr = strrchr(src, '.');
    if (ptr == NULL) return LV_RES_INV;

    if (!strcmp(ptr, ".fake"))
        fake_image = true;
    else
        return LV_RES_INV;

    lv_area_copy(&draw_area, draw_ctx->clip_area);

    bool mask_any = lv_draw_mask_is_any(&draw_area);
    bool transform = draw_dsc->angle != 0 || draw_dsc->zoom != LV_IMG_ZOOM_NONE ? true : false;

    lv_area_t b_area;
    lv_draw_sw_blend_dsc_t blend_dsc;

    lv_memset_00(&blend_dsc, sizeof(lv_draw_sw_blend_dsc_t));
    blend_dsc.opa = draw_dsc->opa;
    blend_dsc.blend_mode = draw_dsc->blend_mode;
    blend_dsc.blend_area = &b_area;

    if (!mask_any && draw_dsc->recolor_opa == LV_OPA_TRANSP) {
        blend_dsc.src_buf = NULL;
        blend_dsc.blend_area = coords;
        lv_area_t blend_area;

        if (!transform) {
            //lv_area_copy(&blend_area, coords);
            if (!_lv_area_intersect(&blend_area, blend_dsc.blend_area, draw_ctx->clip_area)) {
                return LV_RES_INV;
            }
        } else {
            lv_area_copy(&blend_area, draw_ctx->clip_area);
        }

        if (blend_dsc.mask_buf == NULL && blend_dsc.blend_mode == LV_BLEND_MODE_NORMAL
            && draw_target_is_display_buffer(draw_ctx)) {

            if (fake_image) {
                int width;
                int height;
                int blend;
                unsigned int color;

                sscanf(src + 3, "%dx%d_%d_%08x", &width, &height, &blend, &color);
                return ge_run_fill(draw_ctx, color, (color >> 24) & 0xff, blend, &blend_area);
            } else {
                return hw_decode(draw_ctx, draw_dsc, src, &blend_area, coords);
            }
        } else {
            LV_LOG_USER("lv res inv\n");
            return LV_RES_INV;
        }
    }
    return LV_RES_OK;
}

LV_ATTRIBUTE_FAST_MEM void lv_draw_aic_img_decoded(struct _lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * draw_dsc,
                                                  const lv_area_t * coords, const uint8_t *src_buf, lv_img_cf_t cf)
{
    lv_area_t draw_area;
    bool mask_any;
    lv_area_t b_area;
    lv_draw_sw_blend_dsc_t blend_dsc;
    const struct mpp_frame *registered_frame;
    const char *dma_profile_name = NULL;

    registered_frame = ge_dma_image_lookup(src_buf, &dma_profile_name);
    if (registered_frame != NULL) {
        g_profile_image_source = dma_profile_name;
        lv_area_copy(&draw_area, draw_ctx->clip_area);
        mask_any = lv_draw_mask_is_any(&draw_area);

        if (!mask_any && draw_dsc->recolor_opa == LV_OPA_TRANSP &&
            draw_dsc->blend_mode == LV_BLEND_MODE_NORMAL &&
            draw_target_is_display_buffer(draw_ctx)) {
            lv_area_t blend_area;

            lv_area_copy(&blend_area, draw_ctx->clip_area);
            if (is_fix_angle(draw_dsc->angle)) {
                ge_run_blit(draw_ctx, draw_dsc,
                            (struct mpp_frame *)registered_frame,
                            &blend_area, coords);
                return;
            }
        }

        draw_dma_frame_software(draw_ctx, draw_dsc, coords,
                                registered_frame, cf);
        return;
    }

    if (!file_type) {
        lv_draw_sw_img_decoded(draw_ctx, draw_dsc, coords, src_buf, cf);
        return;
    }

    lv_area_copy(&draw_area, draw_ctx->clip_area);
    mask_any = lv_draw_mask_is_any(&draw_area);

    lv_memset_00(&blend_dsc, sizeof(lv_draw_sw_blend_dsc_t));
    blend_dsc.opa = draw_dsc->opa;
    blend_dsc.blend_mode = draw_dsc->blend_mode;
    blend_dsc.blend_area = &b_area;

    if (!mask_any && draw_dsc->recolor_opa == LV_OPA_TRANSP) {
        blend_dsc.src_buf = NULL;
        blend_dsc.blend_area = coords;
        lv_area_t blend_area;

        lv_area_copy(&blend_area, draw_ctx->clip_area);
        if (blend_dsc.mask_buf == NULL && blend_dsc.blend_mode == LV_BLEND_MODE_NORMAL
            && draw_target_is_display_buffer(draw_ctx)) {

            struct mpp_frame frame;
            memcpy(&frame, src_buf, sizeof(frame));

            if (is_fix_angle(draw_dsc->angle)) {
                ge_run_blit(draw_ctx, draw_dsc, &frame, &blend_area, coords);
            } else if (draw_dsc->angle > 0 && draw_dsc->zoom == LV_IMG_ZOOM_NONE) {
                ge_run_rotate(draw_ctx, draw_dsc, &frame, &blend_area, coords);
            } else {
                LV_LOG_ERROR("unsupported angle:%d zoom:%d\n", draw_dsc->angle, draw_dsc->zoom);
                return;
            }
        } else {
            const struct mpp_frame *frame = (const struct mpp_frame *)src_buf;

            draw_dma_frame_software(draw_ctx, draw_dsc, coords, frame, cf);
        }
    }

    return;
}

void lv_draw_aic_blend(lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc)
{
    lv_area_t blend_area;
    bool done = false;
    bool prefer_sw_fill = false;

    if (dsc->mask_buf && dsc->mask_res == LV_DRAW_MASK_RES_TRANSP)
        return;

    /*Let's get the blend area which is the intersection of the area to fill and the clip area.*/
    if (!_lv_area_intersect(&blend_area, dsc->blend_area, draw_ctx->clip_area))
        return; /*Fully clipped, nothing to do */

    if (dsc->src_buf == NULL && dsc->mask_buf == NULL &&
        dsc->blend_mode == LV_BLEND_MODE_NORMAL &&
        dsc->opa >= LV_OPA_MAX &&
        (uint32_t)lv_area_get_width(&blend_area) *
            (uint32_t)lv_area_get_height(&blend_area) <=
            GE_FILL_CPU_THRESHOLD_PIXELS) {
        prefer_sw_fill = true;
    }

    /*Make the blend area relative to the buffer*/
    lv_area_move(&blend_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);

    if (!prefer_sw_fill &&
        dsc->mask_buf == NULL &&
        dsc->blend_mode == LV_BLEND_MODE_NORMAL &&
        draw_target_is_display_buffer(draw_ctx)) {
        int ret = LV_RES_INV;
        unsigned int color = dsc->color.full;
        unsigned char opa = dsc->opa;

        if (!dsc->src_buf)
           ret = ge_run_fill(draw_ctx, color, opa, 1, &blend_area);
        if (ret == LV_RES_OK)
            done = true;
    }

    if (!done)
        lv_draw_sw_blend_basic(draw_ctx, dsc);

}
