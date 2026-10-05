#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "lv_quick_controls.h"
#include "lv_settings.h"
#include "lv_fault_popup.h"
#include "ui_notice.h"
#include "lv_content_pager.h"
#include "smart_island.h"
#include "un260/lv_resources/ui_icons.h"
#include "un260/font/ui_message_font.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_state_runtime.h"
#include "un260/lv_system/ui_export_data.h"
#include "un260/lv_core/page_01_main_layout.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_18_pure.h"
#include "un260/gesture/gesture_service.h"
#include "un260/gesture/touch_feedback.h"
#include "un260/device_info/device_info.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/storage/standby_store.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "un260/app_service/app_standby_runtime.h"

LV_FONT_DECLARE(lv_font_instrument_sans_medium_13);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_20);

static const char *const titles[QUICK_CONTROL_COUNT]={
    UI_N_("Layout edit"),UI_N_("Gestures"),UI_N_("Pure count"),UI_N_("Popup"),
    UI_N_("Touch guide"),UI_N_("Export"),UI_N_("QR code"),UI_N_("Standby"),UI_N_("Top popup")};
static const char *const subtitles[QUICK_CONTROL_COUNT]={
    UI_N_("Long-press on Main"),UI_N_("Swipe navigation"),UI_N_("Focused counting"),
    UI_N_("Auto-open fault guide"),UI_N_("Show touch points"),UI_N_("Current result to USB"),
    UI_N_("Share current result"),UI_N_("Touch screen to wake"),UI_N_("Show operation messages")};
static const char *const icons[QUICK_CONTROL_COUNT]={
    "layout", "gesture", "pure", "popup", "touch", "export", "qr", "standby", "popup"};
