/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <video/artinchip_fb.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/core/lv_refr.h"
#include "lv_port_disp.h"
#include "lv_ge2d.h"
#include "mpp_ge.h"
#include "lv_fbdev.h"
#include "aic_ui/perf_stats.h"
#include "aic_ui/present_damage.h"
#include "un260/lv_system/app_clock.h"

static int draw_fps = 0;
static struct mpp_ge *g_ge = NULL;
static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;
static int g_fb = -1;
char *buf_next = NULL;
static int g_triple_fb = 0;
static enum ge_mode g_ge_mode = GE_MODE_NORMAL;
static struct fb_var_screeninfo g_pan_var;
static int g_pan_var_valid = 0;
static int g_live_vscreeninfo = 0;
static uint32_t g_present_sequence;
static lv_port_present_observer_t g_present_observer;

void lv_port_disp_set_present_observer(lv_port_present_observer_t observer)
{
    g_present_observer = observer;
}

static void notify_present(void)
{
    ++g_present_sequence;
    if (g_present_observer) g_present_observer(app_clock_monotonic_us());
}
static bool g_damage_reuse;
static lv_color_t *g_previous_buffer;
static present_rect_t g_previous_damage[LV_INV_BUF_SIZE];
static size_t g_previous_count;
static uint32_t g_prepare_us;
static uint64_t g_prepare_pixels, g_saved_pixels;
static lv_disp_drv_t *g_retry_driver;
static lv_color_t *g_retry_buffer;
static uint32_t g_retry_tick;
static uint32_t g_present_errors, g_copy_fallbacks;
static perf_profile_flush_sample_t g_retry_sample;
static bool g_retry_profile;
static uint64_t g_retry_started_us;

#ifdef USE_DRAW_BUF
static int g_fb_num = 1;
static int g_fb_id = 0;
#endif

#define US_PER_SEC      1000000
static double get_time_gap(struct timeval *start, struct timeval *end)
{
    double diff;

    if (end->tv_usec < start->tv_usec) {
        diff = (double)(US_PER_SEC + end->tv_usec - start->tv_usec)/US_PER_SEC;
        diff += end->tv_sec - 1 - start->tv_sec;
    } else {
        diff = (double)(end->tv_usec - start->tv_usec)/US_PER_SEC;
        diff += end->tv_sec - start->tv_sec;
    }

    return diff;
}

static int cal_fps(double gap, int cnt)
{
    return (int)(cnt / gap);
}

static void cal_frame_rate()
{
    static int start_cal = 0;
    static int frame_cnt = 0;
    static struct timeval start, end;
    double interval = 0.5;
    double gap = 0;

    if (start_cal == 0) {
        start_cal = 1;
        gettimeofday(&start, NULL);
    }

    gettimeofday(&end, NULL);
    gap = get_time_gap(&start, &end);
    if (gap >= interval) {
        draw_fps = cal_fps(gap, frame_cnt);
        frame_cnt = 0;
        start_cal = 0;
    } else {
        frame_cnt++;
    }
    return;
}

