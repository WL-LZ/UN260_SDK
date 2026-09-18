#include "boot_theme_config.h"
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D
#include "un260/lv_core/page_08_boot.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/boot/boot_service.h"
#include <string.h>
#include "aic_ui/generated_assets/selfcheck_icons.h"
LV_FONT_DECLARE(lv_font_instrument_sans_medium_12);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_16);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_14);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_36);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_16);

/* This page is a projection of the boot service. No inferred successes,
 * timed percentage increments, protocol requests or independent Ready flags. */
static const char background[]="L:/usr/local/share/lvgl_data/boot_theme_d/background.png";
static const char ready_background[]="L:/usr/local/share/lvgl_data/backgrounds/boot.png";
typedef struct { lv_obj_t *seal,*check,*title,*sub,*signature; } ready_visual_t;
static const char *names[5]={"Configuration","Sensors","Motor","Electromagnet","Image board"};
enum { WAIT, CHECK, PASS, FAIL };
static struct {
    lv_obj_t *root,*title,*count,*progress,*percent,*caption,*track,*section;
    lv_obj_t *cards[5],*icons[5],*cores[5],*marks[5],*labels[5],*states[5];
    lv_obj_t *waiting;
    lv_obj_t *ready_bg;
    ready_visual_t ready;
    lv_timer_t *timer;
    uint32_t shown_at,active_at[5],done_at;
    uint8_t phase[5],step,completed;
    bool covered,visible,done,failed;
} view;
static bool defer_create;
/* Disable only the full-surface incoming fade on unusually slow display ports. */
#ifndef UI_BOOT_HANDOFF_FADE_ENABLE
#define UI_BOOT_HANDOFF_FADE_ENABLE 1
#endif
static struct {lv_obj_t *root,*image;ready_visual_t ready;lv_timer_t *timer;uint32_t tick;uint8_t phase;} bridge;
static void set_text(lv_obj_t *o,const char *s){if(o&&strcmp(lv_label_get_text(o),s))lv_label_set_text_static(o,s);}

