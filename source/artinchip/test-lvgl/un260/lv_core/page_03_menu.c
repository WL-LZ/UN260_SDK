#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/app_service/app_ui_runtime.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_03_menu.h"
#include "lv_page_manager.h"
#include "lv_page_event.h"
#include "settings_detail_ui.h"
#include "page_05_set_password.h"
#include "page_19_history.h"
#include "page_01_main_layout.h"
#include "un260/app_service/workspace_service.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/app_service/setting_service.h"
#include "un260/app_service/support_report.h"
#include "un260/storage/workspace_store.h"
#include "un260/storage/standby_store.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_components/lv_quick_controls.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/lv_qr_popup.h"
#include "un260/lv_components/ui_scrollbar.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/lv_system/ui_qr_data.h"
#include "un260/lv_system/app_clock.h"
#include "un260/currency/currency_state.h"
#include "un260/machine_state/machine_state.h"
#include "un260/gesture/gesture_service.h"
#include "un260/gesture/gesture_guide.h"
#include "un260/device_info/device_info.h"
#include "un260/storage/cashbook_store.h"
#include "un260/lv_system/backlight_service.h"
#include "un260/lv_system/machine_time.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_multi.h"
#include "un260/counting/counting_cashbook.h"
#include "un260/counting/counting_history_service.h"
#include "un260/counting/counting_reject_reason.h"
#include "un260/print/print_config.h"
#include "lv_port_indev.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_resources/ui_icons.h"
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
      A_GESTURES,A_LAYOUT,A_QUICK_TOGGLE,A_BRIGHTNESS,A_NEW_PROFILE,A_APPLY_PROFILE,
      A_DELETE_PROFILE,A_SAVE_BATCH,A_ADD_BATCH,A_EDIT_BATCH,A_DELETE_BATCH,A_APPLY_BATCH,
      A_NEW_USER,A_EDIT_NAME,A_USB_SCAN,A_USER_SAVE,A_USER_CANCEL,A_USER_SWITCH,A_USB_CLOSE,A_QUICK_CLOSE,
      A_AUTO_QR,A_CAPACITY,A_LANGUAGE,A_TIMEOUT,A_BUSINESS_DAY,A_CLOSE_DAY,A_CONFIRM_RUN,A_MERGE_RUN,A_SPLIT_RUN,A_EXCLUDE_RUN,A_DIAGNOSTICS,A_ARCHIVES,A_ARCHIVE_PERIOD,A_EXPORT_RECORDS,A_CURRENT_RECORDS,A_CANCEL_VERIFY,A_EDIT_USER,A_EMPLOYEE_ID,A_TEAM,A_REMOVE_PHOTO};
static const char *const tabs[]={UI_N_("Overview"),UI_N_("Count"),UI_N_("Records"),UI_N_("Output"),UI_N_("Preferences"),UI_N_("Help")};
static const char *const tab_icons[]={MICON("overview",19),MICON("layers",19),MICON("history",19),MICON("output",19),MICON("settings",19),MICON("help",19)};
static const uint32_t accents[]={BLUE,ORANGE,TEAL,BLUE,PURPLE,TEAL};
static const char *const sublabels[][5]={{NULL},{UI_N_("Batch presets"),UI_N_("Profiles"),UI_N_("Options")},
    {UI_N_("Daily totals"),UI_N_("Recounts"),UI_N_("History"),UI_N_("Verify count"),UI_N_("Day closes")},{UI_N_("Print"),UI_N_("QR export")},
    {UI_N_("Operators"),UI_N_("Interaction"),UI_N_("Display & sound")},{UI_N_("Reject guide"),UI_N_("Care"),UI_N_("Device")}};