static bool sync_disp_buf(lv_disp_drv_t * drv, lv_color_t * color_p, const lv_area_t * area_p)
{
    int32_t ret;
    struct ge_bitblt blt = { 0 };
    lv_coord_t w = lv_area_get_width(area_p);
    lv_coord_t h = lv_area_get_height(area_p);
    enum mpp_pixel_format fmt = draw_buf_fmt();
    int pitch = draw_buf_pitch();
    int buf_w = disp_drv.hor_res;
    int buf_h = disp_drv.ver_res;

#ifdef USE_DRAW_BUF
    blt.src_buf.buf_type = MPP_DMA_BUF_FD;
    if (color_p == (lv_color_t *)g_draw_buf[0])
        blt.src_buf.fd[0] = g_draw_buf_fd[0];
    else
        blt.src_buf.fd[0] = g_draw_buf_fd[1];
#else
    blt.src_buf.buf_type = MPP_PHY_ADDR;
    if (color_p == (lv_color_t *)g_frame_buf[0])
        blt.src_buf.phy_addr[0] = g_frame_phy[0];
    else
        blt.src_buf.phy_addr[0] = g_frame_phy[1];
#endif

    blt.src_buf.stride[0] = pitch;
    blt.src_buf.size.width = buf_w;
    blt.src_buf.size.height = buf_h;
    blt.src_buf.crop_en = 1;
    blt.src_buf.crop.x = area_p->x1;
    blt.src_buf.crop.y = area_p->y1;
    blt.src_buf.crop.width = w;
    blt.src_buf.crop.height = h;
    blt.src_buf.format = fmt;

#ifdef USE_DRAW_BUF
    blt.dst_buf.buf_type = MPP_DMA_BUF_FD;
    if (color_p == (lv_color_t *)g_draw_buf[0])
        blt.dst_buf.fd[0] = g_draw_buf_fd[1];
    else
        blt.dst_buf.fd[0] = g_draw_buf_fd[0];
#else
    blt.dst_buf.buf_type = MPP_PHY_ADDR;
    if (color_p == (lv_color_t *)g_frame_buf[0])
        blt.dst_buf.phy_addr[0] = g_frame_phy[1];
    else
        blt.dst_buf.phy_addr[0] = g_frame_phy[0];
#endif

    blt.dst_buf.crop_en = 1;
    blt.dst_buf.crop.x = area_p->x1;
    blt.dst_buf.crop.y = area_p->y1;
    blt.dst_buf.crop.width = w;
    blt.dst_buf.crop.height = h;
    blt.dst_buf.stride[0] = pitch;
    blt.dst_buf.size.width = buf_w;
    blt.dst_buf.size.height = buf_h;
    blt.dst_buf.format = fmt;

    ret = mpp_ge_bitblt(g_ge, &blt);
    if (ret < 0) {
        LV_LOG_ERROR("bitblt fail");
        return false;
    }

    /*
     * GE normal mode executes and waits inside IOC_GE_BITBLT.  Calling
     * emit/sync afterwards only takes two extra mutex round-trips because
     * both operations are no-ops in normal_ops.c.  Keep them for CMDQ mode,
     * where they are required to submit and complete the queued command.
     */
    if (g_ge_mode == GE_MODE_CMDQ) {
        ret = mpp_ge_emit(g_ge);
        if (ret < 0) {
            LV_LOG_ERROR("emit fail");
            return false;
        }

        ret = mpp_ge_sync(g_ge);
        if (ret < 0) {
            LV_LOG_ERROR("sync fail");
            return false;
        }
    }

    return true;
}

#ifdef USE_DRAW_BUF
void disp_draw_buf(lv_disp_drv_t * drv, lv_color_t * color_p)
{
    int32_t ret;
    int disp_w;
    int disp_h;
    struct ge_bitblt blt = { 0 };
    enum mpp_pixel_format draw_fmt = draw_buf_fmt();
    int draw_pitch = draw_buf_pitch();
    enum mpp_pixel_format disp_fmt = fbdev_get_fmt();
    int disp_pitch = fbdev_get_pitch();
    int draw_w;
    int draw_h;

    if (drv->rotated == LV_DISP_ROT_90 || drv->rotated == LV_DISP_ROT_270) {
        draw_w = drv->ver_res;
        draw_h = drv->hor_res;
    } else  {
        draw_w = drv->hor_res;
        draw_h = drv->ver_res;
    }

    fbdev_get_size(&disp_w, &disp_h);

    blt.src_buf.buf_type = MPP_DMA_BUF_FD;
    if (color_p == (lv_color_t *)g_draw_buf[0])
        blt.src_buf.fd[0] = g_draw_buf_fd[0];
    else
        blt.src_buf.fd[0] = g_draw_buf_fd[1];

    blt.src_buf.stride[0] = draw_pitch;
    blt.src_buf.size.width = draw_w;
    blt.src_buf.size.height = draw_h;
    blt.src_buf.format =  draw_fmt;

    blt.dst_buf.buf_type = MPP_PHY_ADDR;
    blt.dst_buf.phy_addr[0] = g_frame_phy[g_fb_id];
    blt.dst_buf.stride[0] = disp_pitch;
    blt.dst_buf.size.width = disp_w;
    blt.dst_buf.size.height = disp_h;
    blt.dst_buf.format = disp_fmt;

    /* rotation */
    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        blt.ctrl.flags = MPP_ROTATION_0;
        break;
    case LV_DISP_ROT_90:
        blt.ctrl.flags = MPP_ROTATION_90;
        break;
    case LV_DISP_ROT_180:
        blt.ctrl.flags = MPP_ROTATION_180;
        break;
    case LV_DISP_ROT_270:
        blt.ctrl.flags = MPP_ROTATION_270;
        break;
    default:
        break;
    };

    ret = mpp_ge_bitblt(g_ge, &blt);
    if (ret < 0) {
        LV_LOG_ERROR("bitblt fail");
        return;
    }

    ret = mpp_ge_emit(g_ge);
    if (ret < 0) {
        LV_LOG_ERROR("emit fail");
        return;
    }

    ret = mpp_ge_sync(g_ge);
    if (ret < 0) {
        LV_LOG_ERROR("sync fail");
        return;
    }

    return;
}
#endif