static float smooth(float p){if(p<=0)return 0;if(p>=1)return 1;return p*p*(3-2*p);}
static float settle(float p){if(p<=0)return 0;if(p>=1)return 1;float q=1-p;return 1-q*q*q*q;}
static float span(uint32_t t,uint32_t start,uint32_t length){return t<=start?0:smooth((float)(t-start)/length);}
static void text_alpha(lv_obj_t *o,lv_opa_t a){if(o&&lv_obj_get_style_text_opa(o,0)!=a)lv_obj_set_style_text_opa(o,a,0);}
static void bg_alpha(lv_obj_t *o,lv_opa_t a){if(o&&lv_obj_get_style_bg_opa(o,0)!=a)lv_obj_set_style_bg_opa(o,a,0);}
static lv_obj_t *label(lv_obj_t *parent,int x,int y,int w,const char *text,const lv_font_t *font,uint32_t color){
    lv_obj_t *o=lv_label_create(parent);if(!o)return NULL;
    lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text_static(o,text);return o;
}
static void ready_create(lv_obj_t *parent, ready_visual_t *v)
{
    static const lv_point_t points[]={{7,14},{12,19},{21,9}};
    v->seal=lv_obj_create(parent);
    if(v->seal){lv_obj_remove_style_all(v->seal);lv_obj_set_pos(v->seal,615,101);lv_obj_set_size(v->seal,50,50);
        lv_obj_set_style_radius(v->seal,LV_RADIUS_CIRCLE,0);lv_obj_set_style_bg_color(v->seal,lv_color_hex(0xE4EDF2),0);
        lv_obj_clear_flag(v->seal,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        v->check=lv_line_create(v->seal);
        if(v->check){lv_obj_set_pos(v->check,11,11);lv_line_set_points(v->check,points,3);lv_obj_set_style_line_width(v->check,2,0);lv_obj_set_style_line_rounded(v->check,true,0);lv_obj_set_style_line_color(v->check,lv_color_hex(0x598197),0);}}
    v->title=label(parent,0,174,1280,"Ready to count.",&lv_font_instrument_sans_medium_36,0x304652);
    v->sub=label(parent,0,230,1280,"Precision starts here.",&lv_font_instrument_sans_medium_16,0x869BAB);
    v->signature=label(parent,0,366,1280,"UN260  /  PRECISION IN EVERY NOTE",&lv_font_instrument_sans_medium_12,0x90A4B3);
    lv_obj_t *labels[]={v->title,v->sub,v->signature};
    for(unsigned i=0;i<3;i++)if(labels[i])lv_obj_set_style_text_align(labels[i],LV_TEXT_ALIGN_CENTER,0);
}
static void ready_part(lv_obj_t *o,lv_opa_t a,int base,int dy)
{
    if(!o)return;
    text_alpha(o,a);
    if(lv_obj_get_y(o)!=base+dy)lv_obj_set_y(o,base+dy);
}
static void ready_render(ready_visual_t *v,uint32_t age,float exit)
{
    float icon=span(age,1240,730),title=span(age,1390,360),sub=span(age,1560,360),sig=span(age,1810,360);
    bg_alpha(v->seal,(lv_opa_t)(255*icon*(1-exit)));
    if(v->seal){int y=101+(int)(13*(1-icon)-7*exit);if(lv_obj_get_y(v->seal)!=y)lv_obj_set_y(v->seal,y);}
    if(v->check){lv_opa_t a=(lv_opa_t)(255*icon*(1-exit));if(lv_obj_get_style_line_opa(v->check,0)!=a)lv_obj_set_style_line_opa(v->check,a,0);}
    ready_part(v->title,(lv_opa_t)(255*title*(1-exit)),174,(int)(12*(1-title)-7*exit));
    ready_part(v->sub,(lv_opa_t)(255*sub*(1-exit)),230,(int)(9*(1-sub)-7*exit));
    ready_part(v->signature,(lv_opa_t)(255*sig*(1-exit)),366,0);
}
static void stop(void){if(view.waiting){lv_obj_del(view.waiting);view.waiting=NULL;}lv_timer_t *t=view.timer;view.timer=NULL;if(t)lv_timer_del(t);}
static void deleted(lv_event_t *e){if(lv_event_get_target(e)!=view.root)return;stop();if(!bridge.root)lv_img_cache_invalidate_src(background);memset(&view,0,sizeof(view));}
static void apply(uint32_t now){
    if(!view.root||view.covered||view.step)return;
    boot_snapshot_t s;boot_service_snapshot(&s);
    uint32_t age=now-view.shown_at;
    bool failed=s.stage==BOOT_STAGE_FAIL;unsigned passed=0;
    for(unsigned i=0;i<5;i++){
        uint8_t state=s.items[i].received?(s.items[i].result==1?PASS:FAIL):
            s.connected&&s.requested_count==i+1?CHECK:WAIT;
        /* Configuration also represents controller startup/handshake waiting.
         * This is visual only; received results always win and progress stays real. */
        if(i==0&&!s.items[i].received)state=CHECK;
        if(s.stage==BOOT_STAGE_FAIL&&state==CHECK)state=FAIL;
        if(state==FAIL)failed=true;
        if(state==PASS)passed++;
        if(view.phase[i]!=state){
            view.phase[i]=state;view.active_at[i]=now;
            if(i==0&&state==CHECK&&age<970)view.active_at[i]=view.shown_at+970;
            uint32_t color=state==FAIL?0xB53622:state==PASS?0x209A78:state==CHECK?0x0074F8:0xA1B0BC;
            if(view.states[i]){lv_label_set_text_static(view.states[i],state==FAIL?"Not passed":state==PASS?"Ready":state==CHECK?"Checking":"Waiting");lv_obj_set_style_text_color(view.states[i],lv_color_hex(color),0);}
            if(view.icons[i]){lv_obj_set_style_border_color(view.icons[i],lv_color_hex(color),0);lv_obj_set_style_bg_color(view.icons[i],lv_color_hex(color),0);}
            if(view.marks[i]){
                if(state==PASS||state==FAIL){lv_img_set_src(view.marks[i],state==PASS?&selfcheck_pass:&selfcheck_fail);lv_obj_clear_flag(view.marks[i],LV_OBJ_FLAG_HIDDEN);}
                else lv_obj_add_flag(view.marks[i],LV_OBJ_FLAG_HIDDEN);
            }
            if(view.cards[i])lv_obj_set_style_bg_color(view.cards[i],lv_color_hex(state==FAIL?0xFFF2ED:0xFFFFFF),0);
        }
    }
    if(view.completed!=s.completed_count){
        view.completed=s.completed_count;
        if(view.progress)lv_obj_set_width(view.progress,1176*s.completed_count/5);
        if(view.percent)lv_label_set_text_fmt(view.percent,"%u%%",s.completed_count*20U);
        if(view.count)lv_label_set_text_fmt(view.count,"%u / 5 complete",s.completed_count);
    }
    bool ready=s.stage==BOOT_STAGE_DONE&&passed==5&&!failed;
    if(ready&&!view.done){view.done=true;view.done_at=now;}
    if(!ready)view.done=false;
    if(failed!=view.failed){view.failed=failed;if(view.title)lv_label_set_text_static(view.title,failed?"Needs attention.":"Getting ready.");}
    set_text(view.caption,failed?"Check the reported fault before continuing.":"Checking your system.");
    if(view.caption){lv_color_t c=lv_color_hex(failed?0xB53622:0x68818F);if(lv_obj_get_style_text_color(view.caption,0).full!=c.full)lv_obj_set_style_text_color(view.caption,c,0);}
    if(!ready&&!failed&&!view.waiting){
        view.waiting=lv_spinner_create(view.root,900,72);
        if(view.waiting){lv_obj_set_pos(view.waiting,52,154);lv_obj_set_size(view.waiting,28,28);
            lv_obj_set_style_arc_width(view.waiting,3,LV_PART_MAIN);
            lv_obj_set_style_arc_color(view.waiting,lv_color_hex(0xD9D9DC),LV_PART_MAIN);
            lv_obj_set_style_arc_width(view.waiting,3,LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(view.waiting,lv_color_hex(0x0074F8),LV_PART_INDICATOR);
            lv_obj_clear_flag(view.waiting,LV_OBJ_FLAG_CLICKABLE);}
    }else if((ready||failed)&&view.waiting){lv_obj_del(view.waiting);view.waiting=NULL;}
    if(view.progress){lv_color_t color=lv_color_hex(failed?0xB53622:ready?0x209A78:0x0074F8);if(lv_obj_get_style_bg_color(view.progress,0).full!=color.full)lv_obj_set_style_bg_color(view.progress,color,0);}
    uint32_t done_age=view.done?now-view.done_at:0;
    lv_opa_t heading_opa=(lv_opa_t)(255*(view.done?1-span(done_age,700,540):span(age,0,650)));
    text_alpha(view.title,heading_opa);text_alpha(view.caption,heading_opa);
    if(view.ready_bg){if(view.done&&done_age>=700)lv_obj_clear_flag(view.ready_bg,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(view.ready_bg,LV_OBJ_FLAG_HIDDEN);}
    ready_render(&view.ready,view.done?done_age:0,0);
    for(unsigned i=0;i<5;i++){
        float entry=span(age,i*80U,650U),leave=view.done?span(done_age,720U+i*45U,490U):0;
        float alpha=entry*(1-leave);uint8_t state=view.phase[i];
        int32_t active_delta=(int32_t)(now-view.active_at[i]);
        uint32_t active=active_delta>0?(uint32_t)active_delta:0;
        float activation=state==WAIT?0:settle((float)active/720.0f);
        lv_opa_t ink=(lv_opa_t)(255*alpha*(.48f+.52f*activation));
        if(view.cards[i]){int y=216+(int)(5*(1-entry)+5*(1-activation)*entry-4*leave+.5f);if(lv_obj_get_y(view.cards[i])!=y)lv_obj_set_y(view.cards[i],y);bg_alpha(view.cards[i],(lv_opa_t)(alpha*(38+(state==CHECK?207:72)*activation)));}
        if(view.cards[i]){lv_opa_t sh=(lv_opa_t)(alpha*18);if(lv_obj_get_style_shadow_opa(view.cards[i],0)!=sh)lv_obj_set_style_shadow_opa(view.cards[i],sh,0);}
        text_alpha(view.labels[i],ink);text_alpha(view.states[i],ink);
        if(view.marks[i]){lv_opa_t a=(lv_opa_t)(255*alpha);if(lv_obj_get_style_img_opa(view.marks[i],0)!=a)lv_obj_set_style_img_opa(view.marks[i],a,0);}
        if(view.icons[i]){
            float pulse=1;
            if(state==CHECK&&(active<5400U||i==0)){uint32_t p=active%1800U;pulse=p<900?1-span(p,0,900):span(p,900,900);}
            bg_alpha(view.icons[i],0);
            bg_alpha(view.cores[i],state==CHECK?(lv_opa_t)(alpha*255*(.42f+.58f*pulse)):0);
            lv_opa_t border=state==PASS||state==FAIL?0:ink;if(lv_obj_get_style_border_opa(view.icons[i],0)!=border)lv_obj_set_style_border_opa(view.icons[i],border,0);
        }
    }
    lv_opa_t trim=view.done?(lv_opa_t)(255*(1-span(done_age,700,540))):255;
    text_alpha(view.section,trim);bg_alpha(view.track,trim);text_alpha(view.count,trim);text_alpha(view.percent,trim);bg_alpha(view.progress,trim);
}
static void tick(lv_timer_t *t){(void)t;if(ui_manager_get_current_page()!=UI_PAGE_BOOT){stop();return;}apply(lv_tick_get());}
bool ui_page_08_curr_visual_is_quiet(void){
    return !view.root || (view.done && !view.failed && lv_tick_elaps(view.done_at)>=2170);
}
bool ui_page_08_curr_ready_hold_complete(void){
    /* Title reaches full opacity at 1750 ms; hold it for 600 ms.
     * All staggered text has finished by 2170 ms. */
    return !view.root || (view.done && !view.failed && lv_tick_elaps(view.done_at)>=2350);
}
static void bridge_deleted(lv_event_t *e){
    if(lv_event_get_target(e)!=bridge.root)return;
    lv_timer_t *t=bridge.timer;memset(&bridge,0,sizeof(bridge));if(t)lv_timer_del(t);
    lv_img_cache_invalidate_src(background);
}
static void bridge_tick(lv_timer_t *t){
    (void)t;ui_page_t page=ui_manager_get_current_page();
    if(page!=UI_PAGE_MAIN&&page!=UI_PAGE_PURE){lv_obj_del(bridge.root);return;}
    /* Creation/resume and the first covered render are NOT animation time.
     * Keep the cover opaque for one refresh before starting the visual clock. */
    if(bridge.phase==0){bridge.phase=1;return;}
    if(bridge.phase==1){bridge.phase=2;bridge.tick=lv_tick_get();}
    uint32_t elapsed=lv_tick_elaps(bridge.tick);
    if(elapsed>=1000){lv_obj_del(bridge.root);return;}
    /* Retire old text first. Only then admit Main, never overlap text layers.
     * Image opacity uses the existing renderer; no snapshot, zoom or obj layer. */
    float retired=span(elapsed,0,360);
    ready_render(&bridge.ready,2440,retired);
#if UI_BOOT_HANDOFF_FADE_ENABLE
    lv_opa_t a=(lv_opa_t)(255*(1-span(elapsed,400,600)));
#else
    lv_opa_t a=elapsed<400?255:0;
#endif
    if(lv_obj_get_style_img_opa(bridge.image,0)!=a)lv_obj_set_style_img_opa(bridge.image,a,0);
}
void ui_page_08_curr_start_handoff(void){
    if(!view.done||view.failed)return;
    if(bridge.root)lv_obj_del(bridge.root);
    bridge.root=lv_obj_create(lv_layer_top());if(!bridge.root)return;
    lv_obj_remove_style_all(bridge.root);lv_obj_set_size(bridge.root,1280,400);
    lv_obj_clear_flag(bridge.root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(bridge.root,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bridge.root,bridge_deleted,LV_EVENT_DELETE,NULL);
    bridge.image=lv_img_create(bridge.root);if(!bridge.image){lv_obj_del(bridge.root);return;}
    lv_img_set_src(bridge.image,ready_background);
    ready_create(bridge.root,&bridge.ready);ready_render(&bridge.ready,2440,0);
    bridge.tick=lv_tick_get();bridge.timer=lv_timer_create(bridge_tick,LV_DISP_DEF_REFR_PERIOD,NULL);
    if(!bridge.timer)lv_obj_del(bridge.root);
}
static void make_card(unsigned i){
    lv_obj_t *o=lv_obj_create(view.root);view.cards[i]=o;if(!o)return;
    lv_obj_remove_style_all(o);lv_obj_set_pos(o,52+i*238,216);lv_obj_set_size(o,224,88);
    lv_obj_set_style_shadow_width(o,8,0);lv_obj_set_style_shadow_ofs_y(o,2,0);lv_obj_set_style_shadow_color(o,lv_color_hex(0x8B9DB4),0);
    lv_obj_set_style_radius(o,13,0);lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    view.labels[i]=label(o,46,23,168,names[i],&lv_font_instrument_sans_medium_16,0x243F51);
    view.states[i]=label(o,46,49,168,"Waiting",&lv_font_instrument_sans_medium_12,0xA1B0BC);
    view.icons[i]=lv_obj_create(o);if(view.icons[i]){
        lv_obj_t *icon=view.icons[i];lv_obj_remove_style_all(icon);lv_obj_set_pos(icon,16,35);lv_obj_set_size(icon,18,18);
        lv_obj_set_style_radius(icon,LV_RADIUS_CIRCLE,0);lv_obj_set_style_border_width(icon,1,0);lv_obj_clear_flag(icon,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        view.cores[i]=lv_obj_create(icon);
        if(view.cores[i]){lv_obj_remove_style_all(view.cores[i]);lv_obj_set_pos(view.cores[i],5,5);lv_obj_set_size(view.cores[i],6,6);
            lv_obj_set_style_radius(view.cores[i],LV_RADIUS_CIRCLE,0);lv_obj_set_style_bg_color(view.cores[i],lv_color_hex(0x0074F8),0);
            lv_obj_clear_flag(view.cores[i],LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);}
        view.marks[i]=lv_img_create(o);
        if(view.marks[i]){lv_obj_set_pos(view.marks[i],16,35);lv_img_set_src(view.marks[i],&selfcheck_pass);lv_obj_add_flag(view.marks[i],LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(view.marks[i],LV_OBJ_FLAG_CLICKABLE);}
    }
    view.phase[i]=255;
}
bool ui_page_08_curr_prepare_step(void){
    if(!view.root||!view.step)return true;
    unsigned step=view.step++;
    if(step==1){
        lv_obj_t *img=lv_img_create(view.root);if(img){lv_img_set_src(img,background);(void)_lv_img_cache_open(background,lv_color_black(),0);}
    }else if(step==2){
        view.ready_bg=lv_img_create(view.root);if(view.ready_bg){lv_img_set_src(view.ready_bg,ready_background);lv_obj_add_flag(view.ready_bg,LV_OBJ_FLAG_HIDDEN);(void)_lv_img_cache_open(ready_background,lv_color_black(),0);}
        view.title=label(view.root,52,110,800,"Getting ready.",&lv_font_instrument_sans_medium_36,0x344759);
        view.caption=label(view.root,96,159,800,"Checking your system.",&lv_font_instrument_sans_medium_16,0x68818F);

        view.section=label(view.root,1098,107,130,"SELF-CHECK",&lv_font_instrument_sans_semibold_14,0x68818F);
        view.count=label(view.root,1068,132,160,"0 / 5 complete",&lv_font_instrument_sans_semibold_14,0x788B99);
        lv_obj_t *track=lv_obj_create(view.root);view.track=track;if(track){lv_obj_remove_style_all(track);lv_obj_set_pos(track,52,324);lv_obj_set_size(track,1176,2);lv_obj_set_style_bg_color(track,lv_color_hex(0xD1DCE4),0);lv_obj_set_style_bg_opa(track,255,0);}
        view.progress=lv_obj_create(view.root);if(view.progress){lv_obj_remove_style_all(view.progress);lv_obj_set_pos(view.progress,52,324);lv_obj_set_size(view.progress,0,2);lv_obj_set_style_bg_opa(view.progress,255,0);}
        view.percent=label(view.root,1148,338,80,"0%",&lv_font_instrument_sans_semibold_16,0x788B99);
        if(view.section)lv_obj_set_style_text_align(view.section,LV_TEXT_ALIGN_RIGHT,0);
        if(view.count)lv_obj_set_style_text_align(view.count,LV_TEXT_ALIGN_RIGHT,0);
        if(view.percent)lv_obj_set_style_text_align(view.percent,LV_TEXT_ALIGN_RIGHT,0);
    }else if(step<=7){make_card(step-3);}else{
        ready_create(view.root,&view.ready);ready_render(&view.ready,0,0);
        view.step=0;view.shown_at=lv_tick_get();view.completed=255;
        if(!view.covered){view.visible=true;apply(view.shown_at);view.timer=lv_timer_create(tick,LV_DISP_DEF_REFR_PERIOD,NULL);}return true;
    }return false;
}
void ui_page_08_curr_defer_next_create(void){defer_create=true;}
void ui_page_08_curr_create(lv_obj_t *parent){
    if(view.root)return;
    bool deferred=defer_create;defer_create=false;
    memset(&view,0,sizeof(view));if(!parent)parent=lv_scr_act();view.root=lv_obj_create(parent);if(!view.root)return;
    lv_obj_remove_style_all(view.root);lv_obj_set_size(view.root,1280,400);lv_obj_set_style_bg_color(view.root,lv_color_hex(0xF2F5F7),0);lv_obj_set_style_bg_opa(view.root,255,0);
    lv_obj_clear_flag(view.root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(view.root,deleted,LV_EVENT_DELETE,NULL);view.step=1;
    if(!deferred)while(!ui_page_08_curr_prepare_step()){}
}
void ui_page_08_curr_set_covered(bool covered){
    if(!view.root)return;
    view.covered=covered;
    if(covered){if(view.waiting){lv_obj_del(view.waiting);view.waiting=NULL;}stop();lv_obj_add_flag(view.root,LV_OBJ_FLAG_HIDDEN);return;}
    while(!ui_page_08_curr_prepare_step()){}
    lv_obj_clear_flag(view.root,LV_OBJ_FLAG_HIDDEN);view.shown_at=lv_tick_get();view.visible=true;apply(view.shown_at);
    if(!view.timer)view.timer=lv_timer_create(tick,LV_DISP_DEF_REFR_PERIOD,NULL);
}
void ui_page_08_curr_destroy(void){defer_create=false;stop();if(view.root)lv_obj_del(view.root);memset(&view,0,sizeof(view));}
/* Legacy notification ABI: snapshots, not caller percentages, own this view. */
void boot_progress_set(uint8_t p){(void)p;apply(lv_tick_get());}
void boot_progress_reset(void){apply(lv_tick_get());}
void boot_selftest_list_reset(void){apply(lv_tick_get());}
void boot_selftest_list_sync_step(uint8_t step){(void)step;apply(lv_tick_get());}
void boot_selftest_list_set_result(uint8_t i,uint8_t result){(void)i;(void)result;apply(lv_tick_get());}
void boot_selftest_list_finish(void){apply(lv_tick_get());}
#endif
