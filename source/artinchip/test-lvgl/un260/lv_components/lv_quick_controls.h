#ifndef UN260_LV_QUICK_CONTROLS_H
#define UN260_LV_QUICK_CONTROLS_H
#include "lvgl/lvgl.h"
#include <stdint.h>
#define QUICK_CONTROL_COUNT 8
typedef enum {QUICK_LAYOUT,QUICK_GESTURES,QUICK_PURE,QUICK_POPUP,
    QUICK_TOUCH,QUICK_EXPORT,QUICK_QR,QUICK_STANDBY} quick_control_action_t;
typedef struct {
    bool layout,gestures,pure,popup,touch;
    const char *versions[3];
    uint8_t order[QUICK_CONTROL_COUNT];
} lv_quick_controls_state_t;
typedef struct {
    lv_obj_t *viewport,*track,*tiles[QUICK_CONTROL_COUNT];
    lv_obj_t *states[QUICK_CONTROL_COUNT],*versions[3];
    lv_quick_controls_state_t state;bool initialized;
} lv_quick_controls_t;
void lv_quick_controls_create(lv_quick_controls_t *,lv_obj_t *parent,int x,int y,lv_event_cb_t);
bool lv_quick_controls_refresh(lv_quick_controls_t *,lv_quick_controls_state_t);
lv_quick_controls_state_t lv_quick_controls_current_state(void);
const char *lv_quick_controls_title(quick_control_action_t action);
bool lv_quick_controls_execute(quick_control_action_t action);
#endif