static size_t collect_damage(lv_disp_t *disp, present_rect_t *rects)
{
    size_t count = 0;
    for (unsigned i = 0; i < disp->inv_p; ++i) {
        if (disp->inv_area_joined[i]) continue;
        const lv_area_t *a = &disp->inv_areas[i];
        rects[count++] = (present_rect_t){a->x1, a->y1, a->x2, a->y2};
    }
    return count;
}

static void force_full_damage(lv_disp_t *disp)
{
    /* Called before LVGL chooses clip rectangles. A copy error
     * therefore falls back to a complete redraw, never a partly stale buffer. */
    /* LVGL has already calculated last_i before render_start_cb. Preserve
     * that index, otherwise no final flush/buffer swap would be issued. */
    unsigned last = disp->inv_p - 1;
    while (last && disp->inv_area_joined[last]) --last;
    memset(disp->inv_area_joined, 1, sizeof(disp->inv_area_joined));
    disp->inv_area_joined[last] = 0;
    disp->inv_areas[last] = (lv_area_t){0, 0, disp->driver->hor_res-1,
                                        disp->driver->ver_res-1};
}

static void fbdev_render_start(lv_disp_drv_t *drv)
{
    lv_disp_t *disp = _lv_refr_get_disp_refreshing();
    present_rect_t redraw[LV_INV_BUF_SIZE], copy[PRESENT_DAMAGE_CAPACITY];
    size_t count = 0, redraw_count;
    const present_rect_t *copy_regions = copy;
    uint64_t started;
    if (!g_damage_reuse) return;
    if (g_retry_driver) {
        /* Also covers a second refresh in the SAME lv_timer_handler and
         * explicit lv_refr_now(): do not enter LVGL's flushing wait loop.
         * All suppressed invalidations are restored by a full redraw after
         * recovery, so no model update can disappear permanently. */
        memset(disp->inv_areas, 0, sizeof(disp->inv_areas));
        memset(disp->inv_area_joined, 0, sizeof(disp->inv_area_joined));
        disp->inv_p = 0;
        lv_timer_pause(disp->refr_timer);
        return;
    }
    if (!g_previous_count) return;
    started = app_clock_monotonic_us();
    redraw_count = collect_damage(disp, redraw);
    uint64_t previous_pixels = present_damage_pixels(g_previous_damage, g_previous_count);
    bool planned = present_damage_plan(g_previous_damage, g_previous_count,
                                       redraw, redraw_count, copy,
                                       PRESENT_DAMAGE_CAPACITY, &count);
    /* A fragmented difference can cost more GE submissions than the saved
     * pixels justify. Copy the original damage if planning becomes complex. */
    if (!planned || count > g_previous_count + 4U) {
        count = g_previous_count;
        copy_regions = g_previous_damage;
    }
    uint64_t copied = 0;
    bool copy_failed = false;
    for (size_t i = 0; i < count; ++i) {
        lv_area_t area = {copy_regions[i].x1, copy_regions[i].y1,
                          copy_regions[i].x2, copy_regions[i].y2};
        if (!sync_disp_buf(drv, g_previous_buffer, &area)) {
            ++g_copy_fallbacks;
            copy_failed = true;
            force_full_damage(disp);
            break;
        }
        copied += present_damage_pixels(&copy_regions[i], 1);
    }
    g_previous_count = 0;
    g_prepare_us = app_clock_elapsed_us32(started, app_clock_monotonic_us());
    g_prepare_pixels = copied;
    g_saved_pixels = !copy_failed && previous_pixels >= copied ? previous_pixels - copied : 0;
}

