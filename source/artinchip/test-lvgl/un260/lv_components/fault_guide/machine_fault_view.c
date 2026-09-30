#include "machine_fault_view.h"
#include "machine_fault_assets.h"
#include <string.h>

#define STAGE_W 510
#define STAGE_H 273
#define FRAME_MS 50U
#define CYCLE_MS 4700U
typedef struct { int x,y,w,h; } box_t;
static const box_t boxes[]={{113,15,254,267},{115,15,250,265},{112,19,256,268},{118,19,242,264}};

static int vx(const machine_fault_view_t *v, int raw)
{
    box_t b=boxes[v->step.view];
    return (STAGE_W* b.h - b.w*STAGE_H)/ (2*b.h) + (raw-b.x)*STAGE_H/b.h;
}
static int vy(const machine_fault_view_t *v, int raw)
{
    box_t b=boxes[v->step.view]; return (raw-b.y)*STAGE_H/b.h;
}
static int vs(const machine_fault_view_t *v, int raw)
{
    return raw*STAGE_H/boxes[v->step.view].h;
}

static void rect(lv_draw_ctx_t *ctx, const lv_area_t *origin, int x,int y,int w,int h,
                 int radius, uint32_t fill, lv_opa_t opacity, uint32_t stroke, int border)
{
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color=lv_color_hex(fill); d.bg_opa=opacity; d.radius=radius;
    d.border_width=border; d.border_color=lv_color_hex(stroke);
    lv_area_t a={origin->x1+x,origin->y1+y,origin->x1+x+w-1,origin->y1+y+h-1};
    lv_draw_rect(ctx,&d,&a);
}
static void line(lv_draw_ctx_t *ctx,const lv_area_t *a,int x1,int y1,int x2,int y2,uint32_t color,int width)
{
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);d.color=lv_color_hex(color);d.width=width;d.round_start=1;d.round_end=1;
    lv_point_t p1={a->x1+x1,a->y1+y1},p2={a->x1+x2,a->y1+y2}; lv_draw_line(ctx,&d,&p1,&p2);
}
static void focus(lv_draw_ctx_t *ctx, const lv_area_t *a,machine_fault_view_t *v,int x,int y,int w,int h,bool round)
{
    unsigned phase=v->elapsed_ms%2500U;
    unsigned pulse=phase<1250U?phase:2500U-phase;
    rect(ctx,a,vx(v,x),vy(v,y),vs(v,w),vs(v,h),round?LV_RADIUS_CIRCLE:5,
         0xEF9C36,(lv_opa_t)(22U+pulse*30U/1250U),0xDF9636,2);
}
static void draw_overlay(lv_event_t *event)
{
    machine_fault_view_t *v=lv_event_get_user_data(event);
    if (!v) return;
    lv_draw_ctx_t *ctx=lv_event_get_draw_ctx(event);lv_area_t a;lv_obj_get_coords(v->overlay,&a);
    const mf_step_t *s=&v->step;
    if (s->zone==MF_PRESET) {
        unsigned t=v->elapsed_ms%3000U;
        int lift=t<500U?0:t<2200U?(int)((t-500U)*27U/1700U):27;
        for (int i=2;i>=0;--i) {
            int x=vx(v,173+i*4),y=vy(v,61+i*5-lift);
            int w=vs(v,126),h=vs(v,24);
            rect(ctx,&a,x,y,w,h,3,0xB9D8C5,LV_OPA_COVER,0x386F59,1);
            rect(ctx,&a,x+vs(v,8),y+vs(v,5),w-vs(v,16),h-vs(v,10),2,
                 0xDDEADF,LV_OPA_COVER,0x7EA790,1);
            line(ctx,&a,x+vs(v,22),y+vs(v,12),x+vs(v,45),y+vs(v,12),0x56836C,1);
            line(ctx,&a,x+w-vs(v,47),y+vs(v,12),x+w-vs(v,23),y+vs(v,12),0x56836C,1);
        }
        int ax=vx(v,327),ay=vy(v,74);
        line(ctx,&a,ax,ay,ax,ay-vs(v,32),0xBD6624,2);
        line(ctx,&a,ax,ay-vs(v,32),ax-vs(v,6),ay-vs(v,25),0xBD6624,2);
        line(ctx,&a,ax,ay-vs(v,32),ax+vs(v,6),ay-vs(v,25),0xBD6624,2);
        return;
    }
    if (s->view==MF_SIDE && s->zone==MF_ENCODERS) {
        focus(ctx,&a,v,184,112,64,64,true); focus(ctx,&a,v,249,113,44,44,true);
    } else if(s->view==MF_SIDE && s->zone==MF_IMAGEBOARD) focus(ctx,&a,v,151,181,69,79,false);
    else if(s->view==MF_FRONT) {
        if(s->zone==MF_HOPPER) focus(ctx,&a,v,158,44,164,33,false);
        else if(s->zone==MF_REJECT) focus(ctx,&a,v,163,150,154,41,false);
        else if(s->zone==MF_STACKER) focus(ctx,&a,v,198,210,85,52,false);
        else focus(ctx,&a,v,122,77,236,195,false);
    } else if(s->view==MF_TOP) {
        if(s->zone==MF_MACHINE) focus(ctx,&a,v,128,29,224,243,false);
        else if(v->motion>550U || s->action==MF_FOCUS) focus(ctx,&a,v,161,137,158,64,false);
    } else if(s->view==MF_REAR) {
        if(s->zone==MF_MACHINE) focus(ctx,&a,v,145,75,190,192,false);
        else if(v->motion>550U) focus(ctx,&a,v,155,180,170,50,false);
        else focus(ctx,&a,v,149,177,182,44,false);
    } else focus(ctx,&a,v,139,77,192,187,false);

    if(s->action==MF_REMOVE) {
        unsigned t=v->elapsed_ms%3500U;
        unsigned p=t<700U?0U:t<2660U?(t-700U)*1000U/1960U:1000U;
        int raw_y=s->view==MF_TOP?157:s->view==MF_REAR?198:s->zone==MF_HOPPER?45:s->zone==MF_REJECT?160:226;
        int dx=s->zone==MF_HOPPER?0:(int)(39U*p/1000U),dy=-(int)(28U*p/1000U);
        if(t<3350U) {
            int x=vx(v,209+dx),y=vy(v,raw_y+dy),w=vs(v,61),h=vs(v,23);
            rect(ctx,&a,x,y,w,h,3,0xE1EFE7,LV_OPA_COVER,0x5C9A86,1);
            rect(ctx,&a,x+5,y+4,w-10,h-8,2,0xEDF6F1,LV_OPA_COVER,0x8EBDAA,1);
            rect(ctx,&a,x+w/2-4,y+7,8,8,LV_RADIUS_CIRCLE,0x9FCAB9,LV_OPA_COVER,0,0);
        }
    } else if(s->action==MF_CLEAN) {
        unsigned p=v->elapsed_ms%3600U;int sweep=(int)(p<1800U?p:3600U-p)*50/1800-25;
        int raw_y=s->view==MF_TOP?165:s->view==MF_REAR?199:s->zone==MF_REJECT?166:s->zone==MF_HOPPER?62:223;
        int x=vx(v,220+sweep),y=vy(v,raw_y);
        rect(ctx,&a,x,y,vs(v,42),vs(v,12),3,0xFCFEFF,LV_OPA_COVER,0x6089A5,1);
        rect(ctx,&a,x+vs(v,12),y-vs(v,15),vs(v,19),vs(v,16),3,0x88ABC1,LV_OPA_COVER,0x6089A5,1);
        for(int i=3;i<40;i+=5) line(ctx,&a,x+vs(v,i),y+vs(v,11),x+vs(v,i),y+vs(v,16),0x688DA5,1);
    }
    if(s->action==MF_OPEN || s->action==MF_CLOSE) {
        bool close=s->action==MF_CLOSE;
        int x=vx(v,s->view==MF_REAR?352:137);
        int y1=vy(v,s->view==MF_REAR?187:100),y2=vy(v,s->view==MF_REAR?250:184);
        if((s->view==MF_REAR)==close) { int t=y1;y1=y2;y2=t; }
        line(ctx,&a,x,y1,x,y2,0x2475CC,2);
        int dir=y2>y1?1:-1;
        line(ctx,&a,x,y2,x-6,y2-dir*8,0x2475CC,2);line(ctx,&a,x,y2,x+6,y2-dir*8,0x2475CC,2);
    }
}

