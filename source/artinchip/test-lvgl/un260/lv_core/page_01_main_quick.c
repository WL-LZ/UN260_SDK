#include "page_01_main_quick.h"
#include "page_01_main.h"
#include "page_01_main_layout.h"
#include "lv_page_manager.h"
#include "lv_port_indev.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/app_service/workspace_service.h"
#include "un260/app_service/setting_service.h"
#include "un260/lv_components/lv_quick_controls.h"
#include "un260/lv_resources/ui_icons.h"
#include "un260/device_info/device_info.h"
#include "un260/gesture/gesture_service.h"
#include "un260/gesture/gesture_guide.h"
#include "un260/lv_components/lv_dma_snapshot_cache.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_resources/lv_img_init.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/machine_state/machine_state.h"
#include "un260/protocol/protocol_send.h"
#include "un260/app_service/app_standby_runtime.h"
#include "un260/lv_system/ui_state_runtime.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/counting/counting_data_store.h"
#include "un260/currency/currency_state.h"
#include "un260/font/main_fonts.h"
#include "un260/font/ui_message_font.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_instrument_sans_semibold_25);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_13);
enum { SHEET_W=1280, SHEET_H=328, SHEET_Y=0, CLOSED_Y=-328 };
enum { QC_ARRANGE=240, QC_CANCEL, QC_DONE, QC_CLOSE };
typedef enum {
    QC_POST_NONE, QC_POST_STANDBY, QC_POST_PURE, QC_POST_GESTURE_GUIDE, QC_POST_TOOL=100
} qc_post_action_t;
static struct {
    lv_obj_t *main,*root,*sheet,*grab,*message,*page_label,*context;
    lv_obj_t *arrange,*cancel,*done,*close;
    bool arranging;
    int selected;
    uint8_t order[QUICK_CONTROL_COUNT];
    lv_quick_controls_t controls;
    lv_dma_static_surface_t surface;
    lv_timer_t *timer;
    bool active,moving,opening,prepared,dirty,down,candidate,dragging,drain,tap_grab,navigation_blocked;
    bool preload_failed;
    bool from_open;
    qc_post_action_t after_close;
    lv_point_t start,point;
    uint32_t tick;
    int y,origin_y;
    language_t language;
    uint32_t page;
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
        (uint32_t)ui_manager_get_current_page()==quick.page && !ui_manager_is_transitioning() &&
        !smart_island_is_expanded() &&
        !fault_popup_is_showing();
}
static bool qc_guide_safe(void)
{
    return !app_command_runtime_count_start_busy() && !machine_state_aging_running() &&
        !fault_popup_get_pending_fault(NULL,NULL,NULL);
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
    lv_obj_set_style_text_font(o,ui_message_font(font),0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    return o;
}
static void qc_icon(lv_obj_t *p,int x,int y,const char *path)
{ lv_obj_t *o=lv_img_create(p);lv_img_set_src(o,path);lv_obj_set_pos(o,x,y); }
static void qc_visible(lv_obj_t *o,bool show)
{ if(show)lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN); }
static void qc_refresh(void)
{
    if(!quick.sheet)return;
    lv_quick_controls_state_t state=lv_quick_controls_current_state();
    if(quick.arranging)memcpy(state.order,quick.order,sizeof(state.order));
    if(lv_quick_controls_refresh(&quick.controls,state))quick.dirty=true;
    qc_text(quick.page_label,ui_tr(quick.page==UI_PAGE_PURE?UI_N_("Pure"):UI_N_("Main")));
    char code[4],text[48];currency_state_get_effective_code(code);
    const counting_sim_t *data=counting_data_current();
    snprintf(text,sizeof(text),"%s   |   %d %s",currency_state_display_code(code),data?data->total_pcs:0,ui_tr("pcs"));
    qc_text(quick.context,text);
}
static void qc_edit(bool edit)
{
    quick.arranging=edit;quick.selected=-1;
    if(edit)ui_state_quick_order_get(quick.order);
    lv_quick_controls_show_versions(&quick.controls,false);
    qc_visible(quick.arrange,!edit);qc_visible(quick.close,!edit);
    qc_visible(quick.cancel,edit);qc_visible(quick.done,edit);qc_visible(quick.context,!edit);
    lv_quick_controls_set_editing(&quick.controls,edit,-1);
    qc_text(quick.message,edit?ui_tr("Drag or tap two shortcuts to swap."):"");
    qc_refresh();quick.dirty=true;
}
static void qc_swap(unsigned first,unsigned second)
{
    unsigned a=0,b=0;
    for(unsigned i=0;i<QUICK_CONTROL_COUNT;i++){
        if(quick.order[i]==first)a=i;
        if(quick.order[i]==second)b=i;
    }
    uint8_t id=quick.order[a];quick.order[a]=quick.order[b];quick.order[b]=id;
    quick.selected=-1;qc_refresh();
    lv_quick_controls_set_editing(&quick.controls,true,-1);quick.dirty=true;
}
static void qc_pick(unsigned id)
{
    if(quick.selected>=0 && quick.selected!=(int)id)qc_swap(quick.selected,id);
    else {
        quick.selected=quick.selected==(int)id?-1:(int)id;
        lv_quick_controls_set_editing(&quick.controls,true,quick.selected);quick.dirty=true;
    }
}
static void qc_action(lv_event_t *event)
{
    if(quick.moving || !quick.active)return;
    unsigned id=(uintptr_t)lv_event_get_user_data(event);
    if(!qc_safe()) { qc_close_now();return; }
    if(id==QC_ARRANGE){qc_edit(true);return;}
    if(id==QC_CANCEL){qc_edit(false);return;}
    if(id==QC_DONE){
        if(ui_state_quick_order_save(quick.order))qc_edit(false);
        else qc_text(quick.message,ui_tr("Could not save shortcut order."));
        return;
    }
    if(id==QUICK_DEVICE_INFO || id==QUICK_DEVICE_CLOSE){
        bool show=id==QUICK_DEVICE_INFO && lv_obj_has_flag(quick.controls.version_panel,LV_OBJ_FLAG_HIDDEN);
        lv_quick_controls_show_versions(&quick.controls,show);quick.dirty=true;return;
    }
    if(id==QUICK_REORDER){
        if(quick.arranging)qc_swap(quick.controls.drag_source,quick.controls.drag_target);
        return;
    }
    if(id>=QUICK_CONTROL_COUNT){qc_settle(false);return;}
    if(quick.arranging){qc_pick(id);return;}
    if(id>=QUICK_CALCULATOR&&id<=QUICK_OPERATORS){quick.after_close=(qc_post_action_t)(QC_POST_TOOL+id);qc_settle(false);return;}
    if(id==QUICK_STANDBY){
        const char *reason=app_standby_runtime_blocker();
        if(reason && !app_standby_runtime_needs_fault_review()){
            qc_text(quick.message,ui_tr(reason));return;
        }
    }
    if(id==QUICK_STANDBY || id==QUICK_PURE){
        quick.after_close=id==QUICK_STANDBY?QC_POST_STANDBY:QC_POST_PURE;
        qc_settle(false);return;
    }
    bool guide=id==QUICK_GESTURES && !gesture_service_enabled() && quick.page!=UI_PAGE_PURE;
    if(!lv_quick_controls_execute(id) && id!=QUICK_EXPORT)
        qc_text(quick.message,qc_tr(UI_TEXT_QUICK_SAVE_FAILED));
    else qc_text(quick.message,"");
    qc_refresh();quick.dirty=true;
    if(guide && gesture_service_enabled()){quick.after_close=QC_POST_GESTURE_GUIDE;qc_settle(false);}
}
static lv_obj_t *qc_button(int x,int w,const char *text,const char *image,unsigned action,bool filled)
{
    lv_obj_t *o=qc_box(quick.sheet,x,11,w,44,filled?0xE4ECF2:0xF0F4F7,10);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xDBE6EF),LV_STATE_PRESSED);
    if(image)qc_icon(o,15,14,image);
    lv_obj_t *label=qc_label(o,image?40:8,12,w-(image?48:16),text,&lv_font_instrument_sans_medium_14,0x253D4C);
    if(!image)lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_add_event_cb(o,qc_action,LV_EVENT_CLICKED,(void *)(uintptr_t)action);return o;
}
static void qc_build(void)
{
    if(quick.root && quick.language!=ui_lang_get()) {
        lv_dma_static_surface_release(&quick.surface);lv_obj_del(quick.root);
        quick.root=quick.sheet=NULL;quick.prepared=false;quick.dirty=true;
    }
    if(quick.root)return;
    quick.language=ui_lang_get();quick.preload_failed=false;
    quick.root=qc_box(quick.main,0,0,1280,400,0x203343,0);
    /* Keep the outside-click shield, but never dim the exposed Main area.
     * Only the opaque sheet moves; its parent stays transparent throughout. */
    lv_obj_set_style_bg_opa(quick.root,LV_OPA_TRANSP,0);
    lv_obj_add_flag(quick.root,LV_OBJ_FLAG_HIDDEN);
    quick.sheet=qc_box(quick.root,0,SHEET_Y,SHEET_W,SHEET_H,0xF0F4F7,22);
    /* All child coordinates are physical display coordinates: no implicit
     * padding/border offsets between this sheet and its captured image. */
    lv_obj_t *cap=qc_box(quick.sheet,0,0,SHEET_W,22,0xF0F4F7,0);
    lv_obj_clear_flag(cap,LV_OBJ_FLAG_CLICKABLE);
    qc_icon(quick.sheet,27,22,LVGL_DIR "quick_icons/settings.png");
    lv_obj_t *title=qc_label(quick.sheet,63,17,LV_SIZE_CONTENT,qc_tr(UI_TEXT_QUICK_TITLE),&lv_font_instrument_sans_semibold_25,0x253D4C);
    lv_obj_update_layout(title);
    int divider_x=63+lv_obj_get_width(title)+12;
    lv_obj_t *divider=qc_box(quick.sheet,divider_x,25,1,16,0xCDDBE4,0);
    lv_obj_clear_flag(divider,LV_OBJ_FLAG_CLICKABLE);
    quick.page_label=qc_label(quick.sheet,divider_x+19,25,LV_SIZE_CONTENT,"",&lv_font_instrument_sans_medium_13,0x718B9B);
    if(!lv_quick_controls_create(&quick.controls,quick.sheet,25,65,qc_action)){
        lv_obj_del(quick.root);quick.root=quick.sheet=NULL;
        quick.message=quick.page_label=quick.context=NULL;
        quick.arrange=quick.cancel=quick.done=quick.close=NULL;
        memset(&quick.controls,0,sizeof(quick.controls));
        quick.arranging=false;quick.selected=-1;quick.preload_failed=true;qc_close_now();return;
    }
    quick.arrange=qc_button(1011,118,ui_tr("Arrange"),LVGL_DIR "quick_icons/arrange.png",QC_ARRANGE,false);
    quick.close=qc_button(1143,112,qc_tr(UI_TEXT_QUICK_CLOSE),LVGL_DIR "quick_icons/up.png",QC_CLOSE,true);
    quick.cancel=qc_button(1041,100,ui_tr("Cancel"),NULL,QC_CANCEL,false);
    quick.done=qc_button(1155,100,ui_tr("Done"),NULL,QC_DONE,true);
    qc_visible(quick.cancel,false);qc_visible(quick.done,false);
    quick.message=qc_label(quick.sheet,510,282,490,"",&lv_font_instrument_sans_medium_13,0x946215);
    lv_obj_set_style_text_align(quick.message,LV_TEXT_ALIGN_CENTER,0);
    quick.context=qc_label(quick.sheet,1020,282,235,"",&lv_font_instrument_sans_medium_13,0x7B919F);
    lv_obj_set_style_text_align(quick.context,LV_TEXT_ALIGN_RIGHT,0);
    lv_obj_t *grip=qc_box(quick.sheet,614,314,54,4,0xBDCEDB,2);lv_obj_clear_flag(grip,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *outline=qc_box(quick.sheet,0,0,SHEET_W,SHEET_H,0xF0F4F7,22);
    lv_obj_set_style_bg_opa(outline,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(outline,1,0);
    lv_obj_set_style_border_color(outline,lv_color_hex(0xE1E8EE),0);
    lv_obj_clear_flag(outline,LV_OBJ_FLAG_CLICKABLE);
    quick.selected=-1;
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
    if(!quick.root)return false;
    if(lv_quick_controls_finish(&quick.controls))quick.dirty=true;
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
    if(quick.navigation_blocked){
        gesture_service_set_navigation_blocked(quick.page,false);quick.navigation_blocked=false;
    }
    lv_anim_del(&quick,qc_animation_exec);
    quick.active=quick.moving=quick.dragging=quick.candidate=false;
    quick.after_close=QC_POST_NONE;
    if(quick.arranging)qc_edit(false);
    if(lv_quick_controls_reset(&quick.controls))quick.dirty=true;
    if(quick.sheet && !lv_obj_has_flag(quick.controls.version_panel,LV_OBJ_FLAG_HIDDEN)){
        lv_quick_controls_show_versions(&quick.controls,false);quick.dirty=true;
    }
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
        if(action>=QC_POST_TOOL){(void)lv_quick_controls_execute((quick_control_action_t)(action-QC_POST_TOOL));}
        else if(action==QC_POST_GESTURE_GUIDE) {
            if(qc_guide_safe() && gesture_service_enabled() && !gesture_guide_is_open())gesture_guide_show();
        } else if(action==QC_POST_STANDBY || action==QC_POST_PURE) {
            bool ok=lv_quick_controls_execute(action==QC_POST_STANDBY?QUICK_STANDBY:QUICK_PURE);
            if(!ok && qc_safe()){
                const char *reason=action==QC_POST_STANDBY?app_standby_runtime_blocker():NULL;
                qc_text(quick.message,reason?ui_tr(reason):qc_tr(UI_TEXT_QUICK_SAVE_FAILED));
                qc_settle(true);
            }
        }
    }
}
static void qc_settle(bool open)
{
    if(!quick.root)return;
    if(!open && quick.arranging)qc_edit(false);
    if(!open && !lv_obj_has_flag(quick.controls.version_panel,LV_OBJ_FLAG_HIDDEN)){
        lv_quick_controls_show_versions(&quick.controls,false);quick.dirty=true;
    }
    lv_anim_del(&quick,qc_animation_exec);quick.opening=open;quick.dragging=false;
    int from=quick.moving?quick.y:(quick.active?SHEET_Y:CLOSED_Y);
    /* Refresh only once per transition, never from the animation callback. */
    bool captured=quick.moving?quick.prepared:qc_prepare();
    if(!quick.root)return;
    if(open && quick.page==UI_PAGE_MAIN && !quick.navigation_blocked){
        gesture_service_set_navigation_blocked(quick.page,true);quick.navigation_blocked=true;
    }
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
        if(qc_safe() && !page_01_main_layout_is_editing() &&
           !(quick.preload_failed && quick.language==ui_lang_get()))qc_prepare();
    }
}
void page_01_main_quick_attach(lv_obj_t *host, uint32_t page)
{
    if(!host)return;
    if(quick.main==host && quick.page==page)return;
    if(quick.main) {
        page_01_main_quick_suspend();
        if(quick.root)lv_obj_set_parent(quick.root,host);
        if(quick.grab)lv_obj_set_parent(quick.grab,host);
        quick.main=host;quick.page=page;quick.dirty=true;
        return;
    }
    quick.main=host;quick.page=page;quick.y=CLOSED_Y;
    quick.grab=qc_box(host,610,2,60,4,0x9EAFBC,2);lv_obj_clear_flag(quick.grab,LV_OBJ_FLAG_CLICKABLE);
    quick.timer=lv_timer_create(qc_timer_cb,180,NULL);lv_timer_pause(quick.timer);
}
void page_01_main_quick_schedule_preload(void)
{ if(quick.timer) { lv_timer_set_period(quick.timer,180);lv_timer_reset(quick.timer);lv_timer_resume(quick.timer); } }
bool page_01_main_quick_prepare_preload(void)
{
    if(!quick.main || !lv_obj_is_visible(quick.main) ||
       (uint32_t)ui_manager_get_current_page()!=quick.page ||
       quick.active || quick.moving)return false;
    if(quick.timer)lv_timer_pause(quick.timer);
    if(quick.preload_failed && quick.language==ui_lang_get())return false;
    return qc_prepare();
}
bool page_01_main_quick_prepare_static_step(void)
{
    if(!quick.main || !lv_obj_is_visible(quick.main) || quick.active || quick.moving ||
       (quick.root && quick.language==ui_lang_get()) ||
       (quick.preload_failed && quick.language==ui_lang_get()))return false;
    qc_build();
    return quick.root!=NULL;
}
void page_01_main_quick_refresh_data(uint32_t topics)
{
    if(!(topics&UI_DATA_TOPIC_DEVICE_VERSION))return;
    quick.dirty=true;
    if(quick.active && !quick.moving)qc_refresh();
}
void page_01_main_quick_suspend(void)
{
    qc_close_now();
    /* Main's pointer policy is detached while hidden and cannot observe the
     * release on the next page. The input service still drains any captured
     * sequence; only forget this page's stale contact, never adopt PRESSING. */
    quick.down=quick.drain=quick.tap_grab=false;
}
void page_01_main_quick_detach(void)
{
    qc_close_now();
    if(quick.timer)lv_timer_del(quick.timer);
    lv_dma_static_surface_release(&quick.surface);
    if(quick.root)lv_obj_del(quick.root);
    if(quick.grab)lv_obj_del(quick.grab);
    memset(&quick,0,sizeof(quick));
}
void page_01_main_quick_detach_host(lv_obj_t *host)
{ if(quick.main==host)page_01_main_quick_detach(); }
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
    if(!visible && !workspace_service_quick_enabled() && quick.page!=UI_PAGE_PURE) {
        quick.candidate=false;return false;
    }
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
    if(quick.active && !quick.dragging && lv_quick_controls_pointer(&quick.controls,event,&quick.point,count)){
        quick.dirty=true;
        if(!quick.arranging && lv_quick_controls_take_vertical(&quick.controls,&quick.start,&quick.tick)){
            quick.candidate=true;quick.from_open=true;quick.origin_y=SHEET_Y;quick.tap_grab=false;
        } else {quick.candidate=false;return true;}
    }

    if(!was_down && !released && event==LV_EVENT_PRESSED) {
        quick.start=quick.point;quick.tick=lv_tick_get();quick.from_open=quick.active;
        quick.tap_grab=!quick.active && point->y<16 && point->x>=580 && point->x<=700;
        if(quick.active && point->y>=SHEET_Y+SHEET_H) {
            qc_settle(false);quick.drain=true;return true;
        }
        quick.candidate=quick.active || quick.tap_grab ||
            (!quick.active && point->y>=0 &&
             (point->y<24 || (point->y<44 && point->x>=1058 && point->x<1270)));
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
        if(!quick.root){quick.moving=quick.candidate=false;quick.drain=true;return true;}
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
        if(tap) { qc_request_versions();qc_build();if(quick.root){lv_obj_move_foreground(quick.root);qc_settle(true);}return true; }
    }
    return quick.tap_grab;
}