/* Same-buffer pan/wait retries cannot change ownership until VSYNC succeeds.
 * On failure leave flush pending, return to the application service loop and
 * retry there. Never let LVGL draw into an unconfirmed scanout buffer. */
static bool present_submit(lv_disp_drv_t *drv, lv_color_t *buffer,
                           perf_profile_flush_sample_t *sample)
{
    struct fb_var_screeninfo var;
    uint64_t started;
    int result, zero = 0;
    if (!g_live_vscreeninfo && g_pan_var_valid) var = g_pan_var;
    else if (ioctl(g_fb, FBIOGET_VSCREENINFO, &var) < 0) return false;
    var.xoffset = 0;
    var.yoffset = buffer == (lv_color_t *)g_frame_buf[0] ? 0 : drv->ver_res;
    started = app_clock_monotonic_us();
    do { result = ioctl(g_fb, FBIOPAN_DISPLAY, &var); } while (result < 0 && errno == EINTR);
    sample->pan_us += app_clock_elapsed_us32(started, app_clock_monotonic_us());
    if (result < 0) return false;
    started = app_clock_monotonic_us();
    do { result = ioctl(g_fb, AICFB_WAIT_FOR_VSYNC, &zero); } while (result < 0 && errno == EINTR);
    sample->vsync_us += app_clock_elapsed_us32(started, app_clock_monotonic_us());
    return result >= 0;
}

static void present_complete(lv_disp_drv_t *drv, lv_color_t *buffer,
                             perf_profile_flush_sample_t *sample,
                             bool profile, uint64_t started)
{
    g_previous_buffer = buffer;
    notify_present();
    sample->mirror_us = g_prepare_us;
    sample->mirror_pixels = g_prepare_pixels;
    /* Deferred mirror is still charged to output time. Moving it out of
     * flush_cb must not manufacture an apparent performance improvement. */
    sample->total_us = app_clock_elapsed_us32(started, app_clock_monotonic_us()) + g_prepare_us;
    if (profile) {
        perf_profile_report_flush(sample);
        perf_profile_report_present_reuse(g_prepare_pixels, g_saved_pixels,
                                           g_copy_fallbacks, g_present_errors);
    }
    g_prepare_us = 0;
    g_prepare_pixels = g_saved_pixels = 0;
    g_copy_fallbacks = g_present_errors = 0;
    cal_frame_rate();
    lv_disp_flush_ready(drv);
}

bool lv_port_disp_poll(void)
{
    if (!g_retry_driver) return true;
    uint32_t now = app_clock_uptime_ms();
    if ((uint32_t)(now - g_retry_tick) < 50U) return false;
    g_retry_tick = now;
    if (!present_submit(g_retry_driver, g_retry_buffer, &g_retry_sample)) return false;
    present_complete(g_retry_driver, g_retry_buffer, &g_retry_sample,
                     g_retry_profile, g_retry_started_us);
    g_retry_driver = NULL;
    lv_obj_invalidate(lv_scr_act());
    fprintf(stderr, "DISP_RECOVER submit=ok buffers=released\n");
    return true;
}