static const unsigned subcounts[]={0,3,5,2,3,3};
static struct {
    lv_obj_t *root,*body,*quick,*photo_sheet,*user_sheet,*record_sheet,*nav[6],*task_scroll;
    lv_timer_t *timer;
    unsigned tab,sub,selected_batch,selected_profile,selected_user,batch_page;
    bool record_pending,verify_pick,reject_all;
    bool dirty,batch_dirty,user_edit,scan_wait,photo_wait,photo_ready,show_archives;
    unsigned saving;
    uint32_t selected_group,selected_run,selected_close,record_day,qr_generation;
    unsigned reject_code;
    lv_obj_t *brightness_value;
    int brightness_previous;
    bool brightness_drag;
    lv_obj_t *quick_switch[3],*quick_state[3];
    bool quick_value[3];
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
static void records_detail(lv_obj_t *);
static uint32_t business_day(void);
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
    lv_obj_set_style_text_font(lv_obj_get_child(v,0),h<=36?&lv_font_instrument_sans_medium_15:&lv_font_instrument_sans_medium_17,0);
    lv_obj_set_style_radius(v,h<=36?9:12,0);
    lv_obj_set_style_border_width(v,0,LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(v,lv_color_hex(0xE8EBEE),LV_STATE_DISABLED);
    lv_obj_set_style_text_color(v,lv_color_hex(0x64717C),LV_STATE_DISABLED);
    return v;
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
static void header_content(lv_obj_t *button,const char *title,const char *src,uint32_t color)
{
    lv_obj_t *l=lv_obj_get_child(button,0);
    lv_label_set_text(l,title);lv_obj_set_width(l,LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(l,&lv_font_instrument_sans_medium_16,0);
    lv_obj_t *im=ui_icon_create(button,src);
    lv_obj_set_style_img_recolor(im,lv_color_hex(color),0);
    lv_obj_set_style_img_recolor_opa(im,LV_OPA_COVER,0);
    lv_obj_update_layout(button);
    if(!*title){lv_obj_center(im);return;}
    int iw=lv_obj_get_width(im),tw=lv_obj_get_width(l);
    int start=(lv_obj_get_content_width(button)-iw-8-tw)/2;
    lv_obj_align(im,LV_ALIGN_LEFT_MID,start,0);
    lv_obj_align(l,LV_ALIGN_LEFT_MID,start+iw+8,0);
}
static void header_surface(lv_obj_t *o)
{
    focus_style(o);
    lv_damped_button_set_exact_palette(o,lv_color_hex(HEADER_SURFACE),lv_damped_button_pressed_color(lv_color_hex(HEADER_SURFACE)));
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
    clear_notice();menu.show_archives=false;menu.selected_close=0;
    menu.tab=(uintptr_t)lv_event_get_user_data(e);menu.sub=0;menu.selected_group=0;menu.record_pending=menu.verify_pick=false;menu.dirty=true;
}
static void sub_event(lv_event_t *e)
{
    if(workspace_service_applying()){notify(UI_NOTICE_WARNING,UI_N_("Wait for the profile to finish applying."));return;}
    if(menu.saving||menu.batch_dirty||menu.user_edit){explain(UI_N_("Save or cancel your changes first."));return;}
    clear_notice();menu.show_archives=false;
    menu.sub=(uintptr_t)lv_event_get_user_data(e);menu.selected_group=menu.selected_close=0;menu.record_pending=menu.verify_pick=false;menu.dirty=true;
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
    text(p,0,y+8,w-144,title);if(hint)small(p,0,y+32,w-135,hint);
    lv_obj_t *state=small(p,w-106,y+12,42,on?ui_tr("On"):ui_tr("Off"));
    lv_obj_set_style_text_color(state,lv_color_hex(on?BLUE:MUTED),0);
    lv_obj_t *v=lv_settings_toggle(p,w-52,y+(hint?15:6),on,action,(void *)(uintptr_t)id);
    lv_obj_set_width(v,52);lv_anim_del(v,NULL);
    lv_obj_set_style_bg_color(v,lv_color_hex(on?BLUE:0xB8C7D0),0);
    lv_obj_t *knob=lv_obj_get_child(v,0);lv_anim_del(knob,NULL);lv_obj_set_x(knob,on?25:3);
}
#include "menu_composition.inc"

static void profile_summary(lv_obj_t *p,const workspace_profile_t *v,int y)
{
    const char *speed[]={ui_tr("Low"),ui_tr("Standard"),ui_tr("High")},*sort[]={ui_tr("Off"),ui_tr("Face"),ui_tr("Orientation"),ui_tr("Face + orientation")};
    char batch[20];snprintf(batch,sizeof(batch),v->batch_enabled?ui_trn("%u notes", (v->batch)):ui_tr("Off"),v->batch);
    const char *titles[]={ui_tr("Speed"),ui_tr("Start"),ui_tr("Counting"),ui_tr("Sorting"),"ADD",ui_tr("Batch")};
    const char *values[]={speed[v->speed],v->work?ui_tr("Manual"):ui_tr("Auto"),v->mode==1?ui_tr("Mixed"):v->mode==2?ui_tr("Single"):v->mode==3?ui_tr("Count"):ui_tr("Keep current"),sort[v->sort],v->add?ui_tr("On"):ui_tr("Off"),batch};
    for(unsigned i=0;i<6;i++){
        int x=20+(i%3)*132,top=y+(i/3)*51;
        small(p,x,top,125,titles[i]);text(p,x,top+20,125,values[i]);
    }
}
static workspace_profile_t current_profile(void)
{
    return (workspace_profile_t){.mode=machine_state_mode()==MODE_SDC?2:machine_state_mode()==MODE_CNT?3:1,.speed=machine_state_speed(),.work=machine_state_work_mode(),.add=machine_state_add_enabled(),.sort=machine_state_fo_mode(),.beep=machine_state_buzzer_enabled(),.batch_enabled=machine_state_batch_enabled(),.batch=machine_state_batch_num()};
}
#include "menu_studio_views.inc"
#include "menu_records.inc"
#include "menu_output_help.inc"
static const char *const side_icons[][5]={
    {NULL},{"layers","profiles","options"},
    {"history","repeat","search","shield","receipt"},
    {"print","qr"},{"user","hand","sun"},
    {"reject","brush","screen"}
};
static void render(void)
{
    if(!menu.root)return;
    if(menu.tab!=menu.rendered_tab||menu.sub!=menu.rendered_sub)clear_notice();
    if(menu.task_scroll)menu.scroll_y[menu.rendered_tab][menu.rendered_sub]=lv_obj_get_scroll_y(menu.task_scroll);
    menu.task_scroll=NULL;menu.brightness_value=NULL;
    if(menu.user_sheet){lv_obj_del(menu.user_sheet);menu.user_sheet=NULL;}
    if(menu.record_sheet){lv_obj_del(menu.record_sheet);menu.record_sheet=NULL;}
    lv_obj_clean(menu.body);lv_obj_t *b=menu.body;
    if(!menu.tab)overview();else {
        lv_obj_t *side=box(b,0,0,180,296,0xFFFFFF,0);
        for(unsigned i=0;i<subcounts[menu.tab];i++) {
            bool selected=menu.sub==i;
            lv_obj_t *v=lv_settings_button(side,0,2+48*i,180,44,"",false,sub_event,(void *)(uintptr_t)i);
            lv_damped_button_set_exact_palette(v,lv_color_hex(selected?HEADER_SURFACE:0xFFFFFF),lv_color_hex(0xDCE4E8));
            lv_obj_set_style_border_width(v,0,0);lv_obj_set_style_radius(v,12,0);focus_style(v);
            char path[128];snprintf(path,sizeof(path),LVGL_DIR "menu_icons/%s_20.png",side_icons[menu.tab][i]);
            icon(v,path,12,12,selected?BLUE:MUTED);
            label(v,42,12,132,ui_tr(sublabels[menu.tab][i]),selected?&lv_font_instrument_sans_semibold_16:&lv_font_instrument_sans_medium_16,selected?BLUE:MUTED);
            if(selected)box(v,0,13,3,18,BLUE,2);
        }
        lv_obj_t *content=box(b,204,0,1028,296,0xFFFFFF,0);
        if(menu.tab==1){if(!menu.sub)batch_view(content);else if(menu.sub==1)profiles_view(content);else options_view(content);}
        if(menu.tab==2)records_view(content);
        if(menu.tab==3)output_view(content);
        if(menu.tab==4){if(!menu.sub)operator_view(content);else if(menu.sub==1)interaction_view(content);else display_view(content);}
        if(menu.tab==5)help_view(content);
    }
    for(unsigned i=0;i<6;i++){choose(menu.nav[i],i==menu.tab);lv_obj_t *l=lv_obj_get_child(menu.nav[i],0);lv_obj_set_style_text_color(l,lv_color_hex(i==menu.tab?BLUE:0x455D6B),0);lv_obj_set_style_text_font(l,i==menu.tab?&lv_font_instrument_sans_semibold_16:&lv_font_instrument_sans_medium_16,0);}
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
        if(!workspace_add_profile(m,m->active_id,&p)){free(m);explain(UI_N_("Use a unique profile name. Up to 8 profiles are supported."));return;}save(m);}
    else{if((uintptr_t)context==2)snprintf(menu.employee_id,sizeof(menu.employee_id),"%s",value);else if((uintptr_t)context==3)snprintf(menu.team,sizeof(menu.team),"%s",value);else snprintf(menu.user_name,sizeof(menu.user_name),"%s",value);menu.dirty=true;}
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
static void close_quick(lv_event_t *e)
{if(lv_event_get_target(e)==menu.quick){lv_obj_del(menu.quick);menu.quick=NULL;}}
static void refresh_quick(void)
{
    bool values[]={machine_state_buzzer_enabled(),gesture_service_enabled(),workspace_service_quick_enabled()};
    for(unsigned i=0;i<3;i++){
        if(!menu.quick_switch[i]||menu.quick_value[i]==values[i])continue;
        menu.quick_value[i]=values[i];lv_obj_t *v=menu.quick_switch[i];lv_obj_t *knob=lv_obj_get_child(v,0);
        lv_anim_del(v,NULL);lv_anim_del(knob,NULL);
        if(values[i])lv_obj_add_state(v,LV_STATE_CHECKED);else lv_obj_clear_state(v,LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(v,lv_color_hex(values[i]?BLUE:0xB8C7D0),0);lv_obj_set_x(knob,values[i]?25:3);
        lv_label_set_text(menu.quick_state[i],values[i]?ui_tr("On"):ui_tr("Off"));lv_obj_set_style_text_color(menu.quick_state[i],lv_color_hex(values[i]?BLUE:MUTED),0);
    }
}
static void quick_row(lv_obj_t *p,int y,const char *name,unsigned i,unsigned id,bool on)
{
    text(p,0,y+9,241,name);
    menu.quick_state[i]=label(p,249,y+12,35,on?ui_tr("On"):ui_tr("Off"),&lv_font_instrument_sans_menumedium_14,on?BLUE:MUTED);
    lv_obj_t *v=lv_settings_toggle(p,287,y+6,on,action,(void *)(uintptr_t)id);menu.quick_switch[i]=v;
    lv_obj_set_width(v,52);lv_anim_del(v,NULL);lv_obj_t *knob=lv_obj_get_child(v,0);lv_anim_del(knob,NULL);
    lv_obj_set_style_bg_color(v,lv_color_hex(on?BLUE:0xB8C7D0),0);lv_obj_set_x(knob,on?25:3);menu.quick_value[i]=on;
}
static void open_quick(void)
{
    if(menu.quick)return;
    clear_notice();
    menu.quick=box(menu.root,0,0,1280,400,INK,0);lv_obj_set_style_bg_opa(menu.quick,LV_OPA_30,0);lv_obj_add_flag(menu.quick,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(menu.quick,close_quick,LV_EVENT_CLICKED,NULL);
    lv_obj_t *sheet=box(menu.quick,0,0,1280,255,0xFFFFFF,20);lv_obj_add_flag(sheet,LV_OBJ_FLAG_CLICKABLE);
    icon(sheet,MICON("quick",25),28,25,PURPLE);label(sheet,65,22,900,ui_tr("Quick controls"),&lv_font_instrument_sans_semibold_26,INK);
    lv_obj_t *close=button(sheet,1144,20,108,36,ui_tr("Close"),A_QUICK_CLOSE,false);lv_nav_button_mark_back(close);
    const char *titles[]={ui_tr("Sound"),ui_tr("Interaction"),ui_tr("Taking a break")},*icons[]={MICON("volume",22),MICON("hand",22),MICON("power",22)};
    for(unsigned i=0;i<3;i++){
        int x=28+i*416;lv_obj_t *col=box(sheet,x,80,392,132,0xFFFFFF,0);
        icon(col,icons[i],0,0,PURPLE);label(col,30,1,352,titles[i],&lv_font_instrument_sans_semibold_18,INK);
        if(!i)quick_row(col,39,ui_tr("Machine sound"),0,813,machine_state_buzzer_enabled());
        else if(i==1){quick_row(col,39,ui_tr("Side gestures"),1,A_GESTURES,gesture_service_enabled());quick_row(col,87,ui_tr("Pull-down"),2,A_QUICK_TOGGLE,workspace_service_quick_enabled());}
        else{button(col,0,40,392,44,ui_tr("Standby"),A_STANDBY,false);small(col,0,98,392,ui_tr("Return to exactly where you left off."));}
    }
    small(sheet,28,227,1224,ui_tr("Quick controls are always reachable from Menu. Screen brightness stays in Display & sound."));
}

 #include "menu_record_actions.inc"
static void action(lv_event_t *event)
{
    unsigned id=(uintptr_t)lv_event_get_user_data(event);const workspace_user_t *u=user();
    if(id==A_BACK){if(menu.saving){explain(UI_N_("The workspace is being saved. Please wait a moment."));return;}if(menu.batch_dirty||menu.user_edit||workspace_service_applying())settings_detail_dialog_show(ui_tr("Leave Menu?"),ui_tr("Unsaved edits will be discarded. Remaining profile steps will stop."),ui_tr("Leave"),ui_tr("Stay"),confirmed_back,NULL,NULL);else confirmed_back(NULL);return;}
    if(workspace_service_applying()){notify(UI_NOTICE_WARNING,UI_N_("Wait for the profile, or use Back to stop remaining steps."));return;}
    if(menu.saving){notify(UI_NOTICE_WARNING,UI_N_("The workspace is being saved. Please wait a moment."));return;}
    if((menu.batch_dirty||menu.user_edit)&&(id==A_SETTINGS||id==A_QUICK||id==A_HISTORY||id==A_PRINT_SETUP||id==A_BRIGHTNESS||id==601||id==600||id==A_STANDBY)){
        explain(UI_N_("Save or cancel your changes before leaving this task."));return;
    }
    if(id==A_SETTINGS){ui_page_05_set_password_open();return;}if(id==A_QUICK){open_quick();return;}
    if(id==A_QUICK_CLOSE){if(menu.quick)lv_obj_del(menu.quick);menu.quick=NULL;return;}
    if(id==A_USB_CLOSE){if(menu.photo_sheet)lv_obj_del(menu.photo_sheet);menu.photo_sheet=NULL;menu.scan_wait=menu.photo_wait=menu.photo_ready=false;return;}
    if(id==A_HISTORY){ui_manager_push_page(UI_PAGE_HISTORY);return;}if(id==A_PRINT_SETUP){ui_manager_push_page(UI_PAGE_PRINT_SETTING);return;}
    if(id==A_PRINT){if(app_command_runtime_count_start_busy()||print_config_pending()){explain(UI_N_("Finish counting and receipt settings before printing."));return;}page_01_print_btn_event_cb(event);return;}
    if(id==A_QR){char payload[3072];if(app_command_runtime_count_start_busy()||!ui_qr_data_build_summary(payload,sizeof(payload))||!lv_qr_popup_show(payload))explain(UI_N_("Finish counting and wait for the complete summary."));else menu.qr_generation=lv_qr_popup_generation();return;}
    if(id==A_BRIGHTNESS){ui_manager_push_page(UI_PAGE_BRIGHTNESS_SETTING);return;}
    if(id==601){ui_manager_push_page(UI_PAGE_STANDBY_SETTING);return;}if(id==600){ui_manager_push_page(UI_PAGE_LIST);return;}
    if(id==A_STANDBY){if(app_command_runtime_count_start_busy()||standby_store_busy())explain(UI_N_("Finish the current operation before standby."));else ui_manager_push_page(UI_PAGE_STANDBY);return;}
    if(id==A_GESTURES||id==A_LAYOUT){if(id==A_GESTURES){if(!gesture_service_set_enabled(!gesture_service_enabled()))notify(UI_NOTICE_ERROR,UI_N_("Could not save the gesture preference."));}else page_01_main_layout_set_enabled(!page_01_main_layout_is_enabled());menu.dirty=true;if(menu.quick)refresh_quick();return;}
    if(id==307||id==308){menu.tab=id==307?3:5;menu.sub=0;menu.dirty=true;return;}
    if(id==510||id==511){if(id==510&&menu.batch_page)menu.batch_page--;if(id==511&&(menu.batch_page+1)*5<menu.batch_count)menu.batch_page++;menu.dirty=true;return;}
    if(id>=301&&id<=306){
        menu.tab=id<=303?1:id==306?2:4;
        menu.sub=id==302?1:id==303?2:id==304?1:0;menu.selected_group=0;menu.dirty=true;return;
    }
    if(id==813)id=440+!machine_state_buzzer_enabled();
    if(id>=400&&id<450){if(workspace_service_applying()||app_command_runtime_count_start_busy()){notify(UI_NOTICE_WARNING,UI_N_("Finish the current operation before changing settings."));return;}unsigned kind=(id-400)/10,v=(id-400)%10;bool ok=false;
        switch(kind){
            case 0:ok=setting_service_request_speed(v);break;
            case 1:ok=setting_service_request_work_mode(v);break;
            case 2:ok=setting_service_request_add(v);break;
            case 3:ok=setting_service_request_fo_mode(v);break;
            case 4:ok=setting_service_request_beep(v);break;
        }
        if(!ok)notify(UI_NOTICE_WARNING,UI_N_("Setting not sent. Finish the current operation and try again."));else clear_notice();return;}
    if(id==A_CANCEL_VERIFY){counting_cashbook_cancel_verify();menu.dirty=true;notify(UI_NOTICE_INFO,UI_N_("Verification cancelled. Existing results are unchanged."));return;}
    if(record_action(id)||output_action(id))return;
    if(id==A_DIAGNOSTICS){settings_detail_dialog_show(ui_tr("Export support report?"),ui_tr("Insert a USB drive. The report contains versions, confirmed settings and current reject codes only. It excludes serials, note images, amounts, names, photos and passwords."),ui_tr("Export"),ui_tr("Cancel"),support_export,NULL,NULL);return;}
    if(id>=450&&id<=452){static const uint8_t modes[]={MODE_MDC,MODE_SDC,MODE_CNT};mode_confirm((void *)(uintptr_t)modes[id-450]);return;}
    if(id==A_CAPACITY){char value[8];snprintf(value,sizeof(value),"%u",machine_state_reject_pocket_max());settings_detail_keyboard_show(ui_tr("Reject pocket / 30-100 notes"),value,3,SETTINGS_DETAIL_KEYBOARD_UINT,capacity_done,NULL);return;}
    if(id==A_TIMEOUT){char value[8];snprintf(value,sizeof(value),"%u",standby_config()->minutes);settings_detail_keyboard_show(ui_tr("Standby / 0-60 minutes"),value,2,SETTINGS_DETAIL_KEYBOARD_UINT,timeout_done,NULL);return;}
    if(id==A_LANGUAGE){notify(UI_NOTICE_INFO,UI_N_("Language is managed in Settings > Display."));return;}
    if(!workspace_store_ready()||workspace_store_busy()){ui_notice_post_message(UI_NOTICE_WARNING,"menu.operation",UI_N_("Menu"),workspace_store_message_info());return;}
    if(id==500){reset_batch();menu.dirty=true;return;}
    if(id==A_SAVE_BATCH){if(workspace_service_save_batches(menu.owner,menu.batches,menu.batch_count,menu.batch_active_original,menu.batch_active_edited)){menu.saving=1;app_ui_runtime_notice_started(APP_UI_NOTICE_BATCH_SAVE,UI_N_("Saving..."));}else notify(UI_NOTICE_ERROR,UI_N_("Save failed. Finish the current task and check storage."));return;}
    if(id==A_ADD_BATCH){if(menu.batch_count>=WORKSPACE_BATCHES){explain(UI_N_("Up to 10 slots, including OFF."));return;}if(!menu.batch_dirty)capture_batch_active();unsigned v=1;for(;;v++){bool used=false;for(unsigned i=1;i<menu.batch_count;i++)if(menu.batches[i]==v)used=true;if(!used)break;}menu.selected_batch=menu.batch_count;menu.batches[menu.batch_count++]=v;menu.batch_page=menu.selected_batch/5;menu.batch_dirty=menu.dirty=true;return;}
    if(id==A_EDIT_BATCH){if(!menu.selected_batch){explain(UI_N_("OFF is a fixed slot."));return;}char v[8];snprintf(v,sizeof(v),"%u",menu.batches[menu.selected_batch]);settings_detail_keyboard_show(ui_tr("Notes per batch (1-199)"),v,3,SETTINGS_DETAIL_KEYBOARD_UINT,batch_submit,NULL);return;}
    if(id==A_DELETE_BATCH){if(!menu.selected_batch||menu.batch_count<=2){explain(UI_N_("Keep OFF and at least one numeric slot."));return;}if(!menu.batch_dirty)capture_batch_active();if(menu.batches[menu.selected_batch]==menu.batch_active_edited)menu.batch_active_original=menu.batch_active_edited=0;memmove(menu.batches+menu.selected_batch,menu.batches+menu.selected_batch+1,menu.batch_count-menu.selected_batch-1);menu.batch_count--;menu.selected_batch=menu.batch_page=0;menu.batch_dirty=menu.dirty=true;return;}
    if(id==A_APPLY_BATCH){if(menu.batch_dirty){explain(UI_N_("Save the batch cycle first."));return;}if(app_command_runtime_count_start_busy()||workspace_service_applying()){notify(UI_NOTICE_WARNING,UI_N_("Finish the current operation first."));return;}unsigned n=menu.batches[menu.selected_batch];if(!setting_service_request_batch_switch(n!=0,n?n:200,machine_state_batch_enabled(),machine_state_batch_num()))notify(UI_NOTICE_ERROR,UI_N_("Batch could not be sent. Check the controller."));else clear_notice();return;}
    if(id==A_NEW_PROFILE){settings_detail_keyboard_show(ui_tr("Name this profile"),"",WORKSPACE_NAME,SETTINGS_DETAIL_KEYBOARD_TEXT,name_submit,(void *)1);return;}
    if(id==A_DELETE_PROFILE){if(!u||u->profile_count<=1){explain(UI_N_("Keep at least one counting profile."));return;}char s[120];snprintf(s,sizeof(s),ui_tr("Delete %s? Current machine values will not change."),u->profiles[menu.selected_profile].name);settings_detail_dialog_show_ex(SETTINGS_DIALOG_DESTRUCTIVE,ui_tr("Delete profile"),s,ui_tr("Delete"),ui_tr("Cancel"),delete_profile,NULL,NULL);return;}
    if(id==A_APPLY_PROFILE){settings_detail_dialog_show(ui_tr("Apply profile?"),ui_tr("Speed, sorting, sound, Batch, ADD and start method will be confirmed one at a time. Currency is unchanged. A count-mode change clears the current Main result after confirmation; saved history remains."),ui_tr("Apply"),ui_tr("Cancel"),apply_profile,NULL,NULL);return;}
    if(id==A_QUICK_TOGGLE||id==A_AUTO_QR){workspace_model_t *m=draft();if(!m)return;workspace_user_t *v=workspace_find(m,m->active_id);if(id==A_QUICK_TOGGLE)v->quick_enabled=!v->quick_enabled;else v->qr_after_count=!v->qr_after_count;save(m);return;}
    if(id==A_NEW_USER){if(workspace_store_get()->user_count>=WORKSPACE_USERS){explain(UI_N_("This device supports up to 8 local operators."));return;}menu.editing_user=0;menu.user_edit=true;menu.user_name[0]=menu.employee_id[0]=menu.team[0]=0;memset(&menu.avatar,0,sizeof(menu.avatar));menu.dirty=true;return;}
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
    if(id==A_USER_SWITCH){const char *reason=workspace_service_switch_blocker();if(reason){explain(reason);return;}const workspace_model_t *m=workspace_store_get();if(m->users[menu.selected_user].id==m->active_id)return;if(!workspace_service_switch(m->users[menu.selected_user].id))notify(UI_NOTICE_ERROR,UI_N_("The operator could not be switched."));else app_ui_runtime_notice_started(APP_UI_NOTICE_WORKSPACE_STORE,UI_N_("Switching workspace..."));return;}
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

    if(menu.photo_ready){menu.photo_ready=false;if(menu.user_edit)photo_sheet();}
    if(menu.dirty&&!settings_detail_overlay_is_open()&&!menu.photo_sheet&&!menu.quick){lv_indev_t *input=lv_indev_get_next(NULL);if(input&&input->proc.state==LV_INDEV_STATE_PRESSED)return;render();}
}
void ui_page_03_menu_create(lv_obj_t *parent)
{
    if(menu.root)return;
    menu.root=box(parent?parent:lv_scr_act(),0,0,1280,400,0xFFFFFF,0);
    lv_obj_t *back=lv_settings_back(menu.root,24,14,44,44,action,(void *)A_BACK);
    header_surface(back);lv_damped_button_set_exact_palette(back,lv_color_hex(PANEL),lv_color_hex(LINE));header_content(back,"",MICON("back",22),INK);
    lv_obj_t *title=label(menu.root,80,14,142,ui_tr("Menu"),&lv_font_instrument_sans_semibold_25,INK);
    lv_obj_update_layout(title);lv_obj_set_y(title,14+(44-lv_obj_get_height(title))/2);
    lv_obj_t *nav=box(menu.root,287,12,706,48,HEADER_SURFACE,14);
    const int widths[]={126,103,121,109,147,82};int nav_x=4;
    for(unsigned i=0;i<6;i++){
        menu.nav[i]=lv_settings_button(nav,nav_x,4,widths[i],40,ui_tr(tabs[i]),false,tab_event,(void *)(uintptr_t)i);nav_x+=widths[i]+2;
        header_surface(menu.nav[i]);
        header_content(menu.nav[i],ui_tr(tabs[i]),tab_icons[i],accents[i]);
    }
    lv_obj_t *quick=button(menu.root,1102,14,102,44,ui_tr("Quick"),A_QUICK,false);
    header_surface(quick);lv_damped_button_set_exact_palette(quick,lv_color_hex(PANEL),lv_color_hex(LINE));header_content(quick,ui_tr("Quick"),MICON("quick",22),PURPLE);
    lv_obj_t *lock=button(menu.root,1212,14,44,44,"",A_SETTINGS,false);
    header_surface(lock);lv_damped_button_set_exact_palette(lock,lv_color_hex(PANEL),lv_color_hex(LINE));header_content(lock,"",MICON("lock",22),INK);
    backlight_service_probe();
    menu.body=box(menu.root,24,76,1232,296,0xFFFFFF,0);lv_obj_set_style_bg_opa(menu.body,LV_OPA_TRANSP,0);
    reset_batch();menu.timer=lv_timer_create(timer,80,NULL);render();
}
bool page_03_menu_is_created(void){return menu.root!=NULL;}
bool page_03_menu_is_visible(void){return menu.root&&lv_obj_is_visible(menu.root);}
void ui_page_03_menu_refresh_data(uint32_t topics)
{
    (void)topics;menu.dirty=true;if(!menu.root)return;
    if(menu.saving&&!workspace_store_busy()) {
        if(menu.saving==1&&workspace_store_last_success()) {
            menu.batch_dirty=false;
            /* The next save starts from the persisted edit; only ACK updates Main. */
            menu.batch_active_original=menu.batch_active_edited;
        }else if(workspace_store_last_success())menu.user_edit=false;
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
{if(!menu.root)return;if(menu.qr_generation&&menu.qr_generation==lv_qr_popup_generation())lv_qr_popup_hide();menu.qr_generation=0;if(menu.brightness_drag){backlight_service_set(menu.brightness_previous);menu.brightness_drag=false;}workspace_service_cancel_apply();settings_detail_keyboard_hide();settings_detail_dialog_hide();if(menu.quick)lv_obj_del(menu.quick);if(menu.photo_sheet)lv_obj_del(menu.photo_sheet);if(menu.user_sheet)lv_obj_del(menu.user_sheet);if(menu.record_sheet)lv_obj_del(menu.record_sheet);menu.user_sheet=menu.record_sheet=NULL;menu.quick=menu.photo_sheet=NULL;menu.scan_wait=menu.photo_wait=menu.photo_ready=false;clear_notice();lv_timer_pause(menu.timer);lv_obj_add_flag(menu.root,LV_OBJ_FLAG_HIDDEN);}
void ui_page_03_menu_destroy(void)
{ui_page_03_menu_suspend();if(menu.timer)lv_timer_del(menu.timer);if(menu.root)lv_obj_del(menu.root);memset(&menu,0,sizeof(menu));}
void page_03_menu_open_batch(void)
{menu.tab=1;menu.sub=0;menu.dirty=true;ui_manager_push_page(UI_PAGE_MENU);}
