/*
 * Copyright (C) 2022-2023 ArtinChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include "lvgl/lvgl.h"
#include "mpp_ge.h"
#include "mpp_decoder.h"
#include "frame_allocator.h"
#include "dma_allocator.h"
#include "aic_dec.h"
#include "image_memory.h"
#include "image_recovery.h"
#include "aic_ui.h"
#include "lv_ge2d.h"
#include "aic_ui/perf_stats.h"
#include "un260/lv_system/app_clock.h"
#include "aic_ui/compiled_asset.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <limits.h>
/* This SDK's init may return success with pm == NULL. Inspect its matching
 * source definition before calling get_packet, which otherwise dereferences it. */
#include "../../aic-mpp/ve/include/mpp_codec.h"
#define AIC_IMAGE_CACHE_BUDGET (8U * 1024U * 1024U)
static lv_img_decoder_t *g_aic_decoder;

#define PNG_HEADER_SIZE (8 + 12 + 13) //png signature + IHDR chuck
#define PNGSIG 0x89504e470d0a1a0aull
#define MNGSIG 0x8a4d4e470d0a1a0aull
#define JPEG_SOI 0xFFD8
#define JPEG_SOF 0xFFC0

static inline uint64_t stream_to_u64(uint8_t *ptr)
{
    return ((uint64_t)ptr[0] << 56) | ((uint64_t)ptr[1] << 48) |
           ((uint64_t)ptr[2] << 40) | ((uint64_t)ptr[3] << 32) |
           ((uint64_t)ptr[4] << 24) | ((uint64_t)ptr[5] << 16) |
           ((uint64_t)ptr[6] << 8) | ((uint64_t)ptr[7]);
}

static inline unsigned int stream_to_u32(uint8_t *ptr)
{
    return ((unsigned int)ptr[0] << 24) |
           ((unsigned int)ptr[1] << 16) |
           ((unsigned int)ptr[2] << 8) |
           (unsigned int)ptr[3];
}

static inline unsigned short stream_to_u16(uint8_t *ptr)
{
    return  ((unsigned int)ptr[0] << 8) | (unsigned int)ptr[1];
}

static int get_jpeg_format(uint8_t *buf, enum mpp_pixel_format *pix_fmt)
{
    int i;
    uint8_t h_count[3] = { 0 };
    uint8_t v_count[3]= { 0 };
    uint8_t nb_components = *buf++;
    if(nb_components != 1 && nb_components != 3) return -1;

    for (i = 0; i < nb_components; i++) {
        uint8_t h_v_cnt;

        /* skip component id */
        buf++;
        h_v_cnt = *buf++;
        h_count[i] = h_v_cnt >> 4;
        v_count[i] = h_v_cnt & 0xf;

        /*skip quant_index*/
        buf++;
    }

    if (h_count[0] == 2 && v_count[0] == 2 && h_count[1] == 1 &&
        v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
        *pix_fmt = MPP_FMT_YUV420P;
    } else if (h_count[0] == 4 && v_count[0] == 1 && h_count[1] == 1 &&
               v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
        return -1;
    } else if (h_count[0] == 2 && v_count[0] == 1 && h_count[1] == 1 &&
               v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
        *pix_fmt = MPP_FMT_YUV422P;
    } else if (h_count[0] == 1 && v_count[0] == 1 && h_count[1] == 1 &&
               v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
        *pix_fmt = MPP_FMT_YUV444P;
    } else if (h_count[0] == 1 && v_count[0] == 2 && h_count[1] == 1 &&
               v_count[1] == 2 && h_count[2] == 1 && v_count[2] == 2) {
        *pix_fmt = MPP_FMT_YUV444P;
    } else if (h_count[0] == 1 && v_count[0] == 2 && h_count[1] == 1 &&
               v_count[1] == 1 && h_count[2] == 1 && v_count[2] == 1) {
        return -1;
    } else if (h_count[1] == 0 && v_count[1] == 0 && h_count[2] == 0 &&
               v_count[2] == 0) {
        *pix_fmt = MPP_FMT_YUV400;
    } else {
        printf("Not support format! h_count: %d %d %d, v_count: %d %d %d\n",
            h_count[0], h_count[1], h_count[2],
            v_count[0], v_count[1], v_count[2]);
        return -1;
    }

    return 0;
}