uint32_t fbdev_present_sequence(void) { return g_present_sequence; }

static void fbdev_flush(lv_disp_drv_t * drv, const lv_area_t * area,
                        lv_color_t *color_p)
{
    lv_disp_t * disp = _lv_refr_get_disp_refreshing();
    lv_disp_draw_buf_t * draw_buf = lv_disp_get_draw_buf(disp);
    bool profile_enabled = perf_profile_is_enabled();
    perf_profile_flush_sample_t profile_sample;
    uint64_t profile_started_us = 0;

    LV_UNUSED(area);
    if (profile_enabled) {
        profile_sample = (perf_profile_flush_sample_t){0};
        profile_started_us = app_clock_monotonic_us();
    }

    if (g_damage_reuse) {
        if (!draw_buf->flushing_last) { lv_disp_flush_ready(drv); return; }
        profile_sample = (perf_profile_flush_sample_t){0};
        profile_started_us = app_clock_monotonic_us();
        g_previous_count = collect_damage(disp, g_previous_damage);
        profile_sample.invalid_area_count = g_previous_count;
        profile_sample.invalid_pixels = present_damage_pixels(g_previous_damage, g_previous_count);
        profile_sample.full_screen = profile_sample.invalid_pixels >= (uint64_t)drv->hor_res * drv->ver_res;
        if (!present_submit(drv, color_p, &profile_sample)) {
            ++g_present_errors;
            g_retry_driver = drv;
            g_retry_buffer = color_p;
            g_retry_tick = app_clock_uptime_ms();
            g_retry_sample = profile_sample;
            g_retry_profile = profile_enabled;
            g_retry_started_us = profile_started_us;
            fprintf(stderr, "DISP_RECOVER submit=retry errno=%d buffers=held\n", errno);
            return;
        }
        present_complete(drv, color_p, &profile_sample, profile_enabled, profile_started_us);
        return;
    }

    if (!disp->driver->direct_mode || draw_buf->flushing_last) {
        struct fb_var_screeninfo var = {0};
        int pan_result;
        uint64_t stage_started_us = 0;

        if (!g_live_vscreeninfo && g_pan_var_valid) {
            var = g_pan_var;
        } else {
            if (ioctl(g_fb, FBIOGET_VSCREENINFO, &var) < 0) {
                LV_LOG_WARN("ioctl FBIOGET_VSCREENINFO");
                lv_disp_flush_ready(drv);
                return;
            }
        }

#ifdef USE_DRAW_BUF
        int disp_w;
        int disp_h;

        fbdev_get_size(&disp_w, &disp_h);
        var.xoffset = 0;
        var.yoffset = g_fb_id * disp_h;
        disp_draw_buf(drv, color_p);
        g_fb_id++;
        if (g_fb_id >= g_fb_num)
            g_fb_id = 0;
#else
        if (buf_next) {
            if (draw_buf->buf1 == color_p)
                draw_buf->buf1 = (lv_color_t *)buf_next;
            else
                draw_buf->buf2 = (lv_color_t *)buf_next;

            draw_buf->buf_act = (lv_color_t *)buf_next;
            buf_next = (char *)color_p;
        }

        if (color_p == (lv_color_t *)g_frame_buf[0]) {
            var.xoffset = 0;
            var.yoffset = 0;
        } else if (color_p == (lv_color_t *)g_frame_buf[2]) {
            var.xoffset = 0;
            var.yoffset = disp_drv.ver_res * 2;
        } else {
            var.xoffset = 0;
            var.yoffset = disp_drv.ver_res;
        }
#endif

        if (profile_enabled) {
            stage_started_us = app_clock_monotonic_us();
        }
        pan_result = ioctl(g_fb, FBIOPAN_DISPLAY, &var);
        if (profile_enabled) {
            profile_sample.pan_us = app_clock_elapsed_us32(
                stage_started_us, app_clock_monotonic_us());
        }

        if (pan_result == 0) {
            if (!g_triple_fb) {
                int zero = 0;

                if (profile_enabled) {
                    stage_started_us = app_clock_monotonic_us();
                }
                if (ioctl(g_fb, AICFB_WAIT_FOR_VSYNC, &zero) < 0) {
                    LV_LOG_WARN("ioctl AICFB_WAIT_FOR_VSYNC fail");
                    return;
                }
                if (profile_enabled) {
                    profile_sample.vsync_us = app_clock_elapsed_us32(
                        stage_started_us, app_clock_monotonic_us());
                }
            }
        } else {
            LV_LOG_WARN("pan display err");
        }

        if (profile_enabled) {
            for (int i = 0; i < disp->inv_p; i++) {
                if (disp->inv_area_joined[i] == 0) {
                    uint64_t pixels =
                        (uint64_t)lv_area_get_width(&disp->inv_areas[i]) *
                        (uint64_t)lv_area_get_height(&disp->inv_areas[i]);

                    profile_sample.invalid_area_count++;
                    profile_sample.invalid_pixels += pixels;
                }
            }
            profile_sample.full_screen =
                profile_sample.invalid_pixels >=
                (uint64_t)drv->hor_res * (uint64_t)drv->ver_res;
        }

        if (drv->direct_mode == 1) {
            if (profile_enabled) {
                stage_started_us = app_clock_monotonic_us();
            }
            for (int i = 0; i < disp->inv_p; i++) {
                if (disp->inv_area_joined[i] == 0) {
                    sync_disp_buf(drv, color_p, &disp->inv_areas[i]);
                }
            }
            if (profile_enabled) {
                profile_sample.mirror_us = app_clock_elapsed_us32(
                    stage_started_us, app_clock_monotonic_us());
                profile_sample.mirror_pixels = profile_sample.invalid_pixels;
            }
        }

        if (profile_enabled) {
            profile_sample.total_us = app_clock_elapsed_us32(
                profile_started_us, app_clock_monotonic_us());
            perf_profile_report_flush(&profile_sample);
        }

        cal_frame_rate();
        if (pan_result == 0) notify_present();
        lv_disp_flush_ready(drv);
    }
    else {
        lv_disp_flush_ready(drv);
    }
}


