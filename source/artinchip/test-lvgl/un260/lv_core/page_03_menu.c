#include "un260/app_service/count_mode_service.h"
#include "un260/app_service/app_local_setting.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/app_service/app_add_runtime.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/app_service/app_ui_runtime.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_03_menu.h"

#include "operator_initial.h"
#include "lv_page_manager.h"
#include "lv_page_event.h"
#include "settings_detail_ui.h"
#include "page_05_set_password.h"
#include "page_19_history.h"
#include "un260/app_service/workspace_service.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/app_service/app_standby_runtime.h"
#include "un260/storage/standby_store.h"
#include "un260/app_service/setting_service.h"
#include "un260/app_service/support_report.h"
#include "un260/storage/workspace_store.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_components/lv_quick_controls.h"
#include "un260/lv_components/lv_content_pager.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/lv_qr_popup.h"
#include "un260/lv_components/ui_scrollbar.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/lv_system/ui_qr_data.h"
#include "un260/lv_system/ui_state_runtime.h"
#include "un260/lv_system/app_clock.h"
#include "un260/currency/currency_state.h"
#include "un260/machine_state/machine_state.h"
#include "un260/gesture/gesture_service.h"
#include "un260/gesture/gesture_guide.h"
#include "un260/device_info/device_info.h"
#include "un260/lv_system/machine_time.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_multi.h"
#include "un260/counting/counting_history_service.h"
#include "un260/counting/counting_reject_reason.h"
#include "un260/print/print_config.h"
#include "lv_port_indev.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_resources/ui_icons.h"
#include "un260/font/ui_message_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Menu owns presentation; services retain settings and record decisions. */
#define ENTRY_ICON(name) LVGL_DIR "settings_icons/Entry" name ".png"
enum {INK=0x1D2B34,MUTED=0x586B78,LINE=0xDCE4E8,PANEL=0xF4F6F7,
      HEADER_SURFACE=0xE7EDF0,BLUE=0x1559B7,ORANGE=0xBD6624,TEAL=0x147C69,PURPLE=0x7953BD};
#define MICON(name,size) LVGL_DIR "menu_icons/" name "_" #size ".png"
#include "menu_fonts.inc"
enum {A_BACK=1,A_QUICK,A_SETTINGS,A_HISTORY,A_PRINT_SETUP,A_PRINT,A_QR,A_STANDBY,
      A_QUICK_TOGGLE,A_NEW_PROFILE,A_APPLY_PROFILE,
      A_DELETE_PROFILE,A_SAVE_BATCH,A_ADD_BATCH,A_EDIT_BATCH,A_DELETE_BATCH,A_APPLY_BATCH,
      A_NEW_USER,A_EDIT_NAME,A_USB_SCAN,A_USER_SAVE,A_USER_CANCEL,A_USER_SWITCH,A_USB_CLOSE,A_QUICK_CLOSE,
      A_AUTO_QR,A_CAPACITY,A_LANGUAGE,A_TIMEOUT,A_DIAGNOSTICS,A_EDIT_USER,A_EMPLOYEE_ID,A_TEAM,A_REMOVE_PHOTO,A_DELETE_USER,A_INTERACTION_CONTROLS,A_SHORTCUT_VISIBILITY,A_BATCH_BEHAVIOR,A_TOUCH_SOUND,A_QUICK_ORDER_BASE=900};
static const char *const tabs[]={NULL,UI_N_("Count"),NULL,UI_N_("Output"),UI_N_("Preferences"),UI_N_("Help")};
static const char *const tab_icons[]={MICON("overview",19),MICON("layers",19),MICON("history",19),MICON("output",19),MICON("settings",19),MICON("help",19)};
static const uint32_t accents[]={BLUE,ORANGE,TEAL,BLUE,PURPLE,TEAL};
/* Retain IDs of the remaining Menu sections. */
static const uint8_t visible_tabs[]={1,3,4,5};
static const char *const sublabels[][5]={{NULL},{UI_N_("Batch presets"),UI_N_("Profiles"),UI_N_("Options")},
    {NULL},{UI_N_("Print"),UI_N_("QR export")},
    {UI_N_("Operators"),UI_N_("Interaction"),UI_N_("Display & sound")},{UI_N_("Reject guide"),UI_N_("Care"),UI_N_("Device")}};