static lv_fs_res_t jpeg_get_img_size(lv_fs_file_t *fp, int *w, int *h, enum mpp_pixel_format *pix_fmt)
{
    uint32_t read_num;
    uint8_t buf[128];
    lv_fs_res_t res = LV_FS_RES_OK;

    // read JPEG SOI
    res = lv_fs_read(fp, buf, 2, &read_num);
    if (res != LV_FS_RES_OK || read_num != 2) {
        res = LV_FS_RES_INV_PARAM;
        goto read_err;
    }

    /* check SOI */
    if (stream_to_u16(buf) != JPEG_SOI) {
        res = LV_FS_RES_INV_PARAM;
        goto read_err;
    }

    /* find SOF */
    while (1) {
        int size;
        res = lv_fs_read(fp, buf, 4, &read_num);
        if (res != LV_FS_RES_OK || read_num != 4) {
            res = LV_FS_RES_INV_PARAM;
            goto read_err;
        }

        if (stream_to_u16(buf) == JPEG_SOF) {
            res = lv_fs_read(fp, buf, 15, &read_num);
            if (res != LV_FS_RES_OK || read_num != 15) {
                res = LV_FS_RES_INV_PARAM;
                goto read_err;
            }

            *h = stream_to_u16(buf + 1);
            *w = stream_to_u16(buf + 3);

            if(get_jpeg_format(buf + 5, pix_fmt) < 0) res = LV_FS_RES_INV_PARAM;
            break;
        } else {
            size = stream_to_u16(buf + 2);
            if(size < 2) return LV_FS_RES_INV_PARAM;
            res = lv_fs_seek(fp, size - 2, SEEK_CUR);
            if (res != LV_FS_RES_OK ) {
                res = LV_FS_RES_INV_PARAM;
                goto read_err;
            }
        }
    }

read_err:
    return res;
}

static lv_res_t jpeg_decoder_info(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header)
{
    lv_fs_file_t f;
    lv_fs_res_t res;
    int width;
    int height;
    enum mpp_pixel_format fomat;

    res = lv_fs_open(&f, src, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK)
        return LV_RES_INV;

    res = jpeg_get_img_size(&f, &width, &height, &fomat);
    if (res != LV_FS_RES_OK || width <= 0 || height <= 0) {
        lv_fs_close(&f);
        return LV_RES_INV;
    }

    header->w = width;
    header->h = height;
    header->cf = LV_IMG_CF_TRUE_COLOR;
    lv_fs_close(&f);

    return LV_RES_OK;
}

static lv_fs_res_t png_get_img_size(lv_fs_file_t *fp, int *w, int *h, enum mpp_pixel_format *fomat)
{
    uint32_t read_num;
    unsigned char buf[64];
    int color_type;

    if(lv_fs_read(fp, buf, PNG_HEADER_SIZE, &read_num) != LV_FS_RES_OK ||
       read_num != PNG_HEADER_SIZE || stream_to_u64(buf) != PNGSIG ||
       stream_to_u32(buf + 8) != 13 || memcmp(buf + 12, "IHDR", 4))
        return LV_FS_RES_INV_PARAM;

    *w = stream_to_u32(buf + 8 + 8);
    *h = stream_to_u32(buf + 8 + 8 + 4);
    if(*w <= 0 || *h <= 0 || *w > 4096 || *h > 4096) return LV_FS_RES_INV_PARAM;

    color_type = buf[8 + 8 + 8 + 1];
    if (color_type == 2)
        *fomat = MPP_FMT_RGB_888;
    else
        *fomat = MPP_FMT_ARGB_8888;

    /* File helpers use FS status (OK=0), not draw status (OK=1). */
    return LV_FS_RES_OK;
}

