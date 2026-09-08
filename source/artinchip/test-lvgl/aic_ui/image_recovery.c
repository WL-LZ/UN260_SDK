#include "image_recovery.h"
#include "image_memory.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <string.h>
/* No image copies or dynamically allocated retry nodes. Ownership follows the
 * widget's DELETE event. A source change resets its retry budget. */
#define RECOVERY_SLOTS 64
typedef struct { lv_obj_t *obj; uint64_t key; uint32_t tick; uint8_t tries; bool good, pending; } recovery_t;
static recovery_t slots[RECOVERY_SLOTS];
static lv_timer_t *timer;
static uint32_t failure_serial;
static uint32_t last_overflow_log;
static unsigned tracked;
static uint64_t source_key(const void *src)
{
    if(lv_img_src_get_type(src)!=LV_IMG_SRC_FILE) return (uintptr_t)src;
    uint64_t h=14695981039346656037ULL;
    for(const unsigned char *p=src;*p;p++) { h^=*p; h*=1099511628211ULL; }
    return h;
}
uint32_t image_recovery_failure_serial(void) { return failure_serial; }
static void deleted(lv_event_t *e)
{
    recovery_t *r=lv_event_get_user_data(e);
    if(r->obj==lv_event_get_target(e)) { memset(r,0,sizeof(*r));if(tracked)tracked--; }
}
static void detach(recovery_t *r)
{
    lv_obj_remove_event_cb_with_user_data(r->obj,deleted,r);
    memset(r,0,sizeof(*r));
    if(tracked)tracked--;
}
static void status(lv_obj_t *obj,bool good)
{
    if(!good) failure_serial++;
    if(good && !tracked) return; /* Healthy draw path: no scan/hash/timer work. */
    recovery_t *empty=NULL;
    uint64_t key=source_key(lv_img_get_src(obj));
    for(unsigned i=0;i<RECOVERY_SLOTS;i++) {
        recovery_t *r=&slots[i];
        if(r->obj==obj) {
            if(r->key!=key) { r->key=key;r->tries=0;r->tick=lv_tick_get(); }
            r->good=good;
            r->pending=false;
            lv_timer_resume(timer);
            return;
        }
        if(!r->obj && !empty) empty=r;
    }
    if(good || !lv_img_get_src(obj)) return;
    if(!empty) {
        if(lv_tick_elaps(last_overflow_log)>30000) {
            last_overflow_log=lv_tick_get();fprintf(stderr,"IMG_RECOVERY queue_full capacity=%u\n",RECOVERY_SLOTS);
        }
        return;
    }
    *empty=(recovery_t){.obj=obj,.key=key,.tick=lv_tick_get()};
    tracked++;
    lv_obj_add_event_cb(obj,deleted,LV_EVENT_DELETE,empty);
    fprintf(stderr,"IMG_FALLBACK src=%s retries=3\n",lv_img_src_get_type(lv_img_get_src(obj))==LV_IMG_SRC_FILE ? (const char *)lv_img_get_src(obj):"<internal>");
    lv_timer_resume(timer);
}
static bool may_draw(lv_obj_t *obj)
{
    if(!tracked) return true;
    uint64_t key=source_key(lv_img_get_src(obj));
    for(unsigned i=0;i<RECOVERY_SLOTS;i++)
        if(slots[i].obj==obj && slots[i].key==key && !slots[i].good && slots[i].tries>=3)
            /* Let the last scheduled redraw actually run once. */
            return slots[i].pending;
    return true;
}
static void retry_tick(lv_timer_t *t)
{
    bool active=false;
    for(unsigned i=0;i<RECOVERY_SLOTS;i++) {
        recovery_t *r=&slots[i];
        if(!r->obj) continue;
        if(r->good || !lv_obj_is_visible(r->obj)) { detach(r);continue; }
        if(source_key(lv_img_get_src(r->obj))!=r->key) { detach(r);continue; }
        active=true;
        if(r->tries>=3) continue; /* Monitor visibility; do not perform more I/O. */
        if(lv_tick_elaps(r->tick)<(1500U<<r->tries)) continue;
        r->tries++;r->tick=lv_tick_get();r->pending=true;
        /* Re-read header if necessary. The object retains its source string.
         * No global cache invalidation: that could disrupt another live draw. */
        if(lv_img_src_get_type(lv_img_get_src(r->obj))==LV_IMG_SRC_FILE)
            lv_img_set_src(r->obj,lv_img_get_src(r->obj));
        lv_obj_invalidate(r->obj);
        if(r->tries==3) image_mem_report();
        break; /* At most one recovery invalidation per tick. */
    }
    if(!active) lv_timer_pause(t);
}
void image_recovery_init(void)
{
    if(timer) return;
    timer=lv_timer_create(retry_tick,100,NULL);
    if(!timer) return;
    lv_timer_pause(timer);
    lv_img_set_draw_status_cb(status);
    lv_img_set_draw_retry_cb(may_draw);
}