static const unsigned subcounts[]={0,3,0,2,3,3};
static struct {
    lv_obj_t *root,*body,*quick,*quick_context,*photo_sheet,*user_sheet,*nav[6],*task_scroll;
    lv_timer_t *timer;
    unsigned tab,sub,selected_batch,selected_profile,selected_user,batch_page;
    bool reject_all;
    bool dirty,batch_dirty,user_edit,scan_wait,photo_wait,photo_ready;
    unsigned saving;
    uint8_t batch_apply_target;
    uint32_t qr_generation;
    unsigned reject_code;
    lv_quick_controls_t quick_controls,interaction_controls;
    uint8_t interaction_page;
    bool interaction_created,shortcut_visibility;
    uint8_t batches[WORKSPACE_BATCHES],batch_count;
    uint8_t batch_active_original,batch_active_edited;
    uint32_t owner,editing_user;
    unsigned rendered_tab,rendered_sub;
    lv_coord_t scroll_y[6][5];
    char user_name[WORKSPACE_NAME+1],employee_id[WORKSPACE_EMPLOYEE_ID+1],team[WORKSPACE_TEAM+1];
    workspace_avatar_t avatar;
    lv_img_dsc_t avatar_desc,form_avatar_desc,usb_avatar_desc;
} menu;
static void action(lv_event_t *event);
static void render(void);
static void quick_tile(lv_event_t *event);
static bool menu_quick_pointer(lv_indev_t *,lv_event_code_t,const lv_point_t *,uint8_t);
static void photo_sheet(void);
static void clear_notice(void)
{ui_notice_dismiss("menu.operation");}
static void notify(ui_notice_kind_t kind,const char *message)
{
    if(!message||!*message||!page_03_menu_is_visible())return;
    ui_notice_post_text(kind,"menu.operation",UI_N_("Menu"),message);
}
static void explain(const char *message)
{notify(UI_NOTICE_WARNING,message);}
static void top_popup_save_failed(void)
{
    if(ui_notice_get_enabled())explain(UI_N_("Could not save. Try again."));
    else settings_detail_dialog_show(ui_tr("Top popup"),ui_tr("Could not save. Try again."),
        ui_tr("OK"),NULL,NULL,NULL,NULL);
}
static lv_obj_t *label(lv_obj_t *p,int x,int y,int w,const char *s,const lv_font_t *f,uint32_t c)
{lv_obj_t *o=lv_settings_label(p,s,x,y,f,c);lv_obj_set_width(o,w);lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);return o;}
static lv_obj_t *text(lv_obj_t *p,int x,int y,int w,const char *s)
{return label(p,x,y,w,s,&lv_font_instrument_sans_medium_17,INK);}
static lv_obj_t *small(lv_obj_t *p,int x,int y,int w,const char *s)
{return label(p,x,y,w,s,&lv_font_instrument_sans_menumedium_14,MUTED);}
static lv_obj_t *box(lv_obj_t *p,int x,int y,int w,int h,uint32_t c,int r)
{lv_obj_t *o=lv_settings_box(p,x,y,w,h,c);lv_obj_set_style_radius(o,r,0);return o;}
static void border(lv_obj_t *o,uint32_t c)
{lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,lv_color_hex(c),0);}
static void focus_style(lv_obj_t *o)
{
    lv_obj_set_style_outline_width(o,3,LV_STATE_FOCUS_KEY);lv_obj_set_style_outline_pad(o,2,LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_color(o,lv_color_hex(BLUE),LV_STATE_FOCUS_KEY);lv_obj_set_style_outline_opa(o,LV_OPA_COVER,LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(o,0,LV_STATE_DISABLED);
}
static lv_obj_t *button(lv_obj_t *p,int x,int y,int w,int h,const char *s,int id,bool primary)
{
    lv_obj_t *v=lv_settings_button(p,x,y,w,h,s,primary,action,(void *)(uintptr_t)id);
    focus_style(v);
    if(lv_obj_get_style_bg_color(p,0).full==lv_color_hex(BLUE).full)lv_obj_set_style_outline_color(v,lv_color_hex(0xFFFFFF),LV_STATE_FOCUS_KEY);
    uint32_t surface=lv_obj_get_style_bg_opa(p,0)==LV_OPA_COVER&&lv_obj_get_style_bg_color(p,0).full==lv_color_hex(PANEL).full?0xFFFFFF:HEADER_SURFACE;
    lv_damped_button_set_exact_palette(v,lv_color_hex(primary?BLUE:surface),lv_color_hex(primary?LV_SETTINGS_PRIMARY_PRESSED:LV_SETTINGS_CONTROL_PRESSED));
    lv_obj_set_style_border_width(v,0,0);
    lv_obj_set_style_text_font(lv_obj_get_child(v,0),ui_message_font(h<=36?&lv_font_instrument_sans_medium_15:&lv_font_instrument_sans_medium_17),0);
    lv_obj_set_style_radius(v,h<=36?9:12,0);
    lv_obj_set_style_border_width(v,0,LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(v,lv_color_hex(0xE8EBEE),LV_STATE_DISABLED);
    lv_obj_set_style_text_color(v,lv_color_hex(0x64717C),LV_STATE_DISABLED);
    return v;
}
/* Measure the label after LVGL has shaped its text, using its actual font. */
static int menu_label_width(lv_obj_t *label,const lv_font_t *font)
{
    lv_point_t size;
    lv_txt_get_size(&size,lv_label_get_text(label),font?font:lv_obj_get_style_text_font(label,0),
        lv_obj_get_style_text_letter_space(label,0),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    return size.x;
}
static int menu_button_fit_right(lv_obj_t *button,int right,int minimum,int padding)
{
    lv_obj_t *label=lv_obj_get_child(button,0);
    lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
    int width=LV_MAX(minimum,menu_label_width(label,NULL)+padding);
    lv_obj_set_width(button,width);lv_obj_set_x(button,right-width);
    lv_obj_set_width(label,width-padding);
    return width;
}
static void choose(lv_obj_t *b,bool selected)
{
    if(selected)lv_obj_add_state(b,LV_STATE_CHECKED);else lv_obj_clear_state(b,LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(b,lv_color_hex(0xFFFFFF),LV_STATE_CHECKED);
    lv_obj_set_style_border_width(b,0,0);
    lv_obj_set_style_border_width(b,0,LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(b,0,0);
    lv_obj_set_style_shadow_opa(b,LV_OPA_10,0);
}
/* Match Main's ABC tray; centre the icon and natural-width label as one unit. */
static int menu_text_width(const char *text)
{
    lv_point_t normal,bold;
    lv_txt_get_size(&normal,text,ui_message_font(&lv_font_instrument_sans_medium_16),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    lv_txt_get_size(&bold,text,ui_message_font(&lv_font_instrument_sans_semibold_16),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    return LV_MAX(normal.x,bold.x);
}
static void header_align_content(lv_obj_t *button)
{
    lv_obj_t *l=lv_obj_get_child(button,0),*im=lv_obj_get_child(button,1);
    lv_obj_update_layout(button);
    if(!*lv_label_get_text(l)){lv_obj_center(im);return;}
    int iw=lv_obj_get_width(im),tw=lv_obj_get_width(l);
    int start=(lv_obj_get_content_width(button)-iw-8-tw)/2;
    lv_obj_align(im,LV_ALIGN_LEFT_MID,start,0);
    lv_obj_align(l,LV_ALIGN_LEFT_MID,start+iw+8,0);
}
static void header_content(lv_obj_t *button,const char *title,const char *src,uint32_t color)
{
    lv_obj_t *l=lv_obj_get_child(button,0);
    lv_label_set_text(l,title);lv_obj_set_width(l,LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(l,ui_message_font(&lv_font_instrument_sans_medium_16),0);
    lv_obj_t *im=ui_icon_create(button,src);
    lv_obj_set_style_img_recolor(im,lv_color_hex(color),0);
    lv_obj_set_style_img_recolor_opa(im,LV_OPA_COVER,0);
    header_align_content(button);
}
static void header_surface(lv_obj_t *o)
{
    focus_style(o);
    lv_damped_button_set_exact_palette(o,lv_color_hex(PANEL),lv_damped_button_pressed_color(lv_color_hex(PANEL)));
    lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_radius(o,13,0);
}
static void heading(lv_obj_t *p,const char *s)
{label(p,0,2,650,s,&lv_font_instrument_sans_semibold_26,INK);}
static lv_obj_t *scroll(lv_obj_t *p,int x,int y,int w,int h)
{
    lv_obj_t *o=box(p,x,y,w,h,0xFFFFFF,0);lv_obj_set_style_bg_opa(o,LV_OPA_TRANSP,0);
    lv_obj_add_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_USER_4);lv_obj_set_scroll_dir(o,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(o,LV_SCROLLBAR_MODE_AUTO);return o;
}
static const workspace_user_t *user(void){return workspace_active(workspace_store_get());}
static workspace_model_t *draft(void)
{workspace_model_t *m=malloc(sizeof(*m));if(m)*m=*workspace_store_get();return m;}
static bool save(workspace_model_t *m)
{
    bool ok=m&&workspace_store_save(m);free(m);
    if(ok)app_ui_runtime_notice_started(APP_UI_NOTICE_WORKSPACE_STORE,UI_N_("Saving..."));else notify(UI_NOTICE_ERROR,UI_N_("Could not save. Wait for the current task or check device storage."));return ok;
}
static void capture_batch_active(void)
{
    menu.batch_active_original=menu.batch_active_edited=0;
    if(machine_state_batch_enabled())for(unsigned i=1;i<menu.batch_count;i++)
        if(menu.batches[i]==machine_state_batch_num()) {
            menu.batch_active_original=menu.batch_active_edited=menu.batches[i];break;
        }
}
static void reset_batch(void)
{const workspace_user_t *u=user();if(u){memcpy(menu.batches,u->batches,sizeof(menu.batches));menu.batch_count=u->batch_count;menu.owner=u->id;}menu.batch_dirty=false;menu.selected_batch=0;menu.batch_page=0;
    menu.batch_active_original=menu.batch_active_edited=0;
    if(u&&machine_state_batch_enabled())for(unsigned i=1;i<u->batch_count;i++)if(u->batches[i]==machine_state_batch_num()){menu.selected_batch=i;menu.batch_page=i/5;menu.batch_active_original=menu.batch_active_edited=u->batches[i];break;}}
static void tab_event(lv_event_t *e)
{
    if(menu.saving||menu.batch_dirty||menu.user_edit){explain(UI_N_("Save or cancel your changes before switching sections."));return;}
    if(workspace_service_applying()){notify(UI_NOTICE_WARNING,UI_N_("Wait for the profile to finish applying."));return;}
    clear_notice();
    menu.tab=(uintptr_t)lv_event_get_user_data(e);menu.sub=0;menu.dirty=true;
}
static void sub_event(lv_event_t *e)
{
    if(workspace_service_applying()){notify(UI_NOTICE_WARNING,UI_N_("Wait for the profile to finish applying."));return;}
    if(menu.saving||menu.batch_dirty||menu.user_edit){explain(UI_N_("Save or cancel your changes first."));return;}
    clear_notice();
    unsigned sub=(uintptr_t)lv_event_get_user_data(e);
    menu.sub=sub;menu.dirty=true;
}
static void pick_event(lv_event_t *e)
{
    unsigned n=(uintptr_t)lv_event_get_user_data(e);
    if(menu.tab==1&&menu.sub==0)menu.selected_batch=n;
    else if(menu.tab==1)menu.selected_profile=n;else menu.selected_user=n;
    menu.dirty=true;
}
static void avatar(lv_obj_t *p,int x,int y,const workspace_avatar_t *a,const char *name,lv_img_dsc_t *d)
{
    if(a&&a->present){*d=(lv_img_dsc_t){.header={.cf=LV_IMG_CF_TRUE_COLOR,.w=64,.h=64},.data_size=WORKSPACE_AVATAR_BYTES,.data=a->pixels};
        lv_obj_t *frame=box(p,x,y,64,64,PANEL,18);
        lv_obj_set_style_clip_corner(frame,true,0);
        lv_obj_t *im=lv_img_create(frame);lv_img_set_src(im,d);}
    else {lv_obj_t *v=box(p,x,y,64,64,PANEL,18);char initial[5]="U";
        if(name&&*name){unsigned n=_lv_txt_encoded_size(name);if(n&&n<sizeof(initial)){memcpy(initial,name,n);initial[n]=0;}}
        lv_obj_t *l=label(v,0,10,64,initial,&lv_font_instrument_sans_semibold_32,PURPLE);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);}
}
static void toggle(lv_obj_t *p,int y,int w,const char *title,const char *hint,bool on,int id)
{
    lv_obj_t *state=small(p,0,y+12,42,on?ui_tr("On"):ui_tr("Off"));
    int state_width=LV_MAX(42,menu_label_width(state,NULL));
    int state_x=w-64-state_width;
    lv_obj_set_width(state,state_width);lv_obj_set_x(state,state_x);
    lv_label_set_long_mode(state,LV_LABEL_LONG_CLIP);
    lv_obj_t *caption=text(p,0,y,LV_MIN(w-144,state_x-12),title);
    lv_obj_t *description=hint?small(p,0,y,LV_MIN(w-135,state_x-12),hint):NULL;
    lv_obj_set_style_text_color(state,lv_color_hex(on?BLUE:MUTED),0);
    lv_obj_t *v=lv_settings_toggle(p,w-52,y+(hint?15:6),on,action,(void *)(uintptr_t)id);
    lv_obj_set_width(v,52);lv_anim_del(v,NULL);
    lv_obj_set_style_bg_color(v,lv_color_hex(on?BLUE:0xB8C7D0),0);
    lv_obj_t *knob=lv_obj_get_child(v,0);lv_anim_del(knob,NULL);lv_obj_set_x(knob,on?25:3);
    lv_settings_align_row(caption,description,v,y,hint?60:44,4);
    lv_obj_update_layout(v);
    lv_obj_align_to(state,v,LV_ALIGN_OUT_LEFT_MID,-12,0);
}
#include "menu_composition.inc"

static void profile_summary(lv_obj_t *p,const workspace_profile_t *v,int y)
{
    const char *speed[]={ui_tr("Low"),ui_tr("Standard"),ui_tr("High")},*sort[]={ui_tr("Off"),ui_tr("Face"),ui_tr("Orientation"),ui_tr("Both")};
    char batch[64];snprintf(batch,sizeof(batch),v->batch_enabled?ui_trn("%u notes", (v->batch)):ui_tr("Off"),v->batch);
    const char *titles[]={ui_tr("Speed"),ui_tr("Start"),ui_tr("Counting"),ui_tr("Sorting"),ui_tr("ADD"),ui_tr("Batch")};
    const char *values[]={speed[v->speed],v->work?ui_tr("Manual"):ui_tr("Auto"),v->mode==1?ui_text_get(UI_TEXT_PAGE01_BOTTOM_MODE_MDC):v->mode==2?ui_text_get(UI_TEXT_PAGE01_BOTTOM_MODE_SDC):v->mode==3?ui_text_get(UI_TEXT_PAGE01_BOTTOM_MODE_CNT):ui_tr("Keep current"),sort[v->sort],v->add?ui_tr("On"):ui_tr("Off"),batch};
    for(unsigned i=0;i<6;i++){
        int x=20+(i%3)*132,top=y+(i/3)*51;
        small(p,x,top,125,titles[i]);text(p,x,top+20,125,values[i]);
    }
    small(p,20,y+105,125,ui_tr("Sound"));
    text(p,152,y+101,250,ui_tr(v->beep?UI_N_("On"):UI_N_("Off")));
}
static workspace_profile_t current_profile(void)
{
    return (workspace_profile_t){.mode=machine_state_mode()==MODE_SDC?2:machine_state_mode()==MODE_CNT?3:1,.speed=machine_state_speed(),.work=machine_state_work_mode(),.add=machine_state_add_enabled(),.sort=machine_state_fo_mode(),.beep=machine_state_buzzer_enabled(),.batch_enabled=machine_state_batch_enabled(),.batch=machine_state_batch_num()};
}
#include "menu_studio_views.inc"
#include "menu_output_help.inc"
static const char *const side_icons[][5]={
    {NULL},{"layers","profiles","options"},
    {"history","search","receipt"},
    {"print","qr","receipt"},{"user","hand","screen"},
    {"reject","brush","screen","hand"}
};
static void render(void)
{
    if(!menu.root)return;
    if(menu.tab!=menu.rendered_tab||menu.sub!=menu.rendered_sub)clear_notice();
    if(menu.task_scroll)menu.scroll_y[menu.rendered_tab][menu.rendered_sub]=lv_obj_get_scroll_y(menu.task_scroll);
    menu.task_scroll=NULL;
    if(menu.user_sheet){lv_obj_del(menu.user_sheet);menu.user_sheet=NULL;}
    if(menu.interaction_created){
        menu.interaction_page=lv_quick_controls_page(&menu.interaction_controls);
        lv_quick_controls_reset(&menu.interaction_controls);
    }
    menu.interaction_created=false;
    gesture_service_clear_pointer_policy(UI_PAGE_MENU);
    lv_obj_clean(menu.body);memset(&menu.interaction_controls,0,sizeof(menu.interaction_controls));lv_obj_t *b=menu.body;
    {
        lv_obj_t *side=box(b,0,0,180,296,0xFFFFFF,0);
        for(unsigned i=0;i<subcounts[menu.tab];i++) {
            unsigned sub=i;
            bool selected=menu.sub==sub;
            lv_obj_t *v=lv_settings_button(side,0,2+48*i,180,44,"",false,sub_event,(void *)(uintptr_t)sub);
            lv_damped_button_set_exact_palette(v,lv_color_hex(selected?HEADER_SURFACE:0xFFFFFF),lv_color_hex(0xDCE4E8));
            lv_obj_set_style_border_width(v,0,0);lv_obj_set_style_radius(v,12,0);focus_style(v);
            char path[128];snprintf(path,sizeof(path),LVGL_DIR "menu_icons/%s_20.png",side_icons[menu.tab][i]);
            icon(v,path,12,12,selected?BLUE:MUTED);
            label(v,42,12,132,ui_tr(sublabels[menu.tab][i]),selected?&lv_font_instrument_sans_semibold_16:&lv_font_instrument_sans_medium_16,selected?BLUE:MUTED);
            if(selected)box(v,0,13,3,18,BLUE,2);
        }
        lv_obj_t *content=box(b,204,0,1028,296,0xFFFFFF,0);
        if(menu.tab==1){if(!menu.sub)batch_view(content);else if(menu.sub==1)profiles_view(content);else options_view(content);}
        if(menu.tab==3)output_view(content);
        if(menu.tab==4){if(!menu.sub){operators_summary_view(content);}else if(menu.sub==1)interaction_view(content);else display_view(content);}
        if(menu.tab==5)help_view(content);
    }
    for(unsigned i=0;i<6;i++){if(!menu.nav[i])continue;choose(menu.nav[i],i==menu.tab);lv_obj_t *l=lv_obj_get_child(menu.nav[i],0);lv_obj_set_style_text_color(l,lv_color_hex(i==menu.tab?BLUE:0x455D6B),0);lv_obj_set_style_text_font(l,ui_message_font(i==menu.tab?&lv_font_instrument_sans_semibold_16:&lv_font_instrument_sans_medium_16),0);header_align_content(menu.nav[i]);}
    if(menu.task_scroll){lv_obj_update_layout(menu.task_scroll);lv_obj_scroll_to_y(menu.task_scroll,menu.scroll_y[menu.tab][menu.sub],LV_ANIM_OFF);}
    menu.rendered_tab=menu.tab;menu.rendered_sub=menu.sub;menu.dirty=false;
}
static void confirmed_back(void *unused)
{(void)unused;menu.batch_dirty=menu.user_edit=false;workspace_service_cancel_apply();reset_batch();if(!ui_manager_pop_page())ui_manager_switch(UI_PAGE_MAIN);}
static void delete_profile(void *unused)
{
    (void)unused;const workspace_user_t *u=user();if(!u||menu.selected_profile>=u->profile_count)return;workspace_model_t *m=draft();if(!m)return;
    if(!workspace_delete_profile(m,u->id,u->profiles[menu.selected_profile].id)){free(m);explain(UI_N_("Keep at least one counting profile."));return;}save(m);
}
static void apply_profile(void *unused)
{
    (void)unused;const workspace_user_t *u=user();if(!u||menu.selected_profile>=u->profile_count)return;
    const workspace_profile_t *profile=&u->profiles[menu.selected_profile];
    if(profile->batch_enabled&&profile->batch==200){explain(UI_N_("Batch 200 means OFF. Save a replacement profile with 1-199."));return;}
    if(!workspace_service_apply(&u->profiles[menu.selected_profile],app_clock_uptime_ms()))explain(UI_N_("Finish counting, then apply the profile again."));
    else app_ui_runtime_notice_started(APP_UI_NOTICE_PROFILE_APPLY,UI_N_("Applying profile..."));
}
static void name_submit(const char *value,void *context)
{
    if((uintptr_t)context<2&&!workspace_name_valid(value)){explain(UI_N_("Use 1-24 characters, with no leading or trailing spaces."));return;}
    if((uintptr_t)context==1){workspace_model_t *m=draft();if(!m)return;workspace_profile_t p=current_profile();snprintf(p.name,sizeof(p.name),"%s",value);
        if(!workspace_add_profile(m,m->operators_enabled?m->active_id:0,&p)){free(m);explain(UI_N_("Use a unique profile name. Up to 8 profiles are supported."));return;}save(m);}
    else{if((uintptr_t)context==2)snprintf(menu.employee_id,sizeof(menu.employee_id),"%s",value);else if((uintptr_t)context==3)snprintf(menu.team,sizeof(menu.team),"%s",value);else snprintf(menu.user_name,sizeof(menu.user_name),"%s",value);menu.dirty=true;}
}
static const workspace_user_t *operator_switch_target(const workspace_model_t *model,uint32_t id)
{
    if(!model||!model->operators_enabled)return NULL;
    for(unsigned i=0;i<model->user_count;i++)
        if(model->users[i].id==id&&!model->disabled[i])return &model->users[i];
    return NULL;
}
static void confirm_operator_switch(void *context)
{
    if(!page_03_menu_is_visible())return;
    uint32_t id=(uint32_t)(uintptr_t)context;
    const workspace_model_t *model=workspace_store_get();
    if(!operator_switch_target(model,id)){explain(UI_N_("The operator could not be switched."));return;}
    if(model->active_id==id)return;
    const char *reason=workspace_service_switch_blocker();if(reason){explain(reason);return;}
    if(!workspace_service_switch(id))notify(UI_NOTICE_ERROR,UI_N_("The operator could not be switched."));
    else app_ui_runtime_notice_started(APP_UI_NOTICE_OPERATOR_SWITCH,UI_N_("Switching workspace..."));
}
static void batch_submit(const char *value,void *unused)
{
    (void)unused;char *end;unsigned long v=strtoul(value,&end,10);
    if(!*value||*end||v<1||v>=200){explain(UI_N_("Enter 1-199, within the pocket capacity. 200 means OFF."));return;}
    for(unsigned i=1;i<menu.batch_count;i++)if(i!=menu.selected_batch&&menu.batches[i]==v){explain(UI_N_("This slot already exists."));return;}
    if(!menu.batch_dirty)capture_batch_active();
    if(menu.batch_active_edited&&menu.batches[menu.selected_batch]==menu.batch_active_edited)
        menu.batch_active_edited=v;
    menu.batches[menu.selected_batch]=v;menu.batch_dirty=menu.dirty=true;
    if(app_command_runtime_count_start_busy()||workspace_service_applying()){
        notify(UI_NOTICE_WARNING,UI_N_("Finish the current operation, then use this preset."));return;
    }
    /* Persist first; the storage completion callback sends the controller command. */
    if(workspace_service_save_batches(menu.owner,menu.batches,menu.batch_count,0,0)){
        menu.batch_apply_target=(uint8_t)v;menu.saving=4;
        app_ui_runtime_notice_started(APP_UI_NOTICE_BATCH_APPLY,UI_N_("Saving..."));
    }else notify(UI_NOTICE_ERROR,UI_N_("Save failed. Finish the current task and check storage."));
}
static void photo_event(lv_event_t *e)
{if(!workspace_store_import_avatar((uintptr_t)lv_event_get_user_data(e)))explain(UI_N_("Wait for the current photo task to finish."));else {menu.photo_wait=true;app_ui_runtime_notice_started(APP_UI_NOTICE_WORKSPACE_STORE,UI_N_("Preparing photo..."));}}
static void close_photo(lv_event_t *e)
{
    if(lv_event_get_target(e)!=menu.photo_sheet)return;
    lv_obj_del(menu.photo_sheet);menu.photo_sheet=NULL;
    menu.scan_wait=menu.photo_wait=menu.photo_ready=false;
}
static void photo_sheet(void)
{
    if(menu.photo_sheet)lv_obj_del(menu.photo_sheet);
    menu.photo_sheet=box(menu.root,0,0,1280,400,0x253442,0);lv_obj_set_style_bg_opa(menu.photo_sheet,LV_OPA_30,0);lv_obj_add_flag(menu.photo_sheet,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(menu.photo_sheet,close_photo,LV_EVENT_CLICKED,NULL);
    lv_obj_t *sheet=box(menu.photo_sheet,218,20,1040,360,0xFFFFFF,18);border(sheet,LINE);lv_obj_add_flag(sheet,LV_OBJ_FLAG_CLICKABLE);
    label(sheet,24,19,720,ui_tr("USB photos"),&lv_font_instrument_sans_semibold_24,INK);lv_obj_t *close=button(sheet,904,14,108,42,ui_tr("Close"),A_USB_CLOSE,false);lv_nav_button_mark_back(close);
    lv_obj_t *list=scroll(sheet,24,78,665,245);unsigned row=0;uint32_t available=workspace_store_usb_images();
    for(unsigned i=0;i<=20;i++)if(available&(1U<<i)){
        char name[48];if(i)snprintf(name,sizeof(name),"un260_avatar_%02u.png",i);else strcpy(name,"avatar.png");
        lv_settings_button(list,0,row++*50,638,42,name,false,photo_event,(void *)(uintptr_t)i);
    }
    if(!row)small(list,0,10,638,ui_tr("No supported photo found.\nUse avatar.png or un260_avatar_01.png (01-20)."));
    box(sheet,714,78,1,245,LINE,0);avatar(sheet,815,93,workspace_store_avatar(),menu.user_name,&menu.usb_avatar_desc);
    small(sheet,747,181,257,ui_tr("PNG only. Import previews a photo; creating the operator saves it."));button(sheet,747,268,265,42,ui_tr("Use this photo"),602,true);
}
static void destroy_quick(void)
{
    gesture_service_clear_pointer_policy(UI_PAGE_MENU);
    gesture_service_set_navigation_blocked(UI_PAGE_MENU,false);
    if(menu.quick)lv_obj_del(menu.quick);
    menu.quick=menu.quick_context=NULL;
    if(menu.interaction_created && page_03_menu_is_visible())
        gesture_service_set_pointer_policy(UI_PAGE_MENU,menu_quick_pointer);
}
static bool menu_quick_pointer(lv_indev_t *indev,lv_event_code_t event,const lv_point_t *point,uint8_t count)
{
    (void)indev;
    lv_quick_controls_t *controls=menu.quick?&menu.quick_controls:
        menu.interaction_created && menu.tab==4 && menu.sub==1 && !menu.shortcut_visibility?&menu.interaction_controls:NULL;
    if(!controls || !point || !page_03_menu_is_visible())return false;
    if(settings_detail_overlay_is_open() || menu.photo_sheet){
        bool owned=controls->pointer_down;
        if(owned)lv_quick_controls_pointer(controls,LV_EVENT_PRESS_LOST,point,0);
        return owned;
    }
    lv_obj_t *layers[]={lv_layer_top(),lv_layer_sys()};
    for(unsigned i=0;i<2;i++){
        lv_obj_t *hit=lv_indev_search_obj(layers[i],(lv_point_t *)point);
        if(hit && hit!=layers[i]){
            bool owned=controls->pointer_down;
            if(owned)lv_quick_controls_pointer(controls,LV_EVENT_PRESS_LOST,point,0);
            return owned;
        }
    }
    return lv_quick_controls_pointer(controls,event,point,count);
}
static void close_quick(lv_event_t *e)
{if(lv_event_get_target(e)==menu.quick)destroy_quick();}
static void refresh_quick(void)
{
    if(!menu.quick)return;
    lv_quick_controls_refresh(&menu.quick_controls,lv_quick_controls_current_state());
    char code[4],value[128];currency_state_get_effective_code(code);
    const counting_sim_t *data=counting_data_current();
    char quantity[64];unsigned notes=data?data->total_pcs:0;
    snprintf(quantity,sizeof(quantity),ui_trn("%u notes",notes),notes);
    snprintf(value,sizeof(value),"%s   |   %s",currency_state_display_code(code),quantity);
    if(strcmp(lv_label_get_text(menu.quick_context),value))
        lv_label_set_text(menu.quick_context,value);
}
static void quick_tile(lv_event_t *event)
{
    unsigned id=(uintptr_t)lv_event_get_user_data(event);
    lv_quick_controls_t *controls=menu.quick?&menu.quick_controls:
        menu.interaction_created?&menu.interaction_controls:NULL;
    if(!controls)return;
    bool leaves_or_operates=id==QUICK_PURE || id==QUICK_EXPORT || id==QUICK_QR || id==QUICK_STANDBY ||
        (id>=QUICK_CALCULATOR && id<QUICK_CONTROL_COUNT);
    if((menu.batch_dirty || menu.user_edit) && leaves_or_operates){
        explain(UI_N_("Save or cancel your changes before leaving this task."));return;
    }
    if(workspace_service_applying() || menu.saving){
        explain(UI_N_("The workspace is being saved. Please wait a moment."));return;
    }
    if(id==QUICK_DEVICE_INFO || id==QUICK_DEVICE_CLOSE){
        bool show=id==QUICK_DEVICE_INFO && lv_obj_has_flag(controls->version_panel,LV_OBJ_FLAG_HIDDEN);
        lv_quick_controls_show_versions(controls,show);return;
    }
    if(id==QUICK_TOP_POPUP){
        if(!lv_quick_controls_execute(id) && !ui_notice_get_enabled())top_popup_save_failed();
        refresh_quick();menu.dirty=true;return;
    }
    if(id>=QUICK_CONTROL_COUNT)return;
    bool review=id==QUICK_STANDBY && app_standby_runtime_needs_fault_review();
    if(id==QUICK_STANDBY){
        const char *reason=app_standby_runtime_blocker();
        if(reason && !review){explain(reason);return;}
    }
    if(id==QUICK_STANDBY||id==QUICK_PURE||id==QUICK_EXPORT||id==QUICK_QR||id>=QUICK_CALCULATOR){
        destroy_quick();
    }
    if(!lv_quick_controls_execute(id) && !review){
        const char *reason=id==QUICK_STANDBY?app_standby_runtime_blocker():NULL;
        if(id!=QUICK_LAYOUT && id!=QUICK_GESTURES && id!=QUICK_POPUP && id!=QUICK_TOUCH)
            explain(reason?reason:UI_N_("Quick action is unavailable right now."));
    }
    else refresh_quick();
    menu.dirty=true;
}
static lv_obj_t *quick_label(lv_obj_t *parent,int x,int y,int width,const char *value,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *o=label(parent,x,y,width,value,ui_message_font(font),color);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);return o;
}
static void open_quick(void)
{
    if(menu.quick)return;
    clear_notice();
    menu.quick=box(menu.root,0,0,1280,400,INK,0);lv_obj_set_style_bg_opa(menu.quick,LV_OPA_TRANSP,0);lv_obj_add_flag(menu.quick,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(menu.quick,close_quick,LV_EVENT_CLICKED,NULL);
    lv_obj_t *sheet=box(menu.quick,0,0,1280,328,0xF0F4F7,22);lv_obj_add_flag(sheet,LV_OBJ_FLAG_CLICKABLE);
    box(sheet,0,0,1280,22,0xF0F4F7,0);
    lv_obj_t *settings=ui_icon_create(sheet,LVGL_DIR "quick_icons/settings.png");lv_obj_set_pos(settings,27,22);
    lv_obj_t *title=quick_label(sheet,63,17,LV_SIZE_CONTENT,ui_tr("Quick controls"),&lv_font_instrument_sans_semibold_25,0x253D4C);
    lv_obj_update_layout(title);
    int divider_x=63+lv_obj_get_width(title)+12;
    box(sheet,divider_x,25,1,16,0xCDDBE4,0);
    quick_label(sheet,divider_x+19,25,LV_SIZE_CONTENT,ui_tr("Menu"),&lv_font_instrument_sans_medium_13,0x718B9B);
    if(!lv_quick_controls_create(&menu.quick_controls,sheet,25,65,quick_tile)){
        destroy_quick();explain(UI_N_("Quick action is unavailable right now."));return;
    }
    gesture_service_set_pointer_policy(UI_PAGE_MENU,menu_quick_pointer);
    gesture_service_set_navigation_blocked(UI_PAGE_MENU,true);
    lv_obj_t *close=box(sheet,1143,11,112,44,0xE4ECF2,10);
    lv_obj_add_flag(close,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(close,lv_color_hex(0xDBE6EF),LV_STATE_PRESSED);
    quick_label(close,40,12,64,ui_tr("Close"),&lv_font_instrument_sans_medium_14,0x253D4C);
    lv_obj_t *up=ui_icon_create(close,LVGL_DIR "quick_icons/up.png");lv_obj_set_pos(up,15,14);
    lv_obj_add_event_cb(close,action,LV_EVENT_CLICKED,(void *)A_QUICK_CLOSE);
    lv_nav_button_mark_back(close);focus_style(close);
    menu.quick_context=quick_label(sheet,1020,282,235,"",&lv_font_instrument_sans_medium_13,0x7B919F);
    lv_obj_set_style_text_align(menu.quick_context,LV_TEXT_ALIGN_RIGHT,0);
    box(sheet,614,314,54,4,0xBDCEDB,2);
    lv_obj_t *outline=box(sheet,0,0,1280,328,0xF0F4F7,22);
    lv_obj_set_style_bg_opa(outline,LV_OPA_TRANSP,0);border(outline,0xE1E8EE);
    refresh_quick();
}

static void mode_confirm(void *context)
{
    /* Re-selecting the confirmed mode must not schedule a destructive result clear. */
    if(!count_mode_service_request((uintptr_t)context))notify(UI_NOTICE_WARNING,UI_N_("Mode not sent. Finish counting and try again."));
    else clear_notice();
}
static void capacity_done(const char *value,void *unused)
{
    (void)unused;char *end;long v=strtol(value,&end,10);
    if(!*value||*end||v<REJECT_POCKET_MIN_CAPACITY||v>REJECT_POCKET_MAX_CAPACITY){explain(UI_N_("Enter a capacity from 30 to 100 notes."));return;}
    if(app_add_runtime_busy()||app_command_runtime_count_start_busy()||!setting_service_request_reject_pocket_max(v,machine_state_reject_pocket_max()))notify(UI_NOTICE_WARNING,UI_N_("Capacity not sent. Finish the current operation and retry."));
    else clear_notice();
}
static void timeout_done(const char *value,void *unused)
{
    (void)unused;char *end;long v=strtol(value,&end,10);
    if(!*value||*end||v<0||v>60){explain(UI_N_("Enter 0 to disable automatic standby, or 1-60 minutes."));return;}
    standby_config_t config=*standby_config();config.minutes=v;
    if(!standby_store_save(&config))explain(UI_N_("Standby settings could not be saved. Try again shortly."));
    else ui_notice_post_text(UI_NOTICE_PROGRESS,"settings.standby",UI_N_("Standby settings"),UI_N_("Saving..."));
}

static void action(lv_event_t *event)
{
    unsigned id=(uintptr_t)lv_event_get_user_data(event);const workspace_user_t *u=user();
    if(id==A_BATCH_BEHAVIOR){
        ui_main_batch_action_t next=ui_state_main_batch_action_get()==UI_MAIN_BATCH_CYCLE?
            UI_MAIN_BATCH_SETTINGS:UI_MAIN_BATCH_CYCLE;
        if(!app_local_setting_apply(APP_LOCAL_MAIN_BATCH_ACTION,next,0) && !ui_notice_get_enabled())
            settings_detail_dialog_show(ui_tr("Main Batch button"),ui_tr("Could not save. Try again."),
                ui_tr("OK"),NULL,NULL,NULL,NULL);
        menu.dirty=true;return;
    }
    if(id==A_BACK){if(menu.saving){explain(UI_N_("The workspace is being saved. Please wait a moment."));return;}if(menu.batch_dirty||menu.user_edit||workspace_service_applying())settings_detail_dialog_show(ui_tr("Leave Menu?"),ui_tr("Unsaved edits will be discarded. Remaining profile steps will stop."),ui_tr("Leave"),ui_tr("Stay"),confirmed_back,NULL,NULL);else confirmed_back(NULL);return;}
    bool independent=id==A_QUICK_CLOSE||id==A_USB_CLOSE||id==813||(id>=440&&id<450)||(id>=301&&id<=308);
    if(!independent&&workspace_service_applying()){explain(UI_N_("Wait for the profile, or use Back to stop remaining steps."));return;}
    if(!independent&&menu.saving){explain(UI_N_("The workspace is being saved. Please wait a moment."));return;}
    if((menu.batch_dirty||menu.user_edit)&&(id==A_SETTINGS||id==A_LANGUAGE||id==A_QUICK||id==A_HISTORY||id==A_PRINT_SETUP||id==601||id==600||id==A_STANDBY)){
        explain(UI_N_("Save or cancel your changes before leaving this task."));return;
    }
    if(id==A_SETTINGS||id==A_LANGUAGE){ui_page_05_set_password_open();return;}if(id==A_QUICK){open_quick();return;}
    if(id==A_QUICK_CLOSE){destroy_quick();return;}
    if(id==A_INTERACTION_CONTROLS || id==A_SHORTCUT_VISIBILITY){
        menu.shortcut_visibility=id==A_SHORTCUT_VISIBILITY;menu.dirty=true;return;
    }
    if(id>=A_QUICK_ORDER_BASE&&id<A_QUICK_ORDER_BASE+QUICK_CONTROL_COUNT){
        uint8_t shortcut=id-A_QUICK_ORDER_BASE;
        bool shown=(ui_state_quick_visible_get()&(1U<<shortcut))!=0;
        if(!app_local_setting_apply(APP_LOCAL_SHORTCUT_VISIBILITY,shortcut,!shown) && !ui_notice_get_enabled())
            settings_detail_dialog_show(ui_tr("Pull-down shortcuts"),ui_tr("Could not save. Try again."),
                ui_tr("OK"),NULL,NULL,NULL,NULL);
        menu.dirty=true;return;
    }
    if(id==A_USB_CLOSE){if(menu.photo_sheet)lv_obj_del(menu.photo_sheet);menu.photo_sheet=NULL;menu.scan_wait=menu.photo_wait=menu.photo_ready=false;return;}
    if(id==1902){ui_manager_push_page(UI_PAGE_OPERATORS);return;}
    if(id==1900){ui_manager_push_page(UI_PAGE_CALCULATOR);return;}
    if(id==A_HISTORY){ui_page_19_history_open_all();return;}if(id==A_PRINT_SETUP){ui_manager_push_page(UI_PAGE_PRINT_SETTING);return;}
    if(id==A_PRINT){if(app_command_runtime_count_start_busy()||print_config_pending()){explain(UI_N_("Finish counting and receipt settings before printing."));return;}page_01_print_btn_event_cb(event);return;}
    if(id==A_QR){char payload[3072];if(app_command_runtime_count_start_busy()||!ui_qr_data_build_summary(payload,sizeof(payload))||!lv_qr_popup_show(payload))explain(UI_N_("Finish counting and wait for the complete summary."));else menu.qr_generation=lv_qr_popup_generation();return;}
    if(id==601){ui_manager_push_page(UI_PAGE_STANDBY_SETTING);return;}if(id==600){ui_manager_push_page(UI_PAGE_LIST);return;}
    if(id==A_STANDBY){const char*reason=app_standby_runtime_blocker();if(reason&&!app_standby_runtime_needs_fault_review())explain(reason);else (void)app_standby_runtime_request();return;}
    if(id==307||id==308){menu.tab=id==307?3:5;menu.sub=0;menu.dirty=true;return;}
    if(id==510||id==511){if(id==510&&menu.batch_page)menu.batch_page--;if(id==511&&(menu.batch_page+1)*5<menu.batch_count)menu.batch_page++;menu.dirty=true;return;}
    if(id>=301&&id<=305){
        menu.tab=id<=303?1:4;
        menu.sub=id==302?1:id==303?2:id==304?1:0;menu.dirty=true;return;
    }
    if(id==A_TOUCH_SOUND){app_local_setting_apply(APP_LOCAL_TOUCH_SOUND,!user_cfg_touch_sound_enabled(),0);menu.dirty=true;return;}
    if(id==813)id=440+!machine_state_buzzer_enabled();
    if(id>=400&&id<450){if(id<440&&(workspace_service_applying()||app_command_runtime_count_start_busy()||
            (app_add_runtime_busy()&&(id<420||id>=430)))){notify(UI_NOTICE_WARNING,UI_N_("Finish the current operation before changing settings."));return;}unsigned kind=(id-400)/10,v=(id-400)%10;bool ok=false;
        switch(kind){
            case 0:ok=setting_service_request_speed(v);break;
            case 1:ok=setting_service_request_work_mode(v);break;
            case 2:ok=app_add_runtime_request(v);break;
            case 3:ok=setting_service_request_fo_mode(v);break;
            case 4:ok=setting_service_request_beep(v);break;
        }
        if(!ok)notify(UI_NOTICE_WARNING,UI_N_("Setting not sent. Finish the current operation and try again."));else clear_notice();return;}
    if(output_action(id))return;
    if(id==A_DIAGNOSTICS){settings_detail_dialog_show(ui_tr("Export support report?"),ui_tr("Insert a USB drive. The report contains versions, confirmed settings and current reject codes only. It excludes serials, note images, amounts, names, photos and passwords."),ui_tr("Export"),ui_tr("Cancel"),support_export,NULL,NULL);return;}
    if(id>=450&&id<=452){static const uint8_t modes[]={MODE_MDC,MODE_SDC,MODE_CNT};mode_confirm((void *)(uintptr_t)modes[id-450]);return;}
    if(id==A_CAPACITY){char value[8];snprintf(value,sizeof(value),"%u",machine_state_reject_pocket_max());settings_detail_keyboard_show(ui_tr("Reject pocket / 30-100 notes"),value,3,SETTINGS_DETAIL_KEYBOARD_UINT,capacity_done,NULL);return;}
    if(id==A_TIMEOUT){char value[8];snprintf(value,sizeof(value),"%u",standby_config()->minutes);settings_detail_keyboard_show(ui_tr("Standby / 0-60 minutes"),value,2,SETTINGS_DETAIL_KEYBOARD_UINT,timeout_done,NULL);return;}
    if(!workspace_store_ready()||workspace_store_busy()){ui_notice_post_message(UI_NOTICE_WARNING,"menu.operation",UI_N_("Menu"),workspace_store_message_info());return;}
    if(id==500){reset_batch();menu.dirty=true;return;}
    if(id==A_SAVE_BATCH){if(workspace_service_save_batches(menu.owner,menu.batches,menu.batch_count,menu.batch_active_original,menu.batch_active_edited)){menu.saving=1;app_ui_runtime_notice_started(APP_UI_NOTICE_BATCH_SAVE,UI_N_("Saving..."));}else notify(UI_NOTICE_ERROR,UI_N_("Save failed. Finish the current task and check storage."));return;}
    if(id==A_ADD_BATCH){if(menu.batch_count>=WORKSPACE_BATCHES){explain(UI_N_("Up to 10 slots, including OFF."));return;}if(!menu.batch_dirty)capture_batch_active();unsigned v=1;for(;;v++){bool used=false;for(unsigned i=1;i<menu.batch_count;i++)if(menu.batches[i]==v)used=true;if(!used)break;}menu.selected_batch=menu.batch_count;menu.batches[menu.batch_count++]=v;menu.batch_page=menu.selected_batch/5;menu.batch_dirty=menu.dirty=true;return;}
    if(id==A_EDIT_BATCH){if(!menu.selected_batch){explain(UI_N_("OFF is a fixed slot."));return;}char v[8];snprintf(v,sizeof(v),"%u",menu.batches[menu.selected_batch]);settings_detail_keyboard_show(ui_tr("Notes per batch (1-199)"),v,3,SETTINGS_DETAIL_KEYBOARD_UINT,batch_submit,NULL);return;}
    if(id==A_DELETE_BATCH){if(!menu.selected_batch||menu.batch_count<=2){explain(UI_N_("Keep OFF and at least one numeric slot."));return;}if(!menu.batch_dirty)capture_batch_active();if(menu.batches[menu.selected_batch]==menu.batch_active_edited)menu.batch_active_original=menu.batch_active_edited=0;memmove(menu.batches+menu.selected_batch,menu.batches+menu.selected_batch+1,menu.batch_count-menu.selected_batch-1);menu.batch_count--;if(menu.selected_batch>=menu.batch_count)menu.selected_batch=menu.batch_count-1;menu.batch_page=menu.selected_batch/5;menu.batch_dirty=menu.dirty=true;return;}
    if(id==A_APPLY_BATCH){
        if((app_command_runtime_count_start_busy()||app_add_runtime_busy())||workspace_service_applying()){notify(UI_NOTICE_WARNING,UI_N_("Finish the current operation first."));return;}
        unsigned n=menu.batches[menu.selected_batch];
        if(menu.batch_dirty){
            if(workspace_service_save_batches(menu.owner,menu.batches,menu.batch_count,0,0)){
                menu.batch_apply_target=(uint8_t)n;menu.saving=4;
                app_ui_runtime_notice_started(APP_UI_NOTICE_BATCH_APPLY,UI_N_("Saving..."));
            }else notify(UI_NOTICE_ERROR,UI_N_("Save failed. Finish the current task and check storage."));
            return;
        }
        if(!setting_service_request_batch_switch(n!=0,n?n:200,machine_state_batch_enabled(),machine_state_batch_num()))
            notify(UI_NOTICE_ERROR,UI_N_("Batch could not be sent. Check the controller."));
        else clear_notice();
        return;
    }
    if(id==A_NEW_PROFILE){settings_detail_keyboard_show(ui_tr("Name this profile"),"",WORKSPACE_NAME,SETTINGS_DETAIL_KEYBOARD_TEXT,name_submit,(void *)1);return;}
    if(id==A_DELETE_PROFILE){if(!u||u->profile_count<=1){explain(UI_N_("Keep at least one counting profile."));return;}char s[120];snprintf(s,sizeof(s),ui_tr("Delete %s? Current machine values will not change."),profile_display_name(&u->profiles[menu.selected_profile]));settings_detail_dialog_show_ex(SETTINGS_DIALOG_DESTRUCTIVE,ui_tr("Delete profile"),s,ui_tr("Delete"),ui_tr("Cancel"),delete_profile,NULL,NULL);return;}
    if(id==A_APPLY_PROFILE){
        const workspace_user_t *u=user();if(!u||menu.selected_profile>=u->profile_count)return;
        const workspace_profile_t *p=&u->profiles[menu.selected_profile];
        uint8_t requested=p->mode?p->mode:current_profile().mode;
        const char *message=workspace_service_profile_mode(p)!=requested?
            UI_N_("This currency requires MDC for this profile. Other settings apply normally. Currency and the saved profile stay unchanged. A confirmed count-mode change clears the current result; saved history remains."):
            UI_N_("Speed, sorting, sound, Batch, ADD and start method will be confirmed one at a time. Currency is unchanged. A count-mode change clears the current Main result after confirmation; saved history remains.");
        settings_detail_dialog_show(ui_tr("Apply profile?"),ui_tr(message),ui_tr("Apply"),ui_tr("Cancel"),apply_profile,NULL,NULL);return;
    }
    if(id==A_QUICK_TOGGLE||id==A_AUTO_QR){workspace_model_t *m=draft();if(!m)return;workspace_user_t *v=workspace_find(m,m->operators_enabled?m->active_id:0);if(id==A_QUICK_TOGGLE)v->quick_enabled=!v->quick_enabled;else v->qr_after_count=!v->qr_after_count;save(m);return;}
    if(id==A_NEW_USER){if(workspace_store_get()->user_count>=WORKSPACE_USERS){explain(UI_N_("This device supports up to 8 local operators."));return;}menu.editing_user=0;menu.user_edit=true;menu.user_name[0]=menu.employee_id[0]=menu.team[0]=0;memset(&menu.avatar,0,sizeof(menu.avatar));menu.dirty=true;return;}
    if(id==A_DELETE_USER){ui_manager_push_page(UI_PAGE_OPERATORS);return;}
    if(id==A_REMOVE_PHOTO){memset(&menu.avatar,0,sizeof(menu.avatar));menu.dirty=true;return;}
    if(id==A_EDIT_USER){const workspace_user_t *v=&workspace_store_get()->users[menu.selected_user];menu.editing_user=v->id;menu.user_edit=true;snprintf(menu.user_name,sizeof(menu.user_name),"%s",v->name);snprintf(menu.employee_id,sizeof(menu.employee_id),"%s",v->employee_id);snprintf(menu.team,sizeof(menu.team),"%s",v->team);menu.avatar=v->avatar;menu.dirty=true;return;}
    if(id==A_EMPLOYEE_ID||id==A_TEAM){settings_detail_keyboard_show(id==A_EMPLOYEE_ID?ui_tr("Employee ID (optional)"):ui_tr("Branch / team (optional)"),id==A_EMPLOYEE_ID?menu.employee_id:menu.team,id==A_EMPLOYEE_ID?WORKSPACE_EMPLOYEE_ID:WORKSPACE_TEAM,SETTINGS_DETAIL_KEYBOARD_TEXT,name_submit,(void *)(uintptr_t)(id==A_EMPLOYEE_ID?2:3));return;}
    if(id==A_USER_CANCEL){menu.user_edit=false;menu.dirty=true;return;}
    if(id==A_EDIT_NAME){settings_detail_keyboard_show(ui_tr("Operator name"),menu.user_name,WORKSPACE_NAME,SETTINGS_DETAIL_KEYBOARD_TEXT,name_submit,NULL);return;}
    if(id==A_USB_SCAN){if(workspace_store_scan_usb()){menu.scan_wait=true;app_ui_runtime_notice_started(APP_UI_NOTICE_WORKSPACE_STORE,UI_N_("Looking for USB photos..."));}return;}
    if(id==602){if(!workspace_store_avatar()->present){explain(UI_N_("Choose a photo first."));return;}menu.avatar=*workspace_store_avatar();lv_obj_del(menu.photo_sheet);menu.photo_sheet=NULL;menu.dirty=true;return;}
    if(id==A_USER_SAVE){workspace_model_t *m=draft();if(!m)return;uint32_t new_id=menu.editing_user;
        if(!new_id&&!workspace_add_user(m,menu.user_name,&new_id)){free(m);explain(UI_N_("Use a unique name, 1-24 characters. Maximum 8 operators."));return;}
        workspace_user_t *v=workspace_find(m,new_id);if(!v){free(m);return;}
        snprintf(v->name,sizeof(v->name),"%s",menu.user_name);snprintf(v->employee_id,sizeof(v->employee_id),"%s",menu.employee_id);snprintf(v->team,sizeof(v->team),"%s",menu.team);v->avatar=menu.avatar;
        if(!workspace_model_valid(m)){free(m);explain(UI_N_("Use unique names/IDs. IDs: letters, digits, - or _."));return;}
        unsigned n=menu.editing_user?menu.selected_user:m->user_count-1;if(save(m)){menu.saving=2;menu.selected_user=n;}return;}
    if(id==A_USER_SWITCH){
        const workspace_model_t *m=workspace_store_get();if(!m||menu.selected_user>=m->user_count)return;
        const workspace_user_t *target=operator_switch_target(m,m->users[menu.selected_user].id);
        if(!target||target->id==m->active_id)return;
        const char *reason=workspace_service_switch_blocker();if(reason){explain(reason);return;}
        char content[640];snprintf(content,sizeof(content),"%s\n%s\n\n%s",target->name,
            ui_tr("New records use this person; balances and old records stay unchanged."),
            ui_tr("Unconfirmed calculator drafts will be cleared."));
        settings_detail_dialog_show(ui_tr("Switch operator?"),content,ui_tr("Confirm switch"),ui_tr("Cancel"),
            confirm_operator_switch,NULL,(void *)(uintptr_t)target->id);return;
    }
}
static void timer(lv_timer_t *t)
{
    (void)t;if(!page_03_menu_is_visible())return;
    lv_indev_t *input=lv_indev_get_next(NULL);
    if(input&&input->proc.state==LV_INDEV_STATE_PRESSED)return;
    if(menu.task_scroll&&lv_obj_is_scrolling(menu.task_scroll))return;
    if(menu.tab==3&&!menu.sub){
        print_config_value_t current;print_config_get(&current);
        if(memcmp(&current,&output_observed,sizeof(current))){menu.dirty=true;output_observed=current;}
        if(output_waiting&&!print_config_pending()){
            output_waiting=false;menu.dirty=true;

        }
    }
    if(menu.quick)refresh_quick();
    if(menu.interaction_created){
        if(menu.interaction_controls.pointer_down || lv_content_pager_is_moving(menu.interaction_controls.pager))return;
        lv_quick_controls_refresh(&menu.interaction_controls,lv_quick_controls_current_state());
        if(menu.tab==4 && menu.sub==1 && !menu.shortcut_visibility &&
            !lv_obj_has_flag(menu.interaction_controls.version_panel,LV_OBJ_FLAG_HIDDEN))return;
    }

    if(menu.photo_ready){menu.photo_ready=false;if(menu.user_edit)photo_sheet();}
    if(menu.dirty&&!settings_detail_overlay_is_open()&&!menu.photo_sheet&&!menu.quick){lv_indev_t *input=lv_indev_get_next(NULL);if(input&&input->proc.state==LV_INDEV_STATE_PRESSED)return;render();}
}
void ui_page_03_menu_create(lv_obj_t *parent)
{
    if(menu.root)return;
    menu.root=box(parent?parent:lv_scr_act(),0,0,1280,400,0xFFFFFF,0);
    lv_obj_t *back=lv_nav_back_text_button_create(menu.root,24,15,
        ui_message_font(&lv_font_instrument_sans_medium_16),action,(void *)A_BACK);
    if(back)focus_style(back);
    lv_obj_t *title=label(menu.root,112,14,LV_SIZE_CONTENT,ui_tr("Menu"),&lv_font_instrument_sans_semibold_25,INK);
    lv_label_set_long_mode(title,LV_LABEL_LONG_CLIP);
    lv_obj_update_layout(title);lv_obj_set_y(title,14+(44-lv_obj_get_height(title))/2);
    int quick_width=LV_MAX(102,menu_text_width(ui_tr("Quick"))+46);
    int quick_x=1204-quick_width;
    int widths[4],nav_width=14;
    for(unsigned n=0;n<4;n++){
        unsigned i=visible_tabs[n];
        widths[n]=LV_MAX(140,menu_text_width(ui_tr(tabs[i]))+46);
        nav_width+=widths[n];
    }
    int nav_left=LV_MAX(112+lv_obj_get_width(title)+24,(1280-nav_width)/2);
    nav_left=LV_MIN(nav_left,quick_x-16-nav_width);
    lv_obj_t *nav=box(menu.root,nav_left,12,nav_width,48,PANEL,14);
    int nav_x=4;
    for(unsigned n=0;n<4;n++){
        unsigned i=visible_tabs[n];
        menu.nav[i]=lv_settings_button(nav,nav_x,4,widths[n],40,ui_tr(tabs[i]),false,tab_event,(void *)(uintptr_t)i);nav_x+=widths[n]+2;
        header_surface(menu.nav[i]);
        header_content(menu.nav[i],ui_tr(tabs[i]),tab_icons[i],accents[i]);
    }
    if(!menu.tab)menu.tab=1;
    lv_obj_t *quick=button(menu.root,quick_x,14,quick_width,44,ui_tr("Quick"),A_QUICK,false);
    header_surface(quick);lv_damped_button_set_exact_palette(quick,lv_color_hex(PANEL),lv_color_hex(LINE));header_content(quick,ui_tr("Quick"),MICON("quick",22),PURPLE);
    lv_obj_t *lock=button(menu.root,1212,14,44,44,"",A_SETTINGS,false);
    header_surface(lock);lv_damped_button_set_exact_palette(lock,lv_color_hex(PANEL),lv_color_hex(LINE));header_content(lock,"",MICON("lock",22),INK);
    menu.body=box(menu.root,24,76,1232,296,0xFFFFFF,0);lv_obj_set_style_bg_opa(menu.body,LV_OPA_TRANSP,0);
    reset_batch();menu.timer=lv_timer_create(timer,80,NULL);render();
}
bool page_03_menu_is_created(void){return menu.root!=NULL;}
bool page_03_menu_is_visible(void){return (menu.root&&lv_obj_is_visible(menu.root));}
void ui_page_03_menu_refresh_data(uint32_t topics)
{
    (void)topics;menu.dirty=true;if(!menu.root)return;
    if(menu.saving&&!workspace_store_busy()) {
        if((menu.saving==1||menu.saving==4)&&workspace_store_last_success()) {
            menu.batch_dirty=false;
            /* The next save starts from the persisted edit; only ACK updates Main. */
            menu.batch_active_original=menu.batch_active_edited;
            if(menu.saving==4){
                uint8_t target=menu.batch_apply_target;
                if(!setting_service_request_batch_switch(target!=0,target?target:200,
                        machine_state_batch_enabled(),machine_state_batch_num()))
                    ui_notice_post_text(UI_NOTICE_ERROR,"settings.batch",UI_N_("Batch"),UI_N_("Batch was saved, but could not be sent. Use this preset to retry."));
            }
        }else if(workspace_store_last_success())menu.user_edit=false;
        menu.batch_apply_target=0;
        menu.saving=0;
    }
    if((menu.scan_wait||menu.photo_wait)&&!workspace_store_busy()) {menu.scan_wait=menu.photo_wait=false;if(menu.user_edit&&page_03_menu_is_visible())menu.photo_ready=true;}
}
void page_03_menu_clear_batch_tip(void){clear_notice();}
void page_03_menu_show_batch_saved_tip(void){menu.dirty=true;}
void page_03_menu_refresh_batch_number(void){menu.dirty=true;}
void page_03_menu_refresh_batch_mode(void){menu.dirty=true;}
bool ui_page_03_menu_resume(void)
{if(!menu.root)return false;if(!menu.batch_dirty&&!menu.saving)reset_batch();lv_obj_clear_flag(menu.root,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(menu.root);lv_timer_resume(menu.timer);menu.dirty=true;render();return true;}
void ui_page_03_menu_suspend(void)
{if(!menu.root)return;if(menu.qr_generation&&menu.qr_generation==lv_qr_popup_generation())lv_qr_popup_hide();menu.qr_generation=0;workspace_service_cancel_apply();settings_detail_keyboard_hide();settings_detail_dialog_hide();destroy_quick();if(menu.photo_sheet)lv_obj_del(menu.photo_sheet);if(menu.user_sheet)lv_obj_del(menu.user_sheet);menu.user_sheet=NULL;menu.photo_sheet=NULL;menu.scan_wait=menu.photo_wait=menu.photo_ready=false;clear_notice();if(menu.interaction_created)lv_quick_controls_reset(&menu.interaction_controls);gesture_service_clear_pointer_policy(UI_PAGE_MENU);lv_timer_pause(menu.timer);lv_obj_add_flag(menu.root,LV_OBJ_FLAG_HIDDEN);}
void ui_page_03_menu_destroy(void)
{ui_page_03_menu_suspend();if(menu.timer)lv_timer_del(menu.timer);if(menu.root)lv_obj_del(menu.root);memset(&menu,0,sizeof(menu));}
void page_03_menu_open_batch(void)
{
    menu.tab=1;menu.sub=0;menu.dirty=true;
    ui_manager_push_page(UI_PAGE_MENU);
}