static lv_fs_res_t get_file_size(lv_fs_file_t *fp, unsigned int *file_size)
{
    if(lv_fs_seek(fp, 0, SEEK_END) != LV_FS_RES_OK ||
       lv_fs_tell(fp, file_size) != LV_FS_RES_OK ||
       lv_fs_seek(fp, 0, SEEK_SET) != LV_FS_RES_OK) return LV_FS_RES_FS_ERR;

    return LV_FS_RES_OK;
}

static lv_res_t fake_decoder_info(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header)
{
    int width;
    int height;
    int blend;
    unsigned int color;
    int ret;

    FAKE_IMAGE_PARSE(src, ret, width, height, blend, color);
    header->w = width;
    header->h = height;
    header->cf = LV_IMG_CF_TRUE_COLOR;
    ret = LV_RES_OK;

    return ret;
}

static lv_res_t png_decoder_info(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header)
{
    lv_fs_file_t f;
    lv_fs_res_t res;
    uint32_t read_num;
    uint8_t buf[64];

    res = lv_fs_open(&f, src, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK)
        return LV_RES_INV;

    // read png sig + IHDR chuck
    res = lv_fs_read(&f, buf, PNG_HEADER_SIZE, &read_num);
    if (res != LV_FS_RES_OK || read_num != PNG_HEADER_SIZE) {
        lv_fs_close(&f);
        return LV_RES_INV;
    }

    /* check signature */
    uint64_t sig = stream_to_u64(buf);
    if (sig != PNGSIG && sig != MNGSIG) {
        LV_LOG_WARN("Invalid PNG signature 0x%08llx.", (unsigned long long)sig);
        lv_fs_close(&f);
        return LV_RES_INV;
    }

    header->w = stream_to_u32(buf + 8 + 8);
    header->h = stream_to_u32(buf + 8 + 8 + 4);
    header->cf = LV_IMG_CF_RAW;

    lv_fs_close(&f);
    return LV_RES_OK;
}

static lv_res_t aic_decoder_info(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header)
{
    char* ptr = NULL;

    if (lv_img_src_get_type(src) != LV_IMG_SRC_FILE) {
        return LV_RES_INV;
    }

    const un260_compiled_asset_t *asset = un260_compiled_asset_find(src);
    if (asset) {
        header->always_zero = 0;
        header->w = asset->width;
        header->h = asset->height;
        header->cf = asset->has_alpha ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;
        return LV_RES_OK;
    }

    ptr = strrchr(src, '.');
    if (!ptr) return LV_RES_INV;
    if (!strcmp(ptr, ".png")) {
        return png_decoder_info(decoder, src, header);
    } else if ((!strcmp(ptr, ".jpg")) || (!strcmp(ptr, ".jpeg"))) {
        return jpeg_decoder_info(decoder, src, header);
    } else if (!strcmp(ptr, ".fake")) {
        return fake_decoder_info(decoder, src, header);
    } else {
        return LV_RES_INV;
    }

    return LV_RES_OK;
}

struct ext_frame_allocator {
    struct frame_allocator base;
    struct mpp_frame* frame;
};

static int alloc_frame_buffer(struct frame_allocator *p, struct mpp_frame* frame,
                              int width, int height, enum mpp_pixel_format format)
{
    struct ext_frame_allocator* impl = (struct ext_frame_allocator*)p;

    memcpy(frame, impl->frame, sizeof(struct mpp_frame));
    return 0;
}

static int free_frame_buffer(struct frame_allocator *p, struct mpp_frame *frame)
{
    return 0;
}

static int close_allocator(struct frame_allocator *p)
{
    /* Owned by the decode attempt, including failures before fm_create. */
    (void)p;
    return 0;
}

static struct alloc_ops def_ops = {
    .alloc_frame_buffer = alloc_frame_buffer,
    .free_frame_buffer = free_frame_buffer,
    .close_allocator = close_allocator,
};

static struct frame_allocator* open_allocator(struct mpp_frame* frame)
{
    struct ext_frame_allocator* impl = (struct ext_frame_allocator*)malloc(sizeof(struct ext_frame_allocator));

    if(impl == NULL) {
        return NULL;
    }

    memset(impl, 0, sizeof(struct ext_frame_allocator));

