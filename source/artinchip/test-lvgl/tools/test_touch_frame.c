#include "un260/gesture/touch_frame.h"
#include <assert.h>
#include <stdio.h>
static void feed(touch_frame_t *f, int type, int code, int value)
{
    struct input_event e = {.type=type, .code=code, .value=value};
    touch_frame_feed(f, &e);
}
#define ABS(c,v) feed(&f,EV_ABS,c,v)
#define SYN() feed(&f,EV_SYN,SYN_REPORT,0)
int main(void)
{
    touch_frame_t f;
    touch_frame_init(&f, true);
    ABS(ABS_MT_SLOT,0); ABS(ABS_MT_TRACKING_ID,11);
    ABS(ABS_MT_POSITION_X,100); ABS(ABS_MT_POSITION_Y,300);
    assert(f.count==0); SYN(); assert(f.count==1 && f.x==100 && f.y==300);
    ABS(ABS_MT_SLOT,1); ABS(ABS_MT_TRACKING_ID,12);
    ABS(ABS_MT_POSITION_X,200); ABS(ABS_MT_POSITION_Y,300);
    SYN(); assert(f.count==2 && f.x==150);
    ABS(ABS_MT_SLOT,0); ABS(ABS_MT_POSITION_Y,200);
    ABS(ABS_MT_SLOT,1); ABS(ABS_MT_POSITION_Y,200);
    SYN(); assert(f.count==2 && f.y==200);
    ABS(ABS_MT_SLOT,99); ABS(ABS_MT_POSITION_Y,0);
    SYN(); assert(f.count==2 && f.y==200);
    ABS(ABS_MT_SLOT,0); ABS(ABS_MT_TRACKING_ID,-1);
    SYN(); assert(f.count==1 && f.x==200);
    ABS(ABS_MT_SLOT,1); ABS(ABS_MT_TRACKING_ID,-1);
    SYN(); assert(f.count==0);
    ABS(ABS_MT_TRACKING_ID,99); SYN();
    assert(f.count==1 && f.x==200 && f.y==200);
    feed(&f,EV_KEY,BTN_TOUCH,0); SYN(); assert(f.count==0);
    ABS(ABS_MT_TRACKING_ID,100); SYN(); assert(f.count==1 && f.y==200);
    ABS(ABS_MT_TRACKING_ID,-1); SYN();
    ABS(ABS_MT_SLOT,15); ABS(ABS_MT_TRACKING_ID,20);
    ABS(ABS_MT_POSITION_X,1200); ABS(ABS_MT_POSITION_Y,350);
    SYN(); assert(f.count==1 && f.x==1200);
    feed(&f,EV_SYN,SYN_DROPPED,0); SYN(); assert(f.count==0 && f.recovery);
    ABS(ABS_MT_POSITION_X,10); SYN(); assert(f.count==0);
    feed(&f,EV_KEY,BTN_TOUCH,0); SYN(); assert(!f.recovery);
    ABS(ABS_MT_SLOT,0); ABS(ABS_MT_TRACKING_ID,31);
    ABS(ABS_MT_POSITION_X,1); ABS(ABS_MT_POSITION_Y,2);
    SYN(); assert(f.count==1);
    touch_frame_init(&f, false);
    ABS(ABS_MT_POSITION_X,20); ABS(ABS_MT_POSITION_Y,30);
    feed(&f,EV_SYN,SYN_MT_REPORT,0);
    ABS(ABS_MT_POSITION_X,40); ABS(ABS_MT_POSITION_Y,50);
    feed(&f,EV_SYN,SYN_MT_REPORT,0);
    SYN(); assert(f.count==2 && f.x==30 && f.y==40);
    SYN(); assert(f.count==0);
    puts("touch_frame: PASS (B/A, centroid, release, invalid slot, slot15, SYN_DROPPED recovery)");
}
