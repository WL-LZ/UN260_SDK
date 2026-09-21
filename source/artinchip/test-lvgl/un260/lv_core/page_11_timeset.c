#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_11_timeset.h"
#include "lv_page_manager.h"
#include "settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_system/machine_time.h"
#include "un260/gesture/gesture_service.h"
#include "lv_port_indev.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static lv_settings_frame_t time_frame;
static machine_time_value_t time_original,time_draft;
static lv_obj_t *time_wheels[6],*time_preview,*date_preview,*time_save;
static unsigned wheel_days;
static bool time_home;
static const char *field_names[]={"Year","Month","Day","Hour","Minute","Second"};
static unsigned field_value(unsigned field){
 switch(field){case 0:return time_draft.year;case 1:return time_draft.month;case 2:return time_draft.day;
 case 3:return time_draft.hour;case 4:return time_draft.minute;default:return time_draft.second;}
}
static bool time_dirty(void){
 return time_original.year!=time_draft.year||time_original.month!=time_draft.month||
 time_original.day!=time_draft.day||time_original.hour!=time_draft.hour||
 time_original.minute!=time_draft.minute||time_original.second!=time_draft.second;
}
static unsigned field_limit(unsigned field,bool upper){
 if(!upper)return field<=2?(field==0?2000:1):0;
 if(field==0)return 2099;
 if(field==1)return 12;
 if(field==2){machine_time_value_t end=time_draft;end.day=31;machine_time_normalize(&end);return end.day;}
 return field==3?23:59;
}
static void time_options(unsigned field){
 char options[512];size_t used=0;
 unsigned first=field_limit(field,false),last=field_limit(field,true);
 for(unsigned n=first;n<=last;n++)
  used+=(size_t)snprintf(options+used,sizeof(options)-used,field?"%02u%s":"%04u%s",n,n==last?"":"\n");
 lv_roller_set_options(time_wheels[field],options,LV_ROLLER_MODE_NORMAL);
}
static void time_refresh(void){
 char s[64];
 if(wheel_days!=field_limit(2,true)){
  wheel_days=field_limit(2,true);time_options(2);
 }
 for(unsigned i=0;i<6;i++)
  if(lv_roller_get_selected(time_wheels[i])!=field_value(i)-field_limit(i,false))
   lv_roller_set_selected(time_wheels[i],field_value(i)-field_limit(i,false),LV_ANIM_OFF);
 snprintf(s,sizeof(s),"%02u:%02u:%02u",time_draft.hour,time_draft.minute,time_draft.second);lv_label_set_text(time_preview,s);
 snprintf(s,sizeof(s),"%04u / %02u / %02u",time_draft.year,time_draft.month,time_draft.day);lv_label_set_text(date_preview,s);
 lv_label_set_text(time_frame.message,time_dirty()?"Unsaved changes":"No changes");
 if(time_dirty())lv_obj_clear_state(time_save,LV_STATE_DISABLED);else lv_obj_add_state(time_save,LV_STATE_DISABLED);
}
static void time_wheel_changed(lv_event_t *e){
 unsigned field=(unsigned)(uintptr_t)lv_event_get_user_data(e);
 if(field>=6)return;
 unsigned value=lv_roller_get_selected(lv_event_get_target(e))+field_limit(field,false);
 switch(field){case 0:time_draft.year=value;break;case 1:time_draft.month=value;break;
 case 2:time_draft.day=value;break;case 3:time_draft.hour=value;break;
 case 4:time_draft.minute=value;break;default:time_draft.second=value;}
 machine_time_normalize(&time_draft);time_refresh();
}
static void time_leave(void *data){
 (void)data;
 if(time_home){ui_manager_clear_stack();ui_manager_switch(UI_PAGE_MAIN);}else ui_manager_pop_page();
}
static void time_back(lv_event_t *e){
 (void)e;time_home=false;
 if(time_dirty())settings_detail_dialog_show("Discard changes?","Your changes have not been applied.","Discard","Keep editing",time_leave,NULL,NULL);
 else time_leave(NULL);
}
static bool time_gesture(gesture_action_t action){
 if(settings_detail_overlay_is_open())return true;
 if(action!=GESTURE_ACTION_HOME||!time_dirty())return false;
 time_home=true;settings_detail_dialog_show("Discard changes?","Your changes have not been applied.","Discard","Keep editing",time_leave,NULL,NULL);return true;
}
static void time_cancel(lv_event_t *e){(void)e;time_home=false;time_leave(NULL);}
static void time_apply(lv_event_t *e){
 (void)e;
 if(!time_dirty()||!machine_time_is_valid(&time_draft)||time_draft.year>2099)return;
 /* Existing software clock service; do not invent an RTC write or controller ACK. */
 machine_time_confirm(&time_draft);time_home=false;time_leave(NULL);
}
void ui_page_11_timeset_create(lv_obj_t *parent){
 if(time_frame.root)return;
 machine_time_get(&time_original);time_draft=time_original;time_home=false;
 if(time_draft.year>2099)time_draft.year=2099;
 machine_time_normalize(&time_draft);
 const lv_settings_header_t h={.title="Date & time",.subtitle="Device / Settings",.icon="Clock-active",.back=time_back};
 time_frame=lv_settings_frame_create(parent,&h);lv_obj_t *body=time_frame.body;
 lv_settings_label(body,"Date",20,12,&lv_font_instrument_sans_semibold_14,0x536B79);
 lv_settings_label(body,"Time",450,12,&lv_font_instrument_sans_semibold_14,0x536B79);
 lv_settings_box(body,427,12,1,216,0xE3E9ED);lv_settings_box(body,810,12,1,216,0xE3E9ED);
 for(unsigned i=0;i<6;i++){
  int x=i<3?20+i*132:450+(i-3)*114,w=i<3?122:104;
  lv_settings_label(body,field_names[i],x,40,&lv_font_instrument_sans_medium_14,0x536B79);
  lv_obj_t *wheel=time_wheels[i]=lv_roller_create(body);
  lv_obj_remove_style_all(wheel);lv_obj_set_pos(wheel,x,64);lv_obj_set_width(wheel,w);
  lv_obj_set_style_bg_color(wheel,lv_color_hex(LV_SETTINGS_CONTROL_SURFACE),0);
  lv_obj_set_style_bg_opa(wheel,LV_OPA_COVER,0);lv_obj_set_style_radius(wheel,12,0);
  lv_obj_set_style_text_font(wheel,&lv_font_instrument_sans_medium_22,0);
  lv_obj_set_style_text_color(wheel,lv_color_hex(0x82939F),0);
  lv_obj_set_style_text_line_space(wheel,22,0);
  lv_obj_set_style_text_align(wheel,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_set_style_text_align(wheel,LV_TEXT_ALIGN_CENTER,LV_PART_SELECTED);
  lv_obj_set_style_bg_color(wheel,lv_color_hex(0xFFFFFF),LV_PART_SELECTED);
  lv_obj_set_style_bg_opa(wheel,LV_OPA_COVER,LV_PART_SELECTED);
  lv_obj_set_style_text_color(wheel,lv_color_hex(0x1D2B34),LV_PART_SELECTED);
  lv_obj_set_style_text_font(wheel,&lv_font_instrument_sans_semibold_22,LV_PART_SELECTED);
  lv_obj_set_style_anim_time(wheel,120,0);
  time_options(i);lv_roller_set_visible_row_count(wheel,3);
  lv_port_indev_set_drag_obj(wheel,true);
  lv_obj_add_event_cb(wheel,time_wheel_changed,LV_EVENT_VALUE_CHANGED,(void*)(uintptr_t)i);
 }
 lv_settings_label(body,"Preview",834,12,&lv_font_instrument_sans_semibold_14,0x536B79);
 lv_obj_t *preview=lv_settings_box(body,834,42,376,151,0xF5F7F9);lv_obj_set_style_radius(preview,13,0);
 lv_settings_label(preview,"24-hour",18,14,&lv_font_instrument_sans_medium_14,0x536B79);
 time_preview=lv_settings_label(preview,"",18,43,&lv_font_instrument_sans_semibold_40,0x1D2B34);
 date_preview=lv_settings_label(preview,"",18,104,&lv_font_instrument_sans_medium_18,0x536B79);
 lv_settings_label(body,"Scroll to adjust. Save to apply.",834,207,&lv_font_instrument_sans_medium_12,0x536B79);
 lv_settings_button(time_frame.footer,976,0,116,44,"Cancel",false,time_cancel,NULL);
 time_save=lv_settings_button(time_frame.footer,1102,0,130,44,"Save",true,time_apply,NULL);
 wheel_days=field_limit(2,true);time_refresh();gesture_service_set_page_policy(UI_PAGE_TIMESET,NULL,time_gesture);
}
void ui_page_11_timeset_destroy(void){
 gesture_service_clear_page_policy(UI_PAGE_TIMESET);settings_detail_dialog_hide();
 if(time_frame.root)lv_obj_del(time_frame.root);
 memset(&time_frame,0,sizeof(time_frame));memset(time_wheels,0,sizeof(time_wheels));wheel_days=0;
 time_preview=date_preview=time_save=NULL;
}
