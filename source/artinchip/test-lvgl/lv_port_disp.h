/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//#define TRIPLE_FRAME_BUF_EN

void lv_port_disp_init(void);

void lv_port_disp_exit(void);
bool lv_port_disp_adopt_scanout(void);
/* UI-thread only; retry a failed submit without spinning in LVGL's flush wait. */
bool lv_port_disp_poll(void);
uint32_t fbdev_present_sequence(void);
/* UI-thread diagnostic observer. NULL removes it; no extra clock reads when
 * detached. This observes successful output submission, not optical light. */
typedef void (*lv_port_present_observer_t)(uint64_t present_us);
void lv_port_disp_set_present_observer(lv_port_present_observer_t observer);

int fbdev_draw_fps(void);

int disp_is_swap(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_PORT_DISP_H*/
