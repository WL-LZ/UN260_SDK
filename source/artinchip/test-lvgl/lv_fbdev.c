/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <stddef.h>
#include <sys/mman.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include "lv_fbdev.h"
#include "mpp_ge.h"
#include "lvgl/lvgl.h"
#include "lv_port_disp.h"

static struct mpp_ge *g_ge = NULL;
static int g_fb = -1;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static int g_buf_num = 1;

char *g_frame_buf[MAX_FRAME_NUM] = { NULL };
unsigned int g_frame_phy[MAX_FRAME_NUM] = { 0 };
int g_draw_buf_fd[2] = { -1 };
char *g_draw_buf[2] = { NULL };

#ifdef USE_DRAW_BUF
#define DMA_HEAP_DEV       "/dev/dma_heap/mpp"
#define DRAW_BUF_STRIDE    ((DRAW_BUF_WIDTH * (LV_COLOR_DEPTH / 8) + 7) & (~7))
#define DRAW_BUF_SWAP_STRIDE    ((DRAW_BUF_HEIGHT * (LV_COLOR_DEPTH / 8) + 7) & (~7))
#define DRAW_BUF_SIZE      (DRAW_BUF_STRIDE * DRAW_BUF_HEIGHT)

static int draw_buf_alloc(int id)
{
    int ret;
    struct dma_heap_allocation_data data = { 0 };
    int heap_fd = open(DMA_HEAP_DEV, O_RDWR);
    if (heap_fd < 0) {
        LV_LOG_ERROR("Failed to open %s, errno: %d[%s]\n",
                     DMA_HEAP_DEV, errno, strerror(errno));
        goto failed;
    }
    data.fd = 0;
    data.len = DRAW_BUF_SIZE;
    data.fd_flags = O_RDWR | O_CLOEXEC;
    data.heap_flags = 0;
    ret = ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &data);
    if (ret < 0) {
        LV_LOG_ERROR("ioctl() failed! errno: %d[%s]\n",
                     errno, strerror(errno));
        goto failed;
    }
    g_draw_buf[id] = mmap(NULL, data.len, PROT_READ|PROT_WRITE, MAP_SHARED, data.fd, 0);
    if (g_draw_buf[id] == MAP_FAILED) {
        LV_LOG_ERROR("mmap() failed! errno: %d[%s]\n",
                     errno, strerror(errno));
        goto failed;
    }
    g_draw_buf_fd[id] = data.fd;

failed:
    if (heap_fd >=0)
        close(heap_fd);

    if (g_draw_buf_fd[id] < 0 && data.fd >= 0) {
        close(data.fd);
        data.fd = -1;
    }
    return 0;
}

static int draw_buf_free(int id)
{
    if (g_draw_buf_fd[id] >= 0) {
        munmap(g_draw_buf[id], DRAW_BUF_SIZE);
        close(g_draw_buf_fd[id]);
        g_draw_buf_fd[id] = -1;
    }
    return 0;
}

#endif