static void set_asset(lv_obj_t *image,const machine_fault_asset_t *asset)
{
    lv_img_set_src(image,asset->image);lv_obj_set_pos(image,asset->x,asset->y);
}

static void update_frame(machine_fault_view_t *v)
{
    mf_action_t action=v->step.action;
    uint32_t t=v->elapsed_ms%CYCLE_MS;
    uint16_t amount=t<700U?0:t<3050U?(uint16_t)((t-700U)*1000U/2350U):1000;
    if(action==MF_CLOSE) amount=1000U-amount;
    else if(action!=MF_OPEN) amount=(action==MF_REMOVE||action==MF_CLEAN)?1000:0;
    v->motion=amount;
    if(v->step.view==MF_TOP) {
        /* Focus-only top diagrams expose the passage without suggesting an operation. */
        uint8_t frame=(action==MF_FOCUS)?11:(uint8_t)(amount*11U/1000U);
        if(frame!=v->lid_frame) {set_asset(v->lid,&mf_asset_lid[frame]);v->lid_frame=frame;}
    } else if(v->step.view==MF_REAR) {
        int offset=vs(v,56)*(int)amount/1000;
        lv_obj_set_y(v->drawer,mf_asset_rear_tray.y-vy(v,176)-vs(v,56)+offset);
        lv_obj_set_y(v->face,mf_asset_rear_face.y+offset);
    }
    lv_obj_invalidate(v->overlay);
}
static void tick(lv_timer_t *timer)
{
    machine_fault_view_t *v=timer->user_data;
    uint32_t now=lv_tick_get();
    if(!v->root || !v->playing || !lv_obj_is_visible(v->root)) {v->last_tick=now;return;}
    v->elapsed_ms+=(uint32_t)(now-v->last_tick);v->last_tick=now;update_frame(v);
}
static lv_obj_t *plain(lv_obj_t *parent,int x,int y,int w,int h)
{
    lv_obj_t *obj=lv_obj_create(parent);lv_obj_remove_style_all(obj);lv_obj_set_pos(obj,x,y);lv_obj_set_size(obj,w,h);
    lv_obj_clear_flag(obj,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);return obj;
}
void machine_fault_view_create(machine_fault_view_t *v,lv_obj_t *parent,int x,int y)
{
    memset(v,0,sizeof(*v));v->root=plain(parent,x,y,STAGE_W,STAGE_H);
    v->base=lv_img_create(v->root);v->drawer_clip=plain(v->root,0,0,STAGE_W,STAGE_H);
    v->drawer=lv_img_create(v->drawer_clip);v->face=lv_img_create(v->root);v->lid=lv_img_create(v->root);
    v->overlay=plain(v->root,0,0,STAGE_W,STAGE_H);lv_obj_add_event_cb(v->overlay,draw_overlay,LV_EVENT_DRAW_MAIN,v);
    v->timer=lv_timer_create(tick,FRAME_MS,v);v->playing=true;v->last_tick=lv_tick_get();
}
void machine_fault_view_set_step(machine_fault_view_t *v,const mf_step_t *step)
{
    if(!v || !v->root || !step) return;
    v->step=*step;v->lid_frame=255;
    const machine_fault_asset_t *base=step->view==MF_TOP?&mf_asset_top:step->view==MF_REAR?&mf_asset_rear:step->view==MF_SIDE?&mf_asset_side:&mf_asset_front;
    set_asset(v->base,base);
    lv_obj_add_flag(v->lid,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(v->drawer_clip,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(v->face,LV_OBJ_FLAG_HIDDEN);
    if(step->view==MF_TOP) lv_obj_clear_flag(v->lid,LV_OBJ_FLAG_HIDDEN);
    if(step->view==MF_REAR) {
        lv_obj_set_pos(v->drawer_clip,0,vy(v,176));lv_obj_set_size(v->drawer_clip,STAGE_W,vs(v,62));
        set_asset(v->drawer,&mf_asset_rear_tray);set_asset(v->face,&mf_asset_rear_face);
        lv_obj_clear_flag(v->drawer_clip,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(v->face,LV_OBJ_FLAG_HIDDEN);
    }
    machine_fault_view_restart(v);
}
void machine_fault_view_play(machine_fault_view_t *v,bool playing)
{
    if(!v)return;
    v->playing=playing;v->last_tick=lv_tick_get();
    if(v->timer) {if(playing)lv_timer_resume(v->timer);else lv_timer_pause(v->timer);}
}
void machine_fault_view_restart(machine_fault_view_t *v)
{
    if(!v)return;
    v->elapsed_ms=0;machine_fault_view_play(v,true);update_frame(v);
}
void machine_fault_view_destroy(machine_fault_view_t *v)
{
    if(!v)return;
    if(v->timer)lv_timer_del(v->timer);
    if(v->root)lv_obj_del(v->root);
    memset(v,0,sizeof(*v));
}
