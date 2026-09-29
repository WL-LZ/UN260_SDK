#ifndef MACHINE_FAULT_VIEW_H
#define MACHINE_FAULT_VIEW_H
#include "lvgl/lvgl.h"
#include "fault_guide_catalog.h"

typedef struct {
    lv_obj_t *root, *base, *lid, *drawer_clip, *drawer, *face, *overlay;
    lv_timer_t *timer;
    mf_step_t step;
    uint32_t elapsed_ms, last_tick;
    uint16_t motion;
    uint8_t lid_frame;
    bool playing;
} machine_fault_view_t;

void machine_fault_view_create(machine_fault_view_t *view, lv_obj_t *parent, int x, int y);
void machine_fault_view_set_step(machine_fault_view_t *view, const mf_step_t *step);
void machine_fault_view_play(machine_fault_view_t *view, bool playing);
void machine_fault_view_restart(machine_fault_view_t *view);
void machine_fault_view_destroy(machine_fault_view_t *view);
#endif