int fbdev_open(void)
{
    char *fbp;

    g_fb = open(FBDEV_PATH, O_RDWR);
    if(g_fb == -1) {
        LV_LOG_ERROR("can't find aic framebuffer device!");
        return -1;
    }
    LV_LOG_INFO("The framebuffer device was opened successfully");

    if (ioctl(g_fb, FBIOGET_FSCREENINFO, &finfo) == -1) {
        LV_LOG_ERROR("Error reading fixed information");
        return -1;
    }

    if (ioctl(g_fb, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        LV_LOG_ERROR("Error reading variable information");
        return -1;
    }
    LV_LOG_INFO("%dx%d, %d %dbpp", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    fbp = (char *)mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb, 0);
    if((intptr_t)fbp == -1) {
        LV_LOG_ERROR("Error: failed to map framebuffer device to memory");
        return -1;
    }

    g_frame_buf[0] = fbp;
    g_frame_phy[0] = finfo.smem_start;

    // double frame buffer
    if (finfo.smem_len >= vinfo.yres * finfo.line_length * 2) {
        g_frame_buf[1] = g_frame_buf[0] + vinfo.yres * finfo.line_length;
        g_frame_phy[1] = g_frame_phy[0] + vinfo.yres * finfo.line_length;
    }

    // triple frame buffer
    if (finfo.smem_len >= vinfo.yres * finfo.line_length * 3) {
        g_frame_buf[2] = g_frame_buf[1] + vinfo.yres * finfo.line_length;
        g_frame_phy[2] = g_frame_phy[1] + vinfo.yres * finfo.line_length;
    }

    // only use one draw buffer right now
    g_buf_num = 1;
#ifdef USE_DRAW_BUF
    int i;
    for (i = 0; i < g_buf_num; i++)
        draw_buf_alloc(i);
#endif

    return g_fb;
}

int fbdev_get_size(int *width, int *height)
{
    *width = vinfo.xres;
    *height = vinfo.yres;
    return 0;
}

enum mpp_pixel_format fbdev_get_fmt(void)
{
    if (vinfo.bits_per_pixel == 32)
        return MPP_FMT_ARGB_8888;
    else if (vinfo.bits_per_pixel == 24)
        return MPP_FMT_RGB_888;
    else if (vinfo.bits_per_pixel == 16)
        return MPP_FMT_RGB_565;

    return MPP_FMT_ARGB_8888;
}

int fbdev_get_bpp(void)
{
    return vinfo.bits_per_pixel;
}

int fbdev_get_pitch(void)
{
    return finfo.line_length;
}

int fbdev_get_visible_buffer(struct mpp_buf *buffer)
{
    struct fb_var_screeninfo current_vinfo;
    uint32_t bytes_per_pixel;

    if (buffer == NULL || g_fb < 0 || g_frame_phy[0] == 0U) {
        errno = EINVAL;
        return -1;
    }
    if (ioctl(g_fb, FBIOGET_VSCREENINFO, &current_vinfo) == -1) {
        return -1;
    }
    bytes_per_pixel = current_vinfo.bits_per_pixel / 8U;
    if (current_vinfo.xres == 0U || current_vinfo.yres == 0U ||
        (bytes_per_pixel != 2U && bytes_per_pixel != 3U &&
         bytes_per_pixel != 4U)) {
        errno = EINVAL;
        return -1;
    }

    memset(buffer, 0, sizeof(*buffer));
    buffer->buf_type = MPP_PHY_ADDR;
    buffer->phy_addr[0] = g_frame_phy[0] +
        current_vinfo.yoffset * finfo.line_length +
        current_vinfo.xoffset * bytes_per_pixel;
    buffer->stride[0] = finfo.line_length;
    buffer->size.width = current_vinfo.xres;
    buffer->size.height = current_vinfo.yres;
    buffer->format = fbdev_get_fmt();
    return 0;
}

static int bmp_write_u16(FILE *fp, uint16_t value)
{
    uint8_t data[2] = {
        (uint8_t)(value & 0xffU),
        (uint8_t)((value >> 8) & 0xffU)
    };

    return fwrite(data, 1, sizeof(data), fp) == sizeof(data) ? 0 : -1;
}

static int bmp_write_u32(FILE *fp, uint32_t value)
{
    uint8_t data[4] = {
        (uint8_t)(value & 0xffU),
        (uint8_t)((value >> 8) & 0xffU),
        (uint8_t)((value >> 16) & 0xffU),
        (uint8_t)((value >> 24) & 0xffU)
    };

    return fwrite(data, 1, sizeof(data), fp) == sizeof(data) ? 0 : -1;
}

static uint8_t fbdev_component_to_u8(uint32_t pixel, struct fb_bitfield field)
{
    uint32_t value;
    uint32_t max_value;

    if (field.length == 0U) {
        return 0U;
    }

    value = (pixel >> field.offset) & ((1U << field.length) - 1U);
    max_value = (1U << field.length) - 1U;
    return (uint8_t)((value * 255U + max_value / 2U) / max_value);
}

int fbdev_save_bmp(const char *path)
{
    const uint32_t header_size = 14U + 40U;
    struct fb_var_screeninfo current_vinfo;
    uint32_t width;
    uint32_t height;
    uint32_t src_bytes_per_pixel;
    uint32_t row_size;
    uint32_t image_size;
    uint8_t *row = NULL;
    const uint8_t *frame;
    FILE *fp = NULL;
    uint32_t y;
    int result = -1;

    if (path == NULL || g_frame_buf[0] == NULL || g_fb < 0) {
        errno = EINVAL;
        return -1;
    }

    if (ioctl(g_fb, FBIOGET_VSCREENINFO, &current_vinfo) == -1) {
        return -1;
    }

    width = current_vinfo.xres;
    height = current_vinfo.yres;
    src_bytes_per_pixel = current_vinfo.bits_per_pixel / 8U;
    row_size = (width * 3U + 3U) & ~3U;
    image_size = row_size * height;

    if (width == 0U || height == 0U ||
        (current_vinfo.bits_per_pixel != 16U &&
         current_vinfo.bits_per_pixel != 24U &&
         current_vinfo.bits_per_pixel != 32U)) {
        errno = EINVAL;
        return -1;
    }

    frame = (const uint8_t *)g_frame_buf[0] +
            (size_t)current_vinfo.yoffset * finfo.line_length +
            (size_t)current_vinfo.xoffset * src_bytes_per_pixel;
    row = calloc(1, row_size);
    if (row == NULL) {
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        goto done;
    }

    if (fwrite("BM", 1, 2, fp) != 2 ||
        bmp_write_u32(fp, header_size + image_size) != 0 ||
        bmp_write_u16(fp, 0) != 0 ||
        bmp_write_u16(fp, 0) != 0 ||
        bmp_write_u32(fp, header_size) != 0 ||
        bmp_write_u32(fp, 40) != 0 ||
        bmp_write_u32(fp, width) != 0 ||
        bmp_write_u32(fp, (uint32_t)(-(int32_t)height)) != 0 ||
        bmp_write_u16(fp, 1) != 0 ||
        bmp_write_u16(fp, 24) != 0 ||
        bmp_write_u32(fp, 0) != 0 ||
        bmp_write_u32(fp, image_size) != 0 ||
        bmp_write_u32(fp, 2835) != 0 ||
        bmp_write_u32(fp, 2835) != 0 ||
        bmp_write_u32(fp, 0) != 0 ||
        bmp_write_u32(fp, 0) != 0) {
        goto done;
    }

    for (y = 0; y < height; y++) {
        const uint8_t *src = frame + (size_t)y * finfo.line_length;
        uint32_t x;

        memset(row, 0, row_size);
        for (x = 0; x < width; x++) {
            uint32_t pixel = 0;
            const uint8_t *src_pixel = src + (size_t)x * src_bytes_per_pixel;
            uint8_t *dst_pixel = row + x * 3U;

            memcpy(&pixel, src_pixel, src_bytes_per_pixel);
            dst_pixel[0] = fbdev_component_to_u8(pixel, current_vinfo.blue);
            dst_pixel[1] = fbdev_component_to_u8(pixel, current_vinfo.green);
            dst_pixel[2] = fbdev_component_to_u8(pixel, current_vinfo.red);
        }

        if (fwrite(row, 1, row_size, fp) != row_size) {
            goto done;
        }
    }

    if (fflush(fp) != 0 || fsync(fileno(fp)) != 0) {
        goto done;
    }
    result = 0;

done:
    if (fp != NULL && fclose(fp) != 0) {
        result = -1;
    }
    if (result != 0) {
        unlink(path);
    }
    free(row);
    return result;
}

int draw_buf_size(int *width, int *height)
{
#ifdef USE_DRAW_BUF
    *width = DRAW_BUF_WIDTH;
    *height = DRAW_BUF_HEIGHT;
#else
    *width = vinfo.xres;
    *height = vinfo.yres;
#endif
    return 0;
}

enum mpp_pixel_format draw_buf_fmt(void)
{
    if (vinfo.bits_per_pixel == 32)
        return MPP_FMT_ARGB_8888;
    else if (vinfo.bits_per_pixel == 24)
        return MPP_FMT_RGB_888;
    else if (vinfo.bits_per_pixel == 16)
        return MPP_FMT_RGB_565;

    return MPP_FMT_ARGB_8888;
}

int draw_buf_bpp(void)
{
    return vinfo.bits_per_pixel;
}

int draw_buf_pitch(void)
{
#ifdef USE_DRAW_BUF
    if(disp_is_swap())
        return DRAW_BUF_SWAP_STRIDE;
    else
        return DRAW_BUF_STRIDE;
#else
    return finfo.line_length;
#endif
}

void fbdev_close(void)
{
    if (g_fb >= 0) {
        close(g_fb);
        g_fb = -1;
    }

#ifdef USE_DRAW_BUF
    int i;
    for (i = 0; i < g_buf_num; i++)
        draw_buf_free(i);
#endif
}

void ge_open(void)
{
    g_ge = mpp_ge_open();
    if (!g_ge) {
        LV_LOG_ERROR("ge_open fail");
    }
}

struct mpp_ge *get_ge(void)
{
    return g_ge;
}

void ge_close(void)
{
    if (g_ge) {
        mpp_ge_close(g_ge);
        g_ge = NULL;
    }
}
