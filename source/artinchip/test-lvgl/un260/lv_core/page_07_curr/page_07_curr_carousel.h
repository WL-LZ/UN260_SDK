#ifndef PAGE_07_CURR_CAROUSEL_H
#define PAGE_07_CURR_CAROUSEL_H

#include "lvgl/lvgl.h"
#include "un260/lv_components/ui_scroll_physics.h"

/* One UI-thread owner for a non-scrollable viewport. The callback projects
 * motion.position into card positions; it never commits a currency choice. */
typedef struct {
    ui_scroll_physics_t motion;
    lv_obj_t *viewport;
    lv_timer_t *timer;
    void (*project)(void);
    lv_point_t press_point;
    float press_anchor;
    uint32_t last_interaction;
    bool enabled;
    bool active;
    bool dragging;
    bool suppress_click;
    bool profile_tracking;
    uint32_t profile_started;
    uint32_t profile_last_tick;
    uint32_t profile_max_gap;
    uint32_t profile_steps;
    uint32_t profile_frame_start;
    /* One contact, including direct dragging and its released motion. */
    uint32_t profile_project_calls;
    uint64_t profile_project_total_us;
    uint32_t profile_project_max_us;
    const char *profile_reason;
} page07_curr_carousel_t;

void page07_curr_carousel_init(page07_curr_carousel_t *c, lv_obj_t *viewport,
                               unsigned count, unsigned stride, unsigned index,
                               void (*project)(void));
/* Bind every clickable descendant, including the list and favorite buttons.
 * Events bubble to ONE viewport handler; the global raw observer stays owner
 * of side-navigation and multi-touch arbitration. */
void page07_curr_carousel_bind_child(lv_obj_t *obj);
void page07_curr_carousel_enable(page07_curr_carousel_t *c, bool enabled);
void page07_curr_carousel_snap(page07_curr_carousel_t *c, unsigned index,
                               bool animate);
void page07_curr_carousel_stop(page07_curr_carousel_t *c);
void page07_curr_carousel_destroy(page07_curr_carousel_t *c);
bool page07_curr_carousel_busy(const page07_curr_carousel_t *c);
bool page07_curr_carousel_click_allowed(const page07_curr_carousel_t *c);

#endif
