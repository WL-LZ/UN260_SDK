/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef LV_GE2D_H
#define LV_GE2D_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/src/misc/lv_color.h"
#include "lvgl/src/hal/lv_hal_disp.h"
#include "lvgl/src/draw/sw/lv_draw_sw.h"
#include <video/mpp_types.h>

typedef lv_draw_sw_ctx_t lv_draw_aic_ctx_t;

struct _lv_disp_drv_t;

void lv_draw_aic_ctx_init(struct _lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx);

void lv_draw_aic_ctx_deinit(struct _lv_disp_drv_t * drv, lv_draw_ctx_t * draw_ctx);

/* Drop DMA scale variants before the decoder releases the source frame. */
void lv_ge2d_scaled_cache_drop_source(const void *source_frame);

/* Register an application-owned DMA image behind a stable LVGL variable
 * image data key. The owner keeps the frame alive until unregistering it. */
bool lv_ge2d_register_dma_image(const void *data_key,
                                const struct mpp_frame *frame,
                                const char *profile_name);
void lv_ge2d_unregister_dma_image(const void *data_key);

/* Snapshot rendering uses an off-screen software buffer. These guards let
 * the driver report an unsupported decoder/format instead of publishing a
 * partially rendered cache image. */
void lv_ge2d_offscreen_capture_begin(void);
bool lv_ge2d_offscreen_capture_end(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_GE2D_H*/