void lv_port_disp_init(void)
{
    int width, height;
    void *buf1 = NULL;
    void *buf2 = NULL;

    g_fb = fbdev_open();
    if (g_fb < 0) {
        LV_LOG_ERROR("fbdev_open fail");
        return;
    }

    draw_buf_size(&width, &height);
    ge_open();
    g_ge = get_ge();
    if (!g_ge) {
        LV_LOG_ERROR("ge open fail");
        return;
    }
    g_ge_mode = mpp_ge_get_mode(g_ge);

    /*
     * Resolution, pixel format and virtual framebuffer layout are fixed for
     * the lifetime of this application.  Cache the pan template instead of
     * issuing FBIOGET_VSCREENINFO for every rendered frame.  The old live
     * query path remains available for A/B testing and emergency fallback.
     */
    g_live_vscreeninfo = getenv("UN260_FB_LIVE_VINFO") != NULL;
    if (ioctl(g_fb, FBIOGET_VSCREENINFO, &g_pan_var) == 0) {
        g_pan_var_valid = 1;
    } else {
        g_pan_var_valid = 0;
        g_live_vscreeninfo = 1;
        LV_LOG_WARN("initial FBIOGET_VSCREENINFO failed; use live query");
    }
    printf("DISP_PIPE ge=%s vscreeninfo=%s direct=1\n",
           g_ge_mode == GE_MODE_CMDQ ? "CMDQ" : "NORMAL",
           g_live_vscreeninfo ? "LIVE" : "CACHED");

#ifdef USE_DRAW_BUF
    buf1 = (void *)g_draw_buf[0];
    if (g_frame_buf[1]) {
        g_fb_id = 1;
        g_fb_num = 2;
    }
#ifdef TRIPLE_FRAME_BUF_EN
    if (g_frame_buf[2]) {
        g_fb_id = 1;
        g_fb_num = 3;
    }
#endif // TRIPLE_FRAME_BUF_EN
#else
    buf1 = (void *)g_frame_buf[0];
    buf2 = (void *)g_frame_buf[1];
#endif // USE_DRAW_BUF

    if (!buf2) // single frame buffer
        lv_disp_draw_buf_init(&disp_buf, buf1, 0, width * height);
    else      // double frame buffer
        lv_disp_draw_buf_init(&disp_buf, buf2, buf1, width * height);

    lv_disp_drv_init(&disp_drv);

    /*Set a display buffer*/
    disp_drv.draw_buf = &disp_buf;

    /*Set the resolution of the display*/
    disp_drv.hor_res = width;
    disp_drv.ver_res = height;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 1;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.draw_ctx_init = lv_draw_aic_ctx_init;
    disp_drv.draw_ctx_deinit = lv_draw_aic_ctx_deinit;
    disp_drv.draw_ctx_size = sizeof(lv_draw_aic_ctx_t);

    /* when define USE_DRAW_BUF, disp_drv.rotated can be
      LV_DISP_ROT_90/LV_DISP_ROT_180/LV_DISP_ROT_270
    */
    //disp_drv.rotated = LV_DISP_ROT_90;
#ifdef TRIPLE_FRAME_BUF_EN
    if (g_frame_buf[2]) {
        disp_drv.full_refresh = 1;
        disp_drv.direct_mode = 0;
        buf_next = g_frame_buf[2];
        g_triple_fb = 1;
    }
#endif

#ifdef USE_DRAW_BUF
    disp_drv.full_refresh = 1;
    disp_drv.direct_mode = 0;
#endif

    /*Finally register the driver*/
    /* Only the sealed opaque, synchronous GE-normal dual framebuffer path
     * has the ownership contract needed for deferred mirror subtraction. */
#if !defined(USE_DRAW_BUF) && LV_COLOR_DEPTH == 32 && LV_COLOR_SCREEN_TRANSP == 0
    g_damage_reuse = buf2 && !g_triple_fb && disp_drv.direct_mode &&
                     disp_drv.rotated == LV_DISP_ROT_NONE && !disp_drv.sw_rotate &&
                     disp_drv.offset_x == 0 && disp_drv.offset_y == 0 &&
                     g_ge_mode == GE_MODE_NORMAL &&
                     getenv("UN260_PRESENT_LEGACY") == NULL;
#endif
    if (g_damage_reuse) disp_drv.render_start_cb = fbdev_render_start;
    printf("DISP_REUSE enabled=%d policy=previous-minus-redraw capacity=%u\n",
           g_damage_reuse, PRESENT_DAMAGE_CAPACITY);
    lv_disp_drv_register(&disp_drv);
}

bool lv_port_disp_adopt_scanout(void)
{
#if !defined(USE_DRAW_BUF) && !defined(TRIPLE_FRAME_BUF_EN)
    struct fb_var_screeninfo current;
    if(g_fb<0||!g_frame_buf[1]||g_present_sequence!=0||
       ioctl(g_fb,FBIOGET_VSCREENINFO,&current)<0||current.xoffset||
       (current.yoffset!=0&&current.yoffset!=current.yres))return false;
    unsigned visible=current.yoffset?1U:0U;
    lv_disp_draw_buf_init(&disp_buf,g_frame_buf[visible^1U],g_frame_buf[visible],
                         current.xres*current.yres);
    g_pan_var=current;g_pan_var_valid=1;
    return true;
#else
    return false;
#endif
}

void lv_port_disp_exit(void)
{
    g_present_observer = NULL;
    if (g_ge) {
        ge_close();
        g_ge = NULL;
    }
    fbdev_close();
}

int fbdev_draw_fps(void)
{
    return (int)draw_fps;
}

int disp_is_swap(void)
{
    if (disp_drv.rotated == LV_DISP_ROT_90 || disp_drv.rotated == LV_DISP_ROT_270)
        return 1;
    else
        return 0;
}
