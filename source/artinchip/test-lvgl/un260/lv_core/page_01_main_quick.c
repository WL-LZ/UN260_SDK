#include "page_01_main_quick.h"
#include "page_01_main.h"
#include "page_01_main_layout.h"
#include "lv_page_manager.h"
#include "lv_port_indev.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/device_info/device_info.h"
#include "un260/gesture/gesture_service.h"
#include "un260/gesture/gesture_guide.h"
#include "un260/lv_components/lv_dma_snapshot_cache.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_resources/lv_img_init.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/machine_state/machine_state.h"
#include "un260/protocol/protocol_send.h"
#include "un260/storage/standby_store.h"
#include "un260/font/main_fonts.h"
#include <stdlib.h>
#include <string.h>

enum { SHEET_W=1280, SHEET_H=280, SHEET_Y=0, CLOSED_Y=-280 };
typedef enum {
    QC_POST_NONE, QC_POST_STANDBY, QC_POST_STANDBY_SETTINGS, QC_POST_GESTURE_GUIDE
} qc_post_action_t;
static struct {
    lv_obj_t *main,*root,*sheet,*grab,*switches[2],*thumbs[2],*states[2],*versions[3],*message;
    lv_dma_static_surface_t surface;
    lv_timer_t *timer;
    bool active,moving,opening,prepared,dirty,down,candidate,dragging,drain,tap_grab;
    bool from_open;
    qc_post_action_t after_close;
    lv_point_t start,point;
    uint32_t tick;
    int y,origin_y;
    language_t language;
} quick;

/* Share cancellation across close/fault/suspend/teardown. Without the fade,
 * firmware -O2 otherwise duplicates this path and adds a 4 KiB ELF page. */
