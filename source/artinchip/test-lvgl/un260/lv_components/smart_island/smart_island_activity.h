#ifndef SMART_ISLAND_ACTIVITY_H
#define SMART_ISLAND_ACTIVITY_H
#include "lvgl/lvgl.h"
typedef enum {SI_ACTIVITY_NONE,SI_ACTIVITY_WORKING,SI_ACTIVITY_SUCCESS,SI_ACTIVITY_UNCERTAIN,SI_ACTIVITY_ERROR} si_activity_t;
/* Optional background activity, subordinate to count, result and warning scenes. */
bool smart_island_activity_set(si_activity_t kind,const char *title,const char *subtitle,lv_event_cb_t click);
bool smart_island_activity_slot_available(void);
void smart_island_activity_sync(void);
void smart_island_activity_destroy(void);
#endif