static bool is_switch(unsigned id)
{ return id==QUICK_LAYOUT || id==QUICK_GESTURES || id==QUICK_POPUP || id==QUICK_TOUCH || id==QUICK_TOP_POPUP; }
static bool action_enabled(const lv_quick_controls_state_t *s,unsigned id)
{
    switch(id){
        case QUICK_LAYOUT:return s->layout;
        case QUICK_GESTURES:return s->gestures;
        case QUICK_PURE:return s->pure;
        case QUICK_POPUP:return s->popup;
        case QUICK_TOUCH:return s->touch;
        case QUICK_TOP_POPUP:return s->top_popup;
        default:return false;
    }
}
static lv_obj_t *box(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *o=lv_obj_create(p);lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_radius(o,radius,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);return o;
}
static lv_obj_t *copy(lv_obj_t *p,const char *s,int x,int y,int w,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *l=lv_settings_label(p,s,x,y,font,color);lv_obj_set_width(l,w);
    lv_obj_set_style_text_font(l,ui_message_font(font),0);
    lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);return l;
}
static lv_obj_t *icon(lv_obj_t *parent,const char *name,int x,int y)
{
    char path[160];snprintf(path,sizeof(path),LVGL_DIR "quick_icons/%s.png",name);
    lv_obj_t *o=ui_icon_create(parent,path);lv_obj_set_pos(o,x,y);return o;
}
static void visible(lv_obj_t *o,bool show)
{ if(!o)return;if(show)lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN); }
const char *lv_quick_controls_title(quick_control_action_t action)
{return (unsigned)action<QUICK_CONTROL_COUNT?ui_tr(titles[action]):"";}
lv_quick_controls_state_t lv_quick_controls_current_state(void)
{
    lv_quick_controls_state_t s={
        .layout=page_01_main_layout_is_enabled(),.gestures=gesture_service_enabled(),
        .pure=ui_manager_get_current_page()==UI_PAGE_PURE,.popup=fault_popup_get_auto_enabled(),
        .touch=touch_feedback_enabled(),.top_popup=ui_notice_get_enabled(),
        .versions={device_info_is_valid()?device_info_main_app():NULL,
            device_info_is_valid()?device_info_image_app():NULL,device_info_display_app()}};
    ui_state_quick_order_get(s.order);return s;
}
bool lv_quick_controls_create(lv_quick_controls_t *v,lv_obj_t *parent,int x,int y,lv_event_cb_t cb)
{
    memset(v,0,sizeof(*v));v->selected=-1;
    v->pager=lv_content_pager_create(parent,x,y,1230,220,
        (QUICK_CONTROL_COUNT+QUICK_CONTROLS_PER_PAGE-1)/QUICK_CONTROLS_PER_PAGE);
    if(!v->pager)return false;
    v->viewport=lv_content_pager_get_viewport(v->pager);
    lv_obj_clear_flag(v->viewport,LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(v->pager,LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_content_pager_set_indicator_position(v->pager,205,214);
    lv_obj_add_event_cb(v->viewport,cb,LV_EVENT_VALUE_CHANGED,(void *)QUICK_REORDER);
    for(unsigned i=0;i<QUICK_CONTROL_COUNT;i++){
        lv_obj_t *tile=box(lv_content_pager_get_page(v->pager,0),0,0,299,96,0xFFFFFF,13);
        v->tiles[i]=tile;
        lv_obj_set_style_border_width(tile,1,0);
        lv_obj_set_style_border_color(tile,lv_color_hex(0xDCE5EC),0);
        lv_obj_set_style_bg_color(tile,lv_color_hex(0xE5EFF8),LV_STATE_PRESSED);
        lv_obj_add_flag(tile,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tile,cb,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
        v->icons[i]=icon(tile,icons[i],25,34);
        copy(tile,ui_tr(titles[i]),72,24,155,&lv_font_instrument_sans_semibold_20,0x253D4C);
        copy(tile,ui_tr(subtitles[i]),72,54,172,&lv_font_instrument_sans_medium_13,0x6D8494);
        if(is_switch(i)){
            v->switches[i]=box(tile,232,35,46,26,0xDCE4EB,13);
            v->knobs[i]=box(v->switches[i],4,4,18,18,0xFFFFFF,9);
        } else {
            v->arrows[i]=icon(tile,"arrow",265,40);
            if(i==QUICK_PURE)
                v->states[i]=copy(tile,"",211,41,49,&lv_font_instrument_sans_medium_13,0x1674E8);
        }
        v->handles[i]=icon(tile,"drag",245,36);visible(v->handles[i],false);
    }
    v->device_button=box(parent,x,267,184,44,0xF0F4F7,8);
    lv_obj_add_flag(v->device_button,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(v->device_button,cb,LV_EVENT_CLICKED,(void *)QUICK_DEVICE_INFO);
    copy(v->device_button,"UN260",0,13,55,&lv_font_instrument_sans_semibold_14,0x56798F);
    box(v->device_button,55,15,1,12,0xD2DFE7,0);
    copy(v->device_button,ui_tr("Device info"),66,13,84,&lv_font_instrument_sans_medium_13,0x7C919F);
    icon(v->device_button,"info",144,15);
    v->version_panel=box(parent,x,99,300,176,0xFFFFFF,14);
    lv_obj_set_style_border_width(v->version_panel,1,0);
    lv_obj_set_style_border_color(v->version_panel,lv_color_hex(0xD4E0E8),0);
    lv_obj_add_flag(v->version_panel,LV_OBJ_FLAG_CLICKABLE);
    copy(v->version_panel,ui_tr("Device info"),20,16,220,&lv_font_instrument_sans_semibold_20,0x253D4C);
    lv_obj_t *close=box(v->version_panel,246,8,44,44,0xF0F4F7,8);
    icon(close,"close",13,13);lv_obj_add_flag(close,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close,cb,LV_EVENT_CLICKED,(void *)QUICK_DEVICE_CLOSE);
    const char *names[]={UI_N_("Controller"),UI_N_("Image"),UI_N_("Interface")};
    for(unsigned i=0;i<3;i++){
        copy(v->version_panel,ui_tr(names[i]),20,64+32*i,125,&lv_font_instrument_sans_medium_14,0x7C92A1);
        v->versions[i]=copy(v->version_panel,"",151,64+32*i,128,&lv_font_instrument_sans_semibold_14,0x253D4C);
        lv_obj_set_style_text_align(v->versions[i],LV_TEXT_ALIGN_RIGHT,0);
    }
    visible(v->version_panel,false);
    return true;
}
void lv_quick_controls_show_versions(lv_quick_controls_t *v,bool show)
{ visible(v->version_panel,show);if(show)lv_obj_move_foreground(v->version_panel); }
void lv_quick_controls_set_editing(lv_quick_controls_t *v,bool editing,int selected)
{
    v->editing=editing;v->selected=selected;
    for(unsigned i=0;i<QUICK_CONTROL_COUNT;i++){
        bool chosen=editing && selected==(int)i;
        visible(v->switches[i],!editing);visible(v->arrows[i],!editing);
        visible(v->states[i],!editing);visible(v->handles[i],editing);
        lv_obj_set_style_border_width(v->tiles[i],chosen?2:1,0);
        lv_obj_set_style_border_color(v->tiles[i],lv_color_hex(chosen?0x1674E8:0xDCE5EC),0);
        lv_obj_set_style_bg_color(v->tiles[i],lv_color_hex(chosen?0xEDF5FF:
            (!editing && i==QUICK_PURE && v->state.pure?0xF3F8FF:0xFFFFFF)),0);
        lv_obj_set_style_img_recolor(v->icons[i],lv_color_hex(!editing && action_enabled(&v->state,i)?0x1674E8:0x597C92),0);
        lv_obj_set_style_img_recolor_opa(v->icons[i],LV_OPA_COVER,0);
    }
}
bool lv_quick_controls_refresh(lv_quick_controls_t *v,lv_quick_controls_state_t state)
{
    bool changed=!v->initialized;
    if(!v->pager)return false;
    unsigned used=0;
    for(unsigned pos=0;pos<QUICK_CONTROL_COUNT;pos++){
        unsigned id=state.order[pos];
        if(id>=QUICK_CONTROL_COUNT || (used&(1U<<id)))return false;
        used|=1U<<id;
    }
    for(unsigned pos=0;pos<QUICK_CONTROL_COUNT;pos++){
        unsigned id=state.order[pos];
        if(!v->initialized||v->state.order[pos]!=id){
            unsigned slot=pos%QUICK_CONTROLS_PER_PAGE;
            lv_obj_set_parent(v->tiles[id],lv_content_pager_get_page(v->pager,pos/QUICK_CONTROLS_PER_PAGE));
            lv_obj_set_pos(v->tiles[id],(slot%4)*1242/4,(slot/4)*106);changed=true;
        }
    }
    for(unsigned i=0;i<QUICK_CONTROL_COUNT;i++){
        bool enabled=action_enabled(&state,i);
        if(v->initialized && action_enabled(&v->state,i)==enabled)continue;
        changed=true;
        if(is_switch(i)){
            lv_obj_set_style_bg_color(v->switches[i],lv_color_hex(enabled?0x1674E8:0xDCE4EB),0);
            lv_obj_set_x(v->knobs[i],enabled?24:4);
        } else if(i==QUICK_PURE)lv_label_set_text(v->states[i],ui_tr(state.pure?UI_N_("Exit"):UI_N_("Enter")));
    }
    for(unsigned i=0;i<3;i++){
        const char *value=state.versions[i]&&*state.versions[i]?state.versions[i]:ui_tr("Unavailable");
        if(strcmp(lv_label_get_text(v->versions[i]),value)){
            lv_label_set_text(v->versions[i],value);changed=true;
        }
    }
    v->state=state;v->initialized=true;
    if(changed)lv_quick_controls_set_editing(v,v->editing,v->selected);
    return changed;
}
bool lv_quick_controls_execute(quick_control_action_t action)
{
    switch(action){
        case QUICK_LAYOUT:page_01_main_layout_set_enabled(!page_01_main_layout_is_enabled());return true;
        case QUICK_GESTURES:return gesture_service_set_enabled(!gesture_service_enabled());
        case QUICK_PURE:return ui_page_18_pure_toggle_theme();
        case QUICK_POPUP:
            fault_popup_set_auto_enabled(!fault_popup_get_auto_enabled());
            ui_state_save_popup_auto_state();return true;
        case QUICK_TOP_POPUP:return ui_state_top_popup_set_enabled(!ui_notice_get_enabled());
        case QUICK_TOUCH:return touch_feedback_set_enabled(!touch_feedback_enabled());
        case QUICK_EXPORT:return ui_export_data_request();
        case QUICK_QR:smart_island_show_qr_popup();return true;
        case QUICK_STANDBY:return app_standby_runtime_request();
        default:return false;
    }
}
static bool contains(lv_obj_t *object,const lv_point_t *point)
{
    lv_area_t area;lv_obj_get_coords(object,&area);
    return point->x>=area.x1 && point->x<=area.x2 && point->y>=area.y1 && point->y<=area.y2;
}
static int tile_at(lv_quick_controls_t *v,const lv_point_t *point)
{
    if(!contains(v->viewport,point))return -1;
    lv_obj_t *page=lv_content_pager_get_page(v->pager,lv_quick_controls_page(v));
    for(unsigned i=0;i<QUICK_CONTROL_COUNT;i++)
        if(lv_obj_get_parent(v->tiles[i])==page && contains(v->tiles[i],point))return (int)i;
    return -1;
}
static void release_pressed(lv_quick_controls_t *v)
{
    if(v->pointer_tile>=0)lv_obj_clear_state(v->tiles[v->pointer_tile],LV_STATE_PRESSED);
}
uint8_t lv_quick_controls_page(const lv_quick_controls_t *v)
{return v->pager?lv_content_pager_get_active(v->pager):0;}
bool lv_quick_controls_reset(lv_quick_controls_t *v)
{
    if(!v->pager)return false;
    bool changed=v->pointer_down || lv_quick_controls_page(v)!=0 || lv_obj_get_scroll_x(v->viewport)!=0;
    if(v->pointer_down)release_pressed(v);
    v->pointer_down=v->pointer_swiping=v->pointer_drag=v->pointer_moved=v->pointer_vertical=false;
    lv_content_pager_set_active(v->pager,0,false);
    return changed;
}
bool lv_quick_controls_take_vertical(lv_quick_controls_t *v,lv_point_t *start,uint32_t *tick)
{
    if(!v->pointer_down || !v->pointer_vertical)return false;
    if(start)*start=v->pointer_start;
    if(tick)*tick=v->pointer_tick;
    release_pressed(v);v->pointer_down=v->pointer_vertical=false;
    return true;
}
bool lv_quick_controls_pointer(lv_quick_controls_t *v,lv_event_code_t event,const lv_point_t *point,uint8_t count)
{
    if(!v->pager || !point)return false;
    bool released=event==LV_EVENT_RELEASED || event==LV_EVENT_PRESS_LOST || count==0;
    if(!v->pointer_down){
        if(released || count!=1 || event!=LV_EVENT_PRESSED || !contains(v->viewport,point) ||
           !lv_obj_has_flag(v->version_panel,LV_OBJ_FLAG_HIDDEN))return false;
        v->pointer_down=true;v->pointer_swiping=v->pointer_drag=v->pointer_moved=v->pointer_vertical=false;
        v->pointer_start=*point;v->pointer_tick=lv_tick_get();v->pointer_page=lv_quick_controls_page(v);
        v->pointer_tile=tile_at(v,point);v->pointer_selected=v->selected;
        if(!v->editing && v->pointer_tile>=0)lv_obj_add_state(v->tiles[v->pointer_tile],LV_STATE_PRESSED);
        return true;
    }
    int dx=point->x-v->pointer_start.x,dy=point->y-v->pointer_start.y;
    if(event==LV_EVENT_PRESS_LOST || (!released && count!=1)){
        release_pressed(v);v->pointer_moved=true;v->pointer_swiping=v->pointer_drag=false;
        lv_content_pager_set_active(v->pager,v->pointer_page,false);
        if(event==LV_EVENT_PRESS_LOST)v->pointer_down=false;
        return true;
    }
    if(abs(dx)>8 || abs(dy)>8)v->pointer_moved=true;
    if(!released && !v->pointer_swiping && !v->pointer_drag && v->pointer_moved){
        /* Holding first deliberately selects a card drag. Ordinary horizontal
         * movement always pages, including while arranging shortcuts. */
        if(v->editing && v->pointer_tile>=0 && lv_tick_elaps(v->pointer_tick)>=350){
            v->pointer_drag=true;v->pointer_vertical=false;lv_quick_controls_set_editing(v,true,v->pointer_tile);
        } else if(abs(dx)>12 && abs(dx)>abs(dy))v->pointer_swiping=true;
        else if(abs(dy)>=10 && abs(dy)>abs(dx))v->pointer_vertical=true;
    }
    if(v->pointer_moved)release_pressed(v);
    if(v->pointer_swiping && !released){
        int x=v->pointer_page*1230-dx;
        int maximum=(lv_content_pager_get_count(v->pager)-1)*1230;
        if(x<0)x=0;
        if(x>maximum)x=maximum;
        lv_obj_scroll_to_x(v->viewport,x,LV_ANIM_OFF);
    }
    if(!released)return true;
    release_pressed(v);v->pointer_down=false;
    if(v->pointer_swiping){
        int page=v->pointer_page;
        if(abs(dx)>=80 || (abs(dx)>=40 && lv_tick_elaps(v->pointer_tick)<=350))page+=dx<0?1:-1;
        if(page<0)page=0;
        if(page>=lv_content_pager_get_count(v->pager))page=lv_content_pager_get_count(v->pager)-1;
        v->pointer_swiping=false;lv_content_pager_set_active(v->pager,page,false);
    } else if(v->pointer_drag){
        int target=tile_at(v,point);v->pointer_drag=false;
        if(target>=0 && target!=v->pointer_tile){
            v->drag_source=v->pointer_tile;v->drag_target=target;
            lv_event_send(v->viewport,LV_EVENT_VALUE_CHANGED,NULL);
        } else lv_quick_controls_set_editing(v,true,v->pointer_selected);
    } else if(!v->pointer_moved && tile_at(v,point)==v->pointer_tile && v->pointer_tile>=0){
        /* Host actions may delete this view, so nothing accesses it afterward. */
        lv_event_send(v->tiles[v->pointer_tile],LV_EVENT_CLICKED,NULL);
    }
    return true;
}
