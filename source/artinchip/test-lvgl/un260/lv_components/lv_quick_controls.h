#ifndef UN260_LV_QUICK_CONTROLS_H
#define UN260_LV_QUICK_CONTROLS_H
#include "lvgl/lvgl.h"
typedef enum {QUICK_LAYOUT=0,QUICK_GESTURES=1,QUICK_STANDBY=2,QUICK_CLOSE=3,QUICK_APPEARANCE=4} quick_control_action_t;
typedef struct {bool layout,gestures;const char *versions[3];} lv_quick_controls_state_t;
typedef struct {lv_obj_t *switches[2],*labels[2],*versions[3];lv_quick_controls_state_t state;bool initialized;} lv_quick_controls_t;
/* Presentation only: caller owns state, actions and lifetime. No brightness control. */
void lv_quick_controls_create(lv_quick_controls_t *,lv_obj_t *parent,int x,int y,lv_event_cb_t);
bool lv_quick_controls_refresh(lv_quick_controls_t *,lv_quick_controls_state_t);
#endif