    impl->frame = frame;
    impl->base.ops = &def_ops;
    return &impl->base;
}

static uint32_t frame_dma_bytes(const struct mpp_frame *frame)
{
    uint64_t plane = (uint64_t)frame->buf.stride[0] * frame->buf.size.height;
    uint64_t total = (plane + 4095U) & ~4095ULL;
    if(frame->buf.format == MPP_FMT_YUV420P) total += 2 * ((plane / 4 + 4095) & ~4095ULL);
    else if(frame->buf.format == MPP_FMT_YUV422P) total += 2 * ((plane / 2 + 4095) & ~4095ULL);
    else if(frame->buf.format == MPP_FMT_YUV444P) total *= 3;
    return total > UINT32_MAX ? UINT32_MAX : (uint32_t)total;
}

static uint32_t aic_cache_bytes(const lv_img_decoder_dsc_t *dsc)
{
    if(!dsc->img_data || dsc->decoder != g_aic_decoder) return 0;
    return frame_dma_bytes((const struct mpp_frame *)dsc->img_data);
}

static lv_res_t compiled_asset_open(lv_img_decoder_dsc_t *dsc,
                                    const un260_compiled_asset_t *asset)
{
    struct mpp_frame *frame = calloc(1, sizeof(*frame));
    unsigned char *pixels;
    int heap = dmabuf_device_open();
    uint64_t started = app_clock_monotonic_us();
    if (!frame) { if (heap >= 0) close(heap); return LV_RES_INV; }
    frame->buf.size.width = asset->width;
    frame->buf.size.height = asset->height;
    frame->buf.stride[0] = (asset->stride + 15U) & ~15U;
    /* Both formats are supported by this SDK's DMA allocator and GE. */
    frame->buf.format = asset->has_alpha ? MPP_FMT_ARGB_8888 : MPP_FMT_RGB_888;
    uint32_t managed_bytes = frame_dma_bytes(frame);
    if(!lv_img_cache_reserve_bytes(managed_bytes, AIC_IMAGE_CACHE_BUDGET) ||
       !image_mem_acquire(IMAGE_MEM_IMAGE, managed_bytes)) {
        if(heap >= 0) close(heap);
        free(frame); return LV_RES_INV;
    }
    int allocated = heap >= 0 ? mpp_buf_alloc(heap, &frame->buf) : -1;
    if (heap >= 0) close(heap);
    if (allocated < 0) {
        heap = open("/dev/dma_heap/reserved", O_RDWR | O_CLOEXEC);
        allocated = heap >= 0 ? mpp_buf_alloc(heap, &frame->buf) : -1;
        if (heap >= 0) close(heap);
    }
    if (allocated < 0) { image_mem_release(IMAGE_MEM_IMAGE, managed_bytes); free(frame); return LV_RES_INV; }
    uint32_t bytes = frame->buf.stride[0] * asset->height;
    pixels = dmabuf_mmap(frame->buf.fd[0], bytes);
    if (!pixels) { mpp_buf_free(&frame->buf); image_mem_release(IMAGE_MEM_IMAGE, managed_bytes); free(frame); return LV_RES_INV; }
    struct dma_buf_sync cpu_access = {.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE};
    if (ioctl(frame->buf.fd[0], DMA_BUF_IOCTL_SYNC, &cpu_access) < 0) {
        dmabuf_munmap(pixels, bytes); mpp_buf_free(&frame->buf); image_mem_release(IMAGE_MEM_IMAGE, managed_bytes); free(frame); return LV_RES_INV;
    }
    for (uint32_t y = 0; y < asset->height; y++) {
        memcpy(pixels + y * frame->buf.stride[0], asset->pixels + y * asset->stride, asset->stride);
        memset(pixels + y * frame->buf.stride[0] + asset->stride, 0,
               frame->buf.stride[0] - asset->stride);
    }
    int clean = dmabuf_sync(frame->buf.fd[0], CACHE_CLEAN);
    dmabuf_munmap(pixels, bytes);
    if (clean < 0) { mpp_buf_free(&frame->buf); image_mem_release(IMAGE_MEM_IMAGE, managed_bytes); free(frame); return LV_RES_INV; }
    dsc->header.cf = asset->has_alpha ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;
    dsc->img_data = (const unsigned char *)frame;
    if (perf_profile_is_enabled())
        perf_profile_report_image_decode(dsc->src,
            app_clock_elapsed_us32(started, app_clock_monotonic_us()), bytes);
    return LV_RES_OK;
}

