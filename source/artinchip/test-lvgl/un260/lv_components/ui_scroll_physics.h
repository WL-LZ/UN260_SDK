#ifndef UN260_UI_SCROLL_PHYSICS_H
#define UN260_UI_SCROLL_PHYSICS_H

#include <stdbool.h>
#include <stdint.h>

/* UI-thread-owned one-dimensional motion. No LVGL, heap, I/O or global state.
 * Coordinates are content offsets (increasing offset moves content left).
 * Visual focus is not a controller/business selection. */
typedef enum {
    UI_SCROLL_IDLE = 0,
    UI_SCROLL_DRAG,
    UI_SCROLL_COAST,
    UI_SCROLL_SPRING
} ui_scroll_phase_t;

#define UI_SCROLL_SAMPLE_COUNT 12
typedef struct {
    float position;
    float velocity;
    float target;
    float stride;
    float max_position;
    unsigned count;
    ui_scroll_phase_t phase;
    uint32_t tick;
    uint32_t release_tick;
    uint32_t motion_tick;
    unsigned sample_count;
    struct { float x; uint32_t tick; } samples[UI_SCROLL_SAMPLE_COUNT];
} ui_scroll_physics_t;

void ui_scroll_physics_init(ui_scroll_physics_t *s, unsigned count,
                            float stride, unsigned index);
/* Returns raw drag anchor. Continue with anchor - (pointer_x - press_x).
 * Interrupting a coast/spring never teleports the visible position. */
float ui_scroll_physics_begin(ui_scroll_physics_t *s, uint32_t now_ms);
void ui_scroll_physics_drag(ui_scroll_physics_t *s, float raw_position,
                            uint32_t now_ms);
void ui_scroll_physics_release(ui_scroll_physics_t *s, uint32_t now_ms,
                               bool cancelled);
/* Advance using elapsed time, not frames; returns whether motion remains. */
bool ui_scroll_physics_step(ui_scroll_physics_t *s, uint32_t now_ms);
void ui_scroll_physics_snap(ui_scroll_physics_t *s, unsigned index,
                            bool animate, uint32_t now_ms);
/* Owner hide/destroy/filter change: stop and settle to nearest valid slot. */
void ui_scroll_physics_stop(ui_scroll_physics_t *s);
unsigned ui_scroll_physics_nearest(const ui_scroll_physics_t *s);
/* Smooth proximity in [0,1024], for small position/scale-only projections. */
unsigned ui_scroll_physics_focus(const ui_scroll_physics_t *s, unsigned index);

#endif
