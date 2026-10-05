#ifndef UN260_LV_QUICK_CONTROLS_H
#define UN260_LV_QUICK_CONTROLS_H
#include "lvgl/lvgl.h"
#include <stdint.h>
#include "un260/lv_system/ui_state_store.h"
#define QUICK_CONTROL_COUNT UI_STATE_QUICK_CONTROL_COUNT
#define QUICK_CONTROLS_PER_PAGE 8U
typedef enum {QUICK_LAYOUT,QUICK_GESTURES,QUICK_PURE,QUICK_POPUP,
    QUICK_TOUCH,QUICK_EXPORT,QUICK_QR,QUICK_STANDBY,QUICK_TOP_POPUP} quick_control_action_t;
enum { QUICK_DEVICE_INFO=64, QUICK_DEVICE_CLOSE, QUICK_REORDER };
typedef struct {
    bool layout,gestures,pure,popup,touch,top_popup;
    const char *versions[3];
    uint8_t order[QUICK_CONTROL_COUNT];
} lv_quick_controls_state_t;
typedef struct {
    lv_obj_t *pager,*viewport,*tiles[QUICK_CONTROL_COUNT];
    lv_obj_t *states[QUICK_CONTROL_COUNT],*versions[3],*icons[QUICK_CONTROL_COUNT];
    lv_obj_t *switches[QUICK_CONTROL_COUNT],*knobs[QUICK_CONTROL_COUNT];
    lv_obj_t *arrows[QUICK_CONTROL_COUNT],*handles[QUICK_CONTROL_COUNT];
    lv_obj_t *device_button,*version_panel;
    lv_quick_controls_state_t state;bool initialized,editing;
    int selected;
    bool pointer_down,pointer_swiping,pointer_drag,pointer_moved,pointer_vertical;
    lv_point_t pointer_start;
    uint32_t pointer_tick;
    int pointer_tile,pointer_selected,drag_source,drag_target;
    uint8_t pointer_page;
} lv_quick_controls_t;
bool lv_quick_controls_create(lv_quick_controls_t *,lv_obj_t *parent,int x,int y,lv_event_cb_t);
bool lv_quick_controls_refresh(lv_quick_controls_t *,lv_quick_controls_state_t);
void lv_quick_controls_set_editing(lv_quick_controls_t *,bool editing,int selected);
void lv_quick_controls_show_versions(lv_quick_controls_t *,bool show);
lv_quick_controls_state_t lv_quick_controls_current_state(void);
const char *lv_quick_controls_title(quick_control_action_t action);
bool lv_quick_controls_execute(quick_control_action_t action);
/* Own one viewport contact through release; headers/footer stay with the host.
 * Tap uses the original click callback, long-press drag emits QUICK_REORDER. */
bool lv_quick_controls_pointer(lv_quick_controls_t *,lv_event_code_t,const lv_point_t *,uint8_t count);
/* A host with a vertical sheet gesture may take that contact with its origin.
 * Other hosts can keep consuming vertical movement without an action. */
bool lv_quick_controls_take_vertical(lv_quick_controls_t *,lv_point_t *start,uint32_t *tick);
bool lv_quick_controls_reset(lv_quick_controls_t *);
uint8_t lv_quick_controls_page(const lv_quick_controls_t *);
#endif