static lv_res_t aic_decoder_attempt(lv_img_decoder_dsc_t *dsc,
                                    const char **stage, int *error, uint32_t *bytes)
{
    lv_fs_file_t file;
    bool file_open = false, allocated = false, success = false, claimed = false;
    uint32_t decode_claim = 0;
    int width = 0, height = 0, heap = -1, ret = -1;
    struct mpp_decoder *dec = NULL;
    struct frame_allocator *allocator = NULL;
    struct mpp_frame *output = NULL, frame = {0};
    struct mpp_packet packet = {0};
    struct decode_config config = {0};
    uint32_t len = 0, read_size = 0;
    enum mpp_codec_type type = MPP_CODEC_VIDEO_DECODER_PNG;
    const char *ext = strrchr(dsc->src, '.');
    uint64_t started = app_clock_monotonic_us();
    dsc->img_data = NULL;
    *bytes = 0;
    *stage = "extension"; *error = EINVAL;
    if(!ext) return LV_RES_INV;
    if(!strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg")) type = MPP_CODEC_VIDEO_DECODER_MJPEG;
    else if(strcmp(ext, ".png")) return LV_RES_INV;

    *stage = "file-open";
    ret = lv_fs_open(&file, dsc->src, LV_FS_MODE_RD);
    if(ret != LV_FS_RES_OK) goto cleanup;
    file_open = true;
    *stage = "header";
    ret = type == MPP_CODEC_VIDEO_DECODER_PNG ?
          png_get_img_size(&file, &width, &height, &config.pix_fmt) :
          jpeg_get_img_size(&file, &width, &height, &config.pix_fmt);
    if(ret != LV_FS_RES_OK || width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        if(!ret) ret = -EINVAL;
        goto cleanup;
    }
    *stage = "file-size";
    ret = get_file_size(&file, &len);
    if(ret != LV_FS_RES_OK || len == 0 || len > 16U*1024U*1024U) {
        if(!ret) ret = -EINVAL;
        goto cleanup;
    }
    *stage = "frame-metadata";
    output = calloc(1, sizeof(*output));
    if(!output) { ret = -ENOMEM; goto cleanup; }
    output->buf.size.width = width;
    output->buf.size.height = type == MPP_CODEC_VIDEO_DECODER_PNG ? height : (height + 15) & ~15;
    output->buf.format = config.pix_fmt;
    if(type == MPP_CODEC_VIDEO_DECODER_PNG) {
        output->buf.stride[0] = (width * (config.pix_fmt == MPP_FMT_ARGB_8888 ? 4 : 3) + 15) & ~15;
    } else {
        output->buf.stride[0] = (width + 15) & ~15;
        if(config.pix_fmt == MPP_FMT_YUV420P || config.pix_fmt == MPP_FMT_YUV422P)
            output->buf.stride[1] = output->buf.stride[2] = output->buf.stride[0] / 2;
        else if(config.pix_fmt == MPP_FMT_YUV444P)
            output->buf.stride[1] = output->buf.stride[2] = output->buf.stride[0];
        else if(config.pix_fmt != MPP_FMT_YUV400) { ret = -EINVAL; goto cleanup; }
    }
    *bytes = frame_dma_bytes(output);
    *stage = "cache-budget";
    if(!lv_img_cache_reserve_bytes(*bytes, AIC_IMAGE_CACHE_BUDGET)) { ret = -ENOMEM; goto cleanup; }
    if(!image_mem_acquire(IMAGE_MEM_IMAGE, *bytes)) { ret = -ENOMEM; goto cleanup; }
    claimed = true;
    /* Bitstream plus conservative PNG/JPEG decoder scratch reservation.
     * The global unclaimed headroom additionally covers SDK-private buffers. */
    uint32_t need_decode = ((len + 8U + 1023U) & ~1023U) + 1024U*1024U;
    if(!image_mem_acquire(IMAGE_MEM_DECODE, need_decode)) { ret = -ENOMEM; goto cleanup; }
    decode_claim = need_decode;
    *stage = "heap-open";
    heap = dmabuf_device_open();
    if(heap < 0) { ret = -errno; goto cleanup; }
    *stage = "output-dma";
    errno = 0;
    ret = mpp_buf_alloc(heap, &output->buf);
    if(ret < 0) { ret = errno ? -errno : -ENOMEM; goto cleanup; }
    allocated = true;
    *stage = "decoder-create";
    dec = mpp_decoder_create(type);
    if(!dec) { ret = -ENOMEM; goto cleanup; }
    *stage = "allocator";
    allocator = open_allocator(output);
    if(!allocator) { ret = -ENOMEM; goto cleanup; }
    *stage = "decoder-control";
    ret = mpp_decoder_control(dec, MPP_DEC_INIT_CMD_SET_EXT_FRAME_ALLOCATOR, allocator);
    if(ret != 0) goto cleanup;
    /* packet_manager reserves 8 readable bytes after each packet. Aligning
     * len alone rejects valid files at/near a 1KiB boundary. */
    config.bitstream_buffer_size = (len + 8U + 1023U) & ~1023U;
    config.extra_frame_num = 0;
    config.packet_count = 1;
    *stage = "decoder-init";
    ret = mpp_decoder_init(dec, &config);
    if(ret != 0 || !dec->pm) { if(!ret) ret = -ENOMEM; goto cleanup; }
    *stage = "packet";
    ret = mpp_decoder_get_packet(dec, &packet, len);
    if(ret != 0 || !packet.data) { if(!ret) ret = -ENOMEM; goto cleanup; }
    *stage = "file-read";
    ret = lv_fs_read(&file, packet.data, len, &read_size);
    if(ret != LV_FS_RES_OK || read_size != len) { if(!ret) ret = -EIO; goto cleanup; }
    /* First and only packet in this fresh decoder: the reserved tail belongs
     * to this bitstream allocation, but is not part of packet.size. */
    memset((unsigned char *)packet.data + len, 0, 8);
    packet.size = len;
    packet.flag = PACKET_FLAG_EOS;
    *stage = "put-packet";
    ret = mpp_decoder_put_packet(dec, &packet);
    if(ret != 0) goto cleanup;
    *stage = "decode";
    ret = mpp_decoder_decode(dec);
    if(ret != 0) goto cleanup;
    *stage = "get-frame";
    ret = mpp_decoder_get_frame(dec, &frame);
    if(ret != 0) goto cleanup;
    *stage = "put-frame";
    ret = mpp_decoder_put_frame(dec, &frame);
    if(ret != 0) goto cleanup;
    success = true;
cleanup:
    *error = ret;
    /* Decoder/frame manager must release its references before the external
     * frame is freed. We own allocator even if fm_create never happened. */
    if(dec) mpp_decoder_destory(dec);
    free(allocator);
    if(heap >= 0) dmabuf_device_close(heap);
    if(file_open) lv_fs_close(&file);
    image_mem_release(IMAGE_MEM_DECODE, decode_claim);
    if(!success) {
        if(allocated) mpp_buf_free(&output->buf);
        if(claimed) image_mem_release(IMAGE_MEM_IMAGE, *bytes);
        free(output);
        return LV_RES_INV;
    }
    dsc->header.cf = type == MPP_CODEC_VIDEO_DECODER_PNG && config.pix_fmt == MPP_FMT_ARGB_8888 ?
                     LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;
    dsc->img_data = (const unsigned char *)output;
    if(perf_profile_is_enabled())
        perf_profile_report_image_decode(dsc->src, app_clock_elapsed_us32(started, app_clock_monotonic_us()), *bytes);
    return LV_RES_OK;
}

/* Small failure backoff prevents every redraw from hammering a depleted heap.
 * No successful image is hidden: failed sources get a fresh attempt after 1s. */
typedef struct { uint64_t hash; uint32_t tick; bool valid; } image_failure_t;
static image_failure_t image_failures[8];
static unsigned failure_next;
static uint64_t image_source_hash(const char *path)
{
    uint64_t h = 14695981039346656037ULL;
    while(*path) { h ^= (unsigned char)*path++; h *= 1099511628211ULL; }
    return h;
}
static lv_res_t aic_decoder_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{
    (void)decoder;
    uint64_t hash = image_source_hash(dsc->src);
    for(unsigned i = 0; i < 8; ++i) {
        if(image_failures[i].valid && image_failures[i].hash == hash) {
            if(lv_tick_elaps(image_failures[i].tick) < 1000) return LV_RES_INV;
            image_failures[i].valid = false;
        }
    }
    const un260_compiled_asset_t *asset = un260_compiled_asset_find(dsc->src);
    const char *stage = "compiled-asset";
    int error = 0;
    uint32_t bytes = asset ? ((asset->stride + 15U) & ~15U) * asset->height : 0;
    for(unsigned attempt = 0; attempt < 2; ++attempt) {
        lv_res_t result = asset ? compiled_asset_open(dsc, asset) :
                                 aic_decoder_attempt(dsc, &stage, &error, &bytes);
        if(result == LV_RES_OK) {
            if(attempt) fprintf(stderr, "IMG_RECOVER src=%s bytes=%u cache=%u\n",
                               (const char *)dsc->src, bytes, lv_img_cache_memory_used());
            return result;
        }
        bool memory_candidate = asset || error == -ENOMEM || !strcmp(stage, "decode") ||
                                !strcmp(stage, "packet") || !strcmp(stage, "decoder-init");
        uint32_t freed = 0;
        if(!attempt && memory_candidate)
            freed = lv_img_cache_reclaim_bytes(bytes > UINT32_MAX-2097152U ? UINT32_MAX : bytes+2097152U);
        fprintf(stderr, "IMG_FAIL src=%s stage=%s ret=%d bytes=%u cache=%u reclaimed=%u attempt=%u\n",
                (const char *)dsc->src, stage, error, bytes, lv_img_cache_memory_used(), freed, attempt+1);
        if(!freed) break;
    }
    image_failures[failure_next++ % 8] = (image_failure_t){hash, lv_tick_get(), true};
    return LV_RES_INV;
}

static void aic_decoder_close(lv_img_decoder_t * decoder, lv_img_decoder_dsc_t * dsc)
{
    if (dsc->img_data) {
        struct mpp_frame *alloc_frame = (struct mpp_frame *)dsc->img_data;
        lv_ge2d_scaled_cache_drop_source(alloc_frame);
        mpp_buf_free(&alloc_frame->buf);
        image_mem_release(IMAGE_MEM_IMAGE, frame_dma_bytes(alloc_frame));
        free(alloc_frame);
        dsc->img_data = NULL;
    }

    return;
}

static void reclaim_images(uint32_t bytes) { (void)lv_img_cache_reclaim_bytes(bytes); }
void aic_dec_create()
{
    image_mem_register(IMAGE_MEM_IMAGE, reclaim_images);
    image_recovery_init();
    fprintf(stderr, "ASSET_C count=%u raw_bytes=%u format=ARGB8888 lazy_dma=1\n",
            un260_compiled_asset_count(), un260_compiled_asset_bytes());
    lv_img_decoder_t *aic_dec = lv_img_decoder_create();
    if(!aic_dec) return;
    g_aic_decoder = aic_dec;
    lv_img_cache_set_memory_cb(aic_cache_bytes);
    fprintf(stderr, "IMG_CACHE build=RESILIENCE_R1 budget=%u managed_dma_mib=12 mpp_pool_mib=16 reclaim=unpinned retry=1 packet_tail=8\n", AIC_IMAGE_CACHE_BUDGET);

    lv_img_decoder_set_info_cb(aic_dec, aic_decoder_info);
    lv_img_decoder_set_open_cb(aic_dec, aic_decoder_open);
    lv_img_decoder_set_close_cb(aic_dec, aic_decoder_close);
}