static __attribute__((noinline)) void qc_close_now(void);
static void qc_settle(bool open);
static const char *qc_tr(ui_text_id_t id) { return ui_text_get(id); }
static void qc_text(lv_obj_t *o,const char *value)
{ if(strcmp(lv_label_get_text(o),value)) { lv_label_set_text(o,value);quick.dirty=true; } }
static bool qc_safe(void)
{
    return quick.main && lv_obj_is_visible(quick.main) &&
        ui_manager_get_current_page()==UI_PAGE_MAIN && !ui_manager_is_transitioning() &&
        !app_command_runtime_count_start_busy() && !machine_state_aging_running() &&
        !fault_popup_is_showing() && !fault_popup_get_pending_fault(NULL,NULL,NULL);
}
static bool qc_foreign_layer(const lv_point_t *point)
{
    lv_obj_t *layers[]={lv_layer_top(),lv_layer_sys()};
    for(unsigned i=0;i<2;++i) {
        lv_obj_t *hit=lv_indev_search_obj(layers[i],(lv_point_t *)point);
        if(hit && hit!=layers[i])return true;
    }
    lv_obj_t *hit=lv_indev_search_obj(lv_scr_act(),(lv_point_t *)point);
    for(lv_obj_t *o=hit;o;o=lv_obj_get_parent(o))if(o==quick.main)return false;
    return hit && hit!=lv_scr_act();
}
static lv_obj_t *qc_box(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *o=lv_obj_create(p);lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_radius(o,radius,0);lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
static lv_obj_t *qc_label(lv_obj_t *p,int x,int y,int w,const char *value,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *o=lv_label_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text(o,value);
    lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    return o;
}
static void qc_icon(lv_obj_t *p,int x,int y,const char *path)
{ lv_obj_t *o=lv_img_create(p);lv_img_set_src(o,path);lv_obj_set_pos(o,x,y); }
static void qc_refresh(void)
{
    if(!quick.sheet)return;
    bool values[]={page_01_main_layout_is_enabled(),gesture_service_enabled()};
    for(unsigned i=0;i<2;++i) {
        lv_obj_set_style_bg_color(quick.switches[i],lv_color_hex(values[i]?0x1462CC:0xD9E1E6),0);
        lv_obj_set_x(quick.thumbs[i],values[i]?33:3);
        qc_text(quick.states[i],qc_tr(values[i]?UI_TEXT_QUICK_ON:UI_TEXT_QUICK_OFF));
    }
    const char *versions[]={device_info_is_valid()?device_info_main_app():NULL,
        device_info_is_valid()?device_info_image_app():NULL,device_info_display_app()};
    for(unsigned i=0;i<3;++i)
        qc_text(quick.versions[i],versions[i] && *versions[i]?versions[i]:qc_tr(UI_TEXT_QUICK_UNAVAILABLE));
}
static void qc_action(lv_event_t *event)
{
    if(quick.moving || !quick.active)return;
    unsigned id=(uintptr_t)lv_event_get_user_data(event);
    if(!qc_safe()) { qc_close_now();return; }
    if(id<2) {
        if(id==0)page_01_main_layout_set_enabled(!page_01_main_layout_is_enabled());
        else {
            bool enable=!gesture_service_enabled();
            if(!gesture_service_set_enabled(enable)) {
                qc_text(quick.message,qc_tr(UI_TEXT_QUICK_SAVE_FAILED));qc_refresh();return;
            }
            if(enable)quick.after_close=QC_POST_GESTURE_GUIDE;
        }
        qc_text(quick.message,"");qc_refresh();quick.dirty=true;
        if(quick.after_close==QC_POST_GESTURE_GUIDE)qc_settle(false);
    } else if(id==2 || id==4) {
        if(standby_store_busy()) { qc_text(quick.message,qc_tr(UI_TEXT_QUICK_STANDBY_BUSY));return; }
        quick.after_close=id==2?QC_POST_STANDBY:QC_POST_STANDBY_SETTINGS;
        qc_settle(false);
    } else qc_settle(false);
}
static lv_obj_t *qc_card(int x,int w)
{
    lv_obj_t *o=qc_box(quick.sheet,x,104,w,120,0xFFFFFF,16);
    lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,lv_color_hex(0xE9EFF3),0);
    return o;
}
static void qc_build(void)
{
    if(quick.root && quick.language!=ui_lang_get()) {
        lv_dma_static_surface_release(&quick.surface);lv_obj_del(quick.root);
        quick.root=quick.sheet=NULL;quick.prepared=false;quick.dirty=true;
    }
    if(quick.root)return;
    quick.language=ui_lang_get();
    quick.root=qc_box(quick.main,0,0,1280,400,0x203343,0);
    /* Keep the outside-click shield, but never dim the exposed Main area.
     * Only the opaque sheet moves; its parent stays transparent throughout. */
    lv_obj_set_style_bg_opa(quick.root,LV_OPA_TRANSP,0);
    lv_obj_add_flag(quick.root,LV_OBJ_FLAG_HIDDEN);
    quick.sheet=qc_box(quick.root,0,SHEET_Y,SHEET_W,SHEET_H,0xF6F8FA,22);
    lv_obj_set_style_pad_left(quick.sheet,16,0);
    /* Flush top and side edges; only the lower corners remain rounded. The
     * cap is part of this subtree, so it moves in the same raster as the text. */
    lv_obj_t *cap=qc_box(quick.sheet,-16,0,SHEET_W,22,0xF6F8FA,0);
    lv_obj_clear_flag(cap,LV_OBJ_FLAG_CLICKABLE);
    qc_label(quick.sheet,24,18,700,qc_tr(UI_TEXT_QUICK_TITLE),&lv_font_instrument_sans_semibold_28,0x1D2B34);
    qc_label(quick.sheet,24,53,800,qc_tr(UI_TEXT_QUICK_SUBTITLE),&lv_font_instrument_sans_medium_14,0x586B77);
    qc_label(quick.sheet,24,80,830,qc_tr(UI_TEXT_QUICK_PREFERENCES),&lv_font_instrument_sans_semibold_12,0x586B77);
    qc_label(quick.sheet,892,80,324,qc_tr(UI_TEXT_QUICK_VERSIONS),&lv_font_instrument_sans_semibold_12,0x586B77);
    const ui_text_id_t titles[]={UI_TEXT_QUICK_LAYOUT,UI_TEXT_QUICK_GESTURES};
    const ui_text_id_t hints[]={UI_TEXT_QUICK_LAYOUT_HINT,UI_TEXT_QUICK_GESTURE_HINT};
    const char *icons[]={LVGL_DIR "quick_icons/layout.png",LVGL_DIR "quick_icons/gesture.png"};
    for(unsigned i=0;i<2;++i) {
        lv_obj_t *tile=qc_card(24+298*i,286);
        lv_obj_set_style_bg_color(tile,lv_color_hex(0xEDF2F6),LV_STATE_PRESSED);
        lv_obj_add_event_cb(tile,qc_action,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
        qc_icon(tile,16,18,icons[i]);
        qc_label(tile,54,16,138,qc_tr(titles[i]),&lv_font_instrument_sans_semibold_18,0x1D2B34);
        quick.states[i]=qc_label(tile,54,43,135,"",&lv_font_instrument_sans_medium_14,0x586B77);
        qc_label(tile,16,85,254,qc_tr(hints[i]),&lv_font_instrument_sans_medium_12,0x586B77);
        quick.switches[i]=qc_box(tile,202,21,64,34,0x1462CC,17);
        quick.thumbs[i]=qc_box(quick.switches[i],33,3,28,28,0xFFFFFF,14);
        lv_obj_clear_flag(quick.switches[i],LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(quick.thumbs[i],LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t *standby=qc_card(620,240);
    qc_icon(standby,16,18,LVGL_DIR "quick_icons/standby.png");
    qc_label(standby,54,18,122,qc_tr(UI_TEXT_QUICK_STANDBY),&lv_font_instrument_sans_semibold_18,0x1D2B34);
    qc_label(standby,16,46,156,qc_tr(UI_TEXT_QUICK_WAKE_HINT),&lv_font_instrument_sans_medium_12,0x586B77);
    lv_obj_t *settings=qc_box(standby,180,10,44,44,0xEAF0F4,12);
    lv_obj_set_style_border_width(settings,1,0);
    lv_obj_set_style_border_color(settings,lv_color_hex(0xDCE5EC),0);
    lv_obj_set_style_bg_color(settings,lv_color_hex(0xD7E5F6),LV_STATE_PRESSED);
    lv_obj_set_style_border_color(settings,lv_color_hex(0xA8C6EB),LV_STATE_PRESSED);
    qc_icon(settings,9,9,LVGL_DIR "quick_icons/settings.png");
    lv_obj_add_event_cb(settings,qc_action,LV_EVENT_CLICKED,(void *)4);
    lv_obj_t *enter=qc_box(standby,16,65,208,44,0xEAF1FB,12);
    lv_obj_set_style_bg_color(enter,lv_color_hex(0xD7E5F6),LV_STATE_PRESSED);
    lv_obj_add_event_cb(enter,qc_action,LV_EVENT_CLICKED,(void *)2);
    lv_obj_t *enter_text=qc_label(enter,0,0,180,qc_tr(UI_TEXT_QUICK_ENTER),&lv_font_instrument_sans_semibold_16,0x1462CC);
    lv_obj_set_style_text_align(enter_text,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(enter_text);
    lv_obj_t *line=qc_box(quick.sheet,878,80,1,144,0xDFE7ED,0);lv_obj_clear_flag(line,LV_OBJ_FLAG_CLICKABLE);
    const ui_text_id_t version_titles[]={UI_TEXT_QUICK_CONTROLLER,UI_TEXT_QUICK_IMAGE,UI_TEXT_QUICK_UI};
    for(unsigned i=0;i<3;++i) {
        qc_label(quick.sheet,892,111+38*i,146,qc_tr(version_titles[i]),&lv_font_instrument_sans_medium_14,0x586B77);
        quick.versions[i]=qc_label(quick.sheet,1038,108+38*i,180,"",&lv_font_instrument_sans_semibold_18,0x1D2B34);
        lv_obj_set_style_text_align(quick.versions[i],LV_TEXT_ALIGN_RIGHT,0);
    }
    lv_obj_t *close=qc_box(quick.sheet,1090,18,132,44,0xEAF0F4,12);
    lv_obj_set_style_bg_color(close,lv_color_hex(0xDDE6ED),LV_STATE_PRESSED);
    qc_icon(close,15,13,LVGL_DIR "quick_icons/up.png");
    qc_label(close,42,12,82,qc_tr(UI_TEXT_QUICK_CLOSE),&lv_font_instrument_sans_medium_16,0x1D2B34);
    lv_obj_add_event_cb(close,qc_action,LV_EVENT_CLICKED,(void *)3);
    quick.message=qc_label(quick.sheet,24,235,1170,"",&lv_font_instrument_sans_medium_14,0x946215);
    lv_obj_t *grip=qc_box(quick.sheet,598,263,52,4,0xBBC9D3,2);lv_obj_clear_flag(grip,LV_OBJ_FLAG_CLICKABLE);
    /* Inner outline belongs to the captured sheet, never to the outside shield. */
    lv_obj_t *outline=qc_box(quick.sheet,-16,0,SHEET_W,SHEET_H,0xF6F8FA,22);
    lv_obj_set_style_bg_opa(outline,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(outline,2,0);
    lv_obj_set_style_border_color(outline,lv_color_hex(0xDFE7ED),0);
    lv_obj_clear_flag(outline,LV_OBJ_FLAG_CLICKABLE);
    quick.dirty=true;qc_refresh();
}
static void qc_position(int y)
{
    if(y<CLOSED_Y)y=CLOSED_Y;
    if(y>SHEET_Y)y=SHEET_Y;
    quick.y=y;
    if(quick.surface.image)lv_obj_set_y(quick.surface.image,y);
}
static bool qc_prepare(void)
{
    qc_build();qc_refresh();
    if(quick.prepared && !quick.dirty)return true;
    bool hidden=lv_obj_has_flag(quick.root,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(quick.root,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(quick.sheet,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(quick.sheet,SHEET_Y);
    if(quick.surface.image)lv_obj_add_flag(quick.surface.image,LV_OBJ_FLAG_HIDDEN);
    quick.prepared=lv_dma_transition_surface_capture(&quick.surface,quick.sheet,"Main quick controls");
    quick.dirty=!quick.prepared;
    lv_obj_add_flag(quick.sheet,LV_OBJ_FLAG_HIDDEN);
    if(quick.surface.image)lv_obj_add_flag(quick.surface.image,LV_OBJ_FLAG_HIDDEN);
    if(hidden)lv_obj_add_flag(quick.root,LV_OBJ_FLAG_HIDDEN);
    return quick.prepared;
}
static void qc_animation_exec(void *unused,int32_t y) { (void)unused;qc_position(y); }
static void qc_close_now(void)
{
    lv_anim_del(&quick,qc_animation_exec);
    quick.active=quick.moving=quick.dragging=quick.candidate=false;
    quick.after_close=QC_POST_NONE;
    quick.drain=quick.down;
    if(quick.root)lv_obj_add_flag(quick.root,LV_OBJ_FLAG_HIDDEN);
    qc_position(CLOSED_Y);
    if(quick.timer)lv_timer_pause(quick.timer);
}
static void qc_animation_done(lv_anim_t *unused)
{
    (void)unused;quick.moving=false;
    if(quick.opening && qc_safe()) {
        qc_position(SHEET_Y);
        quick.active=true;
        if(quick.surface.image)lv_obj_add_flag(quick.surface.image,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(quick.sheet,LV_OBJ_FLAG_HIDDEN);qc_refresh();
    } else {
        qc_post_action_t action=qc_safe() && !qc_foreign_layer(&quick.point)?
            quick.after_close:QC_POST_NONE;
        qc_close_now();
        /* Do not layer the tutorial or a new page over the moving drawer.
         * Reuse existing workflows only after its overlay has been removed. */
        if(action==QC_POST_GESTURE_GUIDE) {
            if(gesture_service_enabled() && !gesture_guide_is_open())gesture_guide_show();
        } else if((action==QC_POST_STANDBY || action==QC_POST_STANDBY_SETTINGS) && !standby_store_busy()) {
            ui_manager_push_page(action==QC_POST_STANDBY?UI_PAGE_STANDBY:UI_PAGE_STANDBY_SETTING);
        }
    }
}
static void qc_settle(bool open)
{
    if(!quick.root)return;
    lv_anim_del(&quick,qc_animation_exec);quick.opening=open;quick.dragging=false;
    int from=quick.moving?quick.y:(quick.active?SHEET_Y:CLOSED_Y);
    /* Refresh only once per transition, never from the animation callback. */
    bool captured=quick.moving?quick.prepared:qc_prepare();
    quick.moving=true;lv_obj_clear_flag(quick.root,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(quick.sheet,LV_OBJ_FLAG_HIDDEN);
    /* The live fallback needs the same count/fault guard as the DMA path. */
    lv_timer_set_period(quick.timer,50);lv_timer_reset(quick.timer);lv_timer_resume(quick.timer);
    if(!captured) { qc_animation_done(NULL);return; }
    lv_obj_clear_flag(quick.surface.image,LV_OBJ_FLAG_HIDDEN);qc_position(from);
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,&quick);lv_anim_set_exec_cb(&a,qc_animation_exec);
    lv_anim_set_values(&a,from,open?SHEET_Y:CLOSED_Y);
    lv_anim_set_time(&a,open?180:150);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&a,qc_animation_done);lv_anim_start(&a);
}
static void qc_request_versions(void)
{
    if(quick.message)qc_text(quick.message,"");
    if(protocol_send_is_ready()) { uint8_t request=1;protocol_send(0x17,&request,1); }
}
static void qc_timer_cb(lv_timer_t *timer)
{
    if(quick.active || quick.moving) {
        if(!qc_safe() || qc_foreign_layer(&quick.point))qc_close_now();
    } else {
        lv_timer_pause(timer);
        if(qc_safe() && !page_01_main_layout_is_editing())qc_prepare();
    }
}
void page_01_main_quick_attach(lv_obj_t *main)
{
    page_01_main_quick_detach();quick.main=main;quick.y=CLOSED_Y;
    quick.grab=qc_box(main,610,2,60,4,0x9EAFBC,2);lv_obj_clear_flag(quick.grab,LV_OBJ_FLAG_CLICKABLE);
    quick.timer=lv_timer_create(qc_timer_cb,180,NULL);lv_timer_pause(quick.timer);
}
void page_01_main_quick_schedule_preload(void)
{ if(quick.timer) { lv_timer_set_period(quick.timer,180);lv_timer_reset(quick.timer);lv_timer_resume(quick.timer); } }
void page_01_main_quick_refresh_data(uint32_t topics)
{
    if(!(topics&UI_DATA_TOPIC_DEVICE_VERSION))return;
    quick.dirty=true;
    if(quick.active && !quick.moving)qc_refresh();
}
void page_01_main_quick_suspend(void) { qc_close_now(); }
void page_01_main_quick_detach(void)
{
    qc_close_now();
    if(quick.timer)lv_timer_del(quick.timer);
    lv_dma_static_surface_release(&quick.surface);
    if(quick.root)lv_obj_del(quick.root);
    if(quick.grab)lv_obj_del(quick.grab);
    memset(&quick,0,sizeof(quick));
}
bool page_01_main_quick_request_back(void)
{
    if(!quick.active && !quick.moving)return false;
    quick.after_close=QC_POST_NONE;qc_settle(false);return true;
}
bool page_01_main_quick_is_open(void) { return quick.active || quick.moving; }
bool page_01_main_quick_pointer(lv_indev_t *indev,lv_event_code_t event,const lv_point_t *point,uint8_t count)
{
    (void)indev;
    bool was_down=quick.down,released=event==LV_EVENT_RELEASED || count==0;
    quick.down=!released;if(point)quick.point=*point;
    if(quick.drain) { if(released)quick.drain=false;return true; }
    bool visible=quick.active || quick.moving;
    if(!qc_safe() || (point && qc_foreign_layer(point))) {
        quick.candidate=false;
        if(visible) { qc_close_now();return true; }
        return false;
    }
    if(!released && count!=1) {
        if(visible) { qc_close_now();return true; }
        quick.candidate=false;return false;
    }
    if(quick.moving && !quick.dragging)return true;
    if(!was_down && !released && event==LV_EVENT_PRESSED) {
        quick.start=quick.point;quick.tick=lv_tick_get();quick.from_open=quick.active;
        quick.tap_grab=!quick.active && point->y<16 && point->x>=580 && point->x<=700;
        if(quick.active && point->y>=SHEET_Y+SHEET_H) {
            qc_settle(false);quick.drain=true;return true;
        }
        quick.candidate=quick.active || quick.tap_grab ||
            (!quick.active && point->y<44 && point->x>=1058 && point->x<1270);
        quick.origin_y=quick.active?SHEET_Y:CLOSED_Y;
        if(quick.tap_grab)return true;
    }
    if(!quick.candidate)return false;
    int dx=quick.point.x-quick.start.x,dy=quick.point.y-quick.start.y;
    if(!quick.dragging && abs(dx)>18 && abs(dx)>abs(dy)) { quick.candidate=false;return false; }
    if(!released && !quick.dragging && (quick.from_open?-dy:dy)>=10 && abs(dy)>abs(dx)) {
        quick.dragging=true;quick.opening=!quick.from_open;
        if(!quick.from_open)qc_request_versions();
        qc_prepare();quick.moving=true;
        lv_obj_move_foreground(quick.root);lv_obj_clear_flag(quick.root,LV_OBJ_FLAG_HIDDEN);
        if(quick.surface.image && quick.prepared)lv_obj_clear_flag(quick.surface.image,LV_OBJ_FLAG_HIDDEN);
        lv_timer_set_period(quick.timer,50);lv_timer_reset(quick.timer);lv_timer_resume(quick.timer);
    }
    if(quick.dragging) {
        int y=quick.origin_y+dy;if(y>SHEET_Y)y=SHEET_Y;if(y<CLOSED_Y)y=CLOSED_Y;
        qc_position(y);
        if(released) {
            bool cross=abs(dy)>=90 || (abs(dy)>=45 && lv_tick_elaps(quick.tick)<=350);
            quick.candidate=false;qc_settle(quick.from_open?!cross:cross);
        }
        return true;
    }
    if(released) {
        bool tap=quick.tap_grab && abs(dx)<10 && abs(dy)<10;
        quick.candidate=false;quick.tap_grab=false;
        if(tap) { qc_request_versions();qc_build();lv_obj_move_foreground(quick.root);qc_settle(true);return true; }
    }
    return quick.tap_grab;
}
