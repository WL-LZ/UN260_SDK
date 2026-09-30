#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "lv_quick_controls.h"
#include "lv_settings.h"
#include "lv_fault_popup.h"
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

static const char *const titles[QUICK_CONTROL_COUNT]={
    UI_N_("Layout"),UI_N_("Gestures"),UI_N_("Pure count"),UI_N_("Top popup"),
    UI_N_("Touch guide"),UI_N_("Export"),UI_N_("QR code"),UI_N_("Standby")};
static const char *const icons[QUICK_CONTROL_COUNT]={
    LVGL_DIR "menu_icons/screen_20.png",LVGL_DIR "menu_icons/hand_22.png",
    LVGL_DIR "menu_icons/note_22.png",LVGL_DIR "menu_icons/warning_22.png",
    LVGL_DIR "menu_icons/help_19.png",LVGL_DIR "menu_icons/output_25.png",
    LVGL_DIR "menu_icons/qr_22.png",LVGL_DIR "menu_icons/power_22.png"};
static const uint32_t accents[QUICK_CONTROL_COUNT]={
    0x2763BD,0x7552AE,0x118572,0xBE672D,0x7552AE,0x2763BD,0x118572,0xBE672D};

static lv_obj_t *copy(lv_obj_t *p,const char *s,int x,int y,int w,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *l=lv_settings_label(p,s,x,y,font,color);lv_obj_set_width(l,w);
    lv_obj_set_style_text_font(l,ui_message_font(font),0);
    lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);return l;
}
const char *lv_quick_controls_title(quick_control_action_t action)
{return (unsigned)action<QUICK_CONTROL_COUNT?ui_tr(titles[action]):"";}
lv_quick_controls_state_t lv_quick_controls_current_state(void)
{
    lv_quick_controls_state_t s={
        .layout=page_01_main_layout_is_enabled(),.gestures=gesture_service_enabled(),
        .pure=smart_island_pure_count_is_enabled(),.popup=fault_popup_get_auto_enabled(),
        .touch=touch_feedback_enabled(),
        .versions={device_info_is_valid()?device_info_main_app():NULL,
            device_info_is_valid()?device_info_image_app():NULL,device_info_display_app()}};
    ui_state_quick_order_get(s.order);return s;
}
void lv_quick_controls_create(lv_quick_controls_t *v,lv_obj_t *parent,int x,int y,lv_event_cb_t cb)
{
    memset(v,0,sizeof(*v));
    copy(parent,ui_tr("SHORTCUTS"),x,y,320,&lv_font_instrument_sans_semibold_12,0x61798C);
    lv_obj_t *swipe=copy(parent,ui_tr("SWIPE FOR MORE"),x+674,y,156,&lv_font_instrument_sans_medium_12,0x78909E);
    lv_obj_set_style_text_align(swipe,LV_TEXT_ALIGN_RIGHT,0);
    v->viewport=lv_obj_create(parent);lv_obj_remove_style_all(v->viewport);
    lv_obj_set_pos(v->viewport,x,y+25);lv_obj_set_size(v->viewport,830,130);
    lv_obj_set_scroll_dir(v->viewport,LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(v->viewport,LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(v->viewport,LV_OPA_TRANSP,0);
    v->track=lv_obj_create(v->viewport);lv_obj_remove_style_all(v->track);
    lv_obj_set_pos(v->track,0,0);lv_obj_set_size(v->track,4*258,125);
    lv_obj_clear_flag(v->track,LV_OBJ_FLAG_SCROLLABLE);
    for(unsigned i=0;i<QUICK_CONTROL_COUNT;i++){
        lv_obj_t *tile=lv_obj_create(v->track);lv_obj_remove_style_all(tile);
        v->tiles[i]=tile;lv_obj_set_pos(tile,(i/2)*258,(i%2)*64);lv_obj_set_size(tile,246,58);
        lv_obj_set_style_bg_color(tile,lv_color_hex(0xFFFFFF),0);
        lv_obj_set_style_bg_opa(tile,LV_OPA_COVER,0);lv_obj_set_style_radius(tile,13,0);
        lv_obj_set_style_border_width(tile,1,0);
        lv_obj_set_style_border_color(tile,lv_color_hex(0xDAE5ED),0);
        lv_obj_set_style_bg_color(tile,lv_color_hex(0xE7F0F8),LV_STATE_PRESSED);
        lv_obj_add_flag(tile,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tile,cb,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
        lv_obj_t *accent=lv_settings_box(tile,0,12,3,34,accents[i]);
        lv_obj_set_style_radius(accent,2,0);lv_obj_clear_flag(accent,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *well=lv_settings_box(tile,11,10,38,38,0xEEF3F8);
        lv_obj_set_style_radius(well,11,0);lv_obj_clear_flag(well,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *icon=ui_icon_create(well,icons[i]);lv_obj_center(icon);
        lv_obj_clear_flag(icon,LV_OBJ_FLAG_CLICKABLE);
        copy(tile,lv_quick_controls_title(i),57,18,132,&lv_font_instrument_sans_medium_16,0x203441);
        v->states[i]=copy(tile,"",188,19,46,&lv_font_instrument_sans_medium_14,accents[i]);
        lv_obj_set_style_text_align(v->states[i],LV_TEXT_ALIGN_RIGHT,0);
    }
    lv_obj_t *device=lv_settings_box(parent,x+854,y,338,160,0x203B52);
    lv_obj_set_style_radius(device,17,0);
    lv_obj_t *mark=lv_settings_box(device,16,14,4,27,0x73B8E5);
    lv_obj_set_style_radius(mark,2,0);lv_obj_clear_flag(mark,LV_OBJ_FLAG_CLICKABLE);
    copy(device,"UN260",29,12,90,&lv_font_instrument_sans_semibold_18,0xFFFFFF);
    copy(device,ui_tr("DEVICE VERSIONS"),130,17,188,&lv_font_instrument_sans_medium_12,0xBFD2E0);
    lv_obj_t *rule=lv_settings_box(device,16,47,306,1,0x3D5A70);
    lv_obj_clear_flag(rule,LV_OBJ_FLAG_CLICKABLE);
    static const char *const names[]={UI_N_("Controller"),UI_N_("Image"),UI_N_("Interface")};
    for(unsigned i=0;i<3;i++){
        lv_obj_t *dot=lv_settings_box(device,19,64+31*i,6,6,i==0?0x76C6B6:i==1?0x8CB8EC:0xD8AD7D);
        lv_obj_set_style_radius(dot,3,0);lv_obj_clear_flag(dot,LV_OBJ_FLAG_CLICKABLE);
        copy(device,ui_tr(names[i]),33,57+31*i,155,&lv_font_instrument_sans_medium_14,0xBDD1DF);
        v->versions[i]=copy(device,"",196,54+31*i,122,&lv_font_instrument_sans_semibold_18,0xFFFFFF);
        lv_obj_set_style_text_align(v->versions[i],LV_TEXT_ALIGN_RIGHT,0);
        if(i<2){
            lv_obj_t *separator=lv_settings_box(device,18,81+31*i,300,1,0x35536A);
            lv_obj_clear_flag(separator,LV_OBJ_FLAG_CLICKABLE);
        }
    }
}
bool lv_quick_controls_refresh(lv_quick_controls_t *v,lv_quick_controls_state_t state)
{
    bool changed=!v->initialized;
    bool enabled[]={state.layout,state.gestures,state.pure,state.popup,state.touch};
    for(unsigned pos=0;pos<QUICK_CONTROL_COUNT;pos++){
        unsigned id=state.order[pos];if(id>=QUICK_CONTROL_COUNT)id=pos;
        if(!v->initialized||v->state.order[pos]!=id){
            lv_obj_set_pos(v->tiles[id],(pos/2)*258,(pos%2)*64);changed=true;
        }
    }
    for(unsigned i=0;i<QUICK_CONTROL_COUNT;i++){
        const char *value=i<5?ui_tr(enabled[i]?"On":"Off"):ui_tr("Open");
        if(strcmp(lv_label_get_text(v->states[i]),value)){
            lv_label_set_text(v->states[i],value);changed=true;
            lv_obj_set_style_text_color(v->states[i],lv_color_hex(i<5&&!enabled[i]?0x7E939E:accents[i]),0);
        }
    }
    for(unsigned i=0;i<3;i++){
        const char *value=state.versions[i]&&*state.versions[i]?state.versions[i]:ui_tr("Unavailable");
        if(strcmp(lv_label_get_text(v->versions[i]),value)){
            lv_label_set_text(v->versions[i],value);changed=true;
        }
    }
    v->state=state;v->initialized=true;return changed;
}
bool lv_quick_controls_execute(quick_control_action_t action)
{
    switch(action){
        case QUICK_LAYOUT:page_01_main_layout_set_enabled(!page_01_main_layout_is_enabled());return true;
        case QUICK_GESTURES:return gesture_service_set_enabled(!gesture_service_enabled());
        case QUICK_PURE:{
            bool in_pure=ui_manager_get_current_page()==UI_PAGE_PURE;
            bool enable=!in_pure && !smart_island_pure_count_is_enabled();
            smart_island_set_pure_count_enabled(enable);ui_state_save_pure_count_state();
            if(enable)ui_manager_push_page(UI_PAGE_PURE);
            else if(in_pure)ui_page_18_pure_request_exit();
            return true;
        }
        case QUICK_POPUP:
            fault_popup_set_auto_enabled(!fault_popup_get_auto_enabled());
            ui_state_save_popup_auto_state();return true;
        case QUICK_TOUCH:return touch_feedback_set_enabled(!touch_feedback_enabled());
        case QUICK_EXPORT:return ui_export_data_request();
        case QUICK_QR:smart_island_show_qr_popup();return true;
        case QUICK_STANDBY:
            if(app_command_runtime_count_start_busy()||standby_store_busy())return false;
            ui_manager_push_page(UI_PAGE_STANDBY);return true;
        default:return false;
    }
}
