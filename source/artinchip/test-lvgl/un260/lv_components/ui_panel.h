#ifndef UN260_UI_PANEL_H
#define UN260_UI_PANEL_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UI_PANEL_WIDTH 1280
#define UI_PANEL_HEIGHT 400
#define UI_PANEL_WING_WIDTH 244
#define UI_PANEL_CONTENT_WIDTH 792
#define UI_PANEL_MAX_KEYS 7
#define UI_PANEL_NO_ACTION 0

typedef enum { UI_PANEL_LAYOUT_STANDARD, UI_PANEL_LAYOUT_ENTRY } ui_panel_layout_t;
typedef enum { UI_PANEL_EVENT_CLICK, UI_PANEL_EVENT_LONG_PRESS } ui_panel_event_t;
typedef enum { UI_PANEL_KEY_TAP, UI_PANEL_KEY_TURN, UI_PANEL_KEY_FEED,
    UI_PANEL_KEY_SCAN, UI_PANEL_KEY_SWAP } ui_panel_key_motion_t;
typedef enum {
    UI_PANEL_SCENE_TRANSPORT, UI_PANEL_SCENE_DIAL, UI_PANEL_SCENE_STACK,
    UI_PANEL_SCENE_LOCK, UI_PANEL_SCENE_SCAN, UI_PANEL_SCENE_PRINT,
    UI_PANEL_SCENE_SUN, UI_PANEL_SCENE_CLOCK, UI_PANEL_SCENE_SOUND,
    UI_PANEL_SCENE_GRID, UI_PANEL_SCENE_SLIDERS, UI_PANEL_SCENE_SORT,
    UI_PANEL_SCENE_CLEAR, UI_PANEL_SCENE_CHECK, UI_PANEL_SCENE_JAM,
    UI_PANEL_SCENE_COUNT
} ui_panel_scene_t;
typedef enum { UI_PANEL_TONE_NEUTRAL, UI_PANEL_TONE_SUCCESS,
    UI_PANEL_TONE_WARNING, UI_PANEL_TONE_ERROR } ui_panel_tone_t;

typedef struct {
    uint16_t action; /* Caller-owned token. Zero means no action. */
    const char *icon; /* Lucide name from the approved panel icon subset; NULL for text. */
    const char *label;
    const char *sub;
    ui_panel_key_motion_t motion;
    uint32_t value; /* Semantic value, unaffected by translated wording. */
    int16_t angle_tenths; /* Gauge hand, 0.1 degree units. */
    bool lit, disabled, primary, danger, numeric, long_press;
    uint8_t digit; /* Used only when numeric=true. */
} ui_panel_key_t;

typedef struct {
    ui_panel_scene_t scene;
    const char *title;
    const char *detail;
    uint16_t value; /* 0..1000. */
    uint8_t pin_digits; /* 0..4; only the lock scene uses this field. */
    ui_panel_tone_t tone;
    bool running;
    uint32_t revision; /* Change only for a new user action/state transition. */
} ui_panel_motion_t;

typedef void (*ui_panel_action_cb_t)(void *user, uint16_t action, ui_panel_event_t event);
/* Zero-initialize; context must outlive its root. Parent deletion clears these
 * fields and releases owned animation resources. destroy is then a no-op. */
typedef struct { lv_obj_t *root, *content; void *private_state; } ui_panel_t;

/* false covers invalid input or failure of the component's initial state
 * allocation; those return paths do not mutate the context or parent.
 * Current LVGL object/style/event/timer allocation failures are process-fatal
 * under lv_conf.h's abort policy before they can return to this component.
 * This API does not promise recovery from arbitrary partial LVGL allocation. */
bool ui_panel_create(ui_panel_t *panel, lv_obj_t *parent,
                     ui_panel_action_cb_t callback, void *user);
lv_obj_t *ui_panel_content(const ui_panel_t *panel);
/* Standard left=6/right=5, entry left=7/right=7. The final right key is wide;
 * entry's final left key is also wide. NULL strings are empty. Text is copied
 * by LVGL; descriptors/arrays need not outlive this call. No entry animation. */
void ui_panel_set_keys(ui_panel_t *panel, ui_panel_layout_t layout,
                       const ui_panel_key_t *left, size_t left_count,
                       const ui_panel_key_t *right, size_t right_count,
                       uint32_t revision, uint16_t trigger_action);
/* Compatibility status entry: title/detail/tone update the fixed 203 x 72 slot.
 * Scene value/revision/running metadata no longer creates or drives graphics. */
void ui_panel_set_motion(ui_panel_t *panel, const ui_panel_motion_t *motion);
/* Enables only key icon/lamp/numeric micro-motion. Held-key brightness feedback
 * remains immediate and active when animations are disabled. */
void ui_panel_set_animation_enabled(ui_panel_t *panel, bool enabled);
void ui_panel_set_visible(ui_panel_t *panel, bool visible);
void ui_panel_destroy(ui_panel_t *panel);
/* Stable fonts, no lookup-time I/O/allocation. Locale fonts must have been
 * prepared by the existing application locale service. */
const lv_font_t *ui_panel_font(unsigned px, bool semibold);
/* Native-size transparent icon, caller owns its parent/lifetime. */
lv_obj_t *ui_panel_icon_create(lv_obj_t *parent, const char *name,
                               unsigned px, lv_color_t color);
#endif
