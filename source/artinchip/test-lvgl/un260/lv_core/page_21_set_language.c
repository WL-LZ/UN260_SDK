#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_21_set_language.h"
#include "lv_page_manager.h"
#include "settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/ui_text.h"
#include "un260/gesture/gesture_service.h"
#include <stdint.h>
#include <string.h>
/* Add only languages with verified resources, never preview-only translations. */
static const struct {language_t id;const char *code,*name;} options[]={{LANGUAGE_EN,"EN","English"}};
static lv_settings_frame_t language_frame;
static language_t language_original,language_draft;
static lv_obj_t *language_save,*language_preview;
static lv_obj_t *language_rows[sizeof(options)/sizeof(options[0])];
static lv_obj_t *language_checks[sizeof(options)/sizeof(options[0])];
static bool language_home;
static bool language_dirty(void){return language_original!=language_draft;}
static void language_refresh(void){
 const char *name="Unavailable";
 for(unsigned i=0;i<sizeof(options)/sizeof(options[0]);i++){
  bool selected=language_draft==options[i].id;
  lv_obj_set_style_bg_color(language_rows[i],lv_color_hex(selected?0xEDF4FF:0xF3F6F8),0);
  lv_obj_set_style_border_color(language_rows[i],lv_color_hex(selected?0x8CACDA:0xE3E9ED),0);
  if(selected){lv_obj_clear_flag(language_checks[i],LV_OBJ_FLAG_HIDDEN);name=options[i].name;}
  else lv_obj_add_flag(language_checks[i],LV_OBJ_FLAG_HIDDEN);
 }
 if(language_dirty())settings_detail_action_block(language_save, NULL);else settings_detail_action_block(language_save, "No changes to save.");
 lv_label_set_text(language_frame.message,language_dirty()?"Unsaved changes":"No changes");
 lv_label_set_text(language_preview,name);
}
static void language_leave(void *data){
 (void)data;
 if(language_home){ui_manager_suspend_to_home();}else ui_manager_pop_page();
}
static void language_back(lv_event_t *e){
 (void)e;language_home=false;
 if(language_dirty())settings_detail_dialog_show("Discard changes?","Your changes have not been applied.","Discard","Keep editing",language_leave,NULL,NULL);
 else language_leave(NULL);
}
static bool language_gesture(gesture_action_t action){
 if(settings_detail_overlay_is_open())return true;
 if(action!=GESTURE_ACTION_HOME||!language_dirty())return false;
 language_home=true;settings_detail_dialog_show("Discard changes?","Your changes have not been applied.","Discard","Keep editing",language_leave,NULL,NULL);return true;
}
static void language_cancel(lv_event_t *e){(void)e;language_home=false;language_leave(NULL);}
static void language_apply(lv_event_t *e){
 (void)e;if(!language_dirty())return;
 ui_lang_set(language_draft);ui_manager_invalidate_all_page_caches();language_home=false;language_leave(NULL);
}
static void language_choice(lv_event_t *e){
 unsigned n=(unsigned)(uintptr_t)lv_event_get_user_data(e);
 if(n>=sizeof(options)/sizeof(options[0]))return;
 language_draft=options[n].id;
 language_refresh();
}
void ui_page_21_set_language_create(lv_obj_t *parent){
 if(language_frame.root)return;
 language_original=language_draft=ui_lang_get();language_home=false;
 const lv_settings_header_t h={.title="Language",.subtitle="Device / Settings",.icon="Languages-active",.back=language_back};
 language_frame=lv_settings_frame_create(parent,&h);
 lv_obj_t *body=language_frame.body;
 lv_settings_label(body,"Interface language",20,12,&lv_font_instrument_sans_semibold_14,0x536B79);
 lv_obj_t *list=lv_settings_grid(body,17,40,610,184);lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN);
 for(unsigned i=0;i<sizeof(options)/sizeof(options[0]);i++){
  lv_obj_t *b=lv_settings_button(list,0,0,590,76,"",false,language_choice,(void*)(uintptr_t)i);
  language_rows[i]=b;
  lv_obj_set_style_bg_color(b,lv_color_hex(0xEDF4FF),0);lv_obj_set_style_border_width(b,1,0);
  lv_obj_set_style_border_color(b,lv_color_hex(0x8CACDA),0);lv_obj_set_style_radius(b,13,0);
  lv_obj_t *code=lv_settings_box(b,18,16,44,44,0xFFFFFF);lv_obj_set_style_radius(code,10,0);
  lv_obj_t *label=lv_settings_label(code,options[i].code,0,0,&lv_font_instrument_sans_semibold_18,0x155CBA);lv_obj_center(label);
  lv_settings_label(b,options[i].name,78,25,&lv_font_instrument_sans_semibold_22,0x174F9D);
  language_checks[i]=lv_settings_icon(b,"Check-active",548,26);
 }
 lv_settings_box(body,644,12,1,212,0xE3E9ED);
 lv_settings_label(body,"Preview",666,12,&lv_font_instrument_sans_semibold_14,0x536B79);
 lv_obj_t *sample=lv_settings_box(body,666,42,544,151,0xF5F7F9);lv_obj_set_style_radius(sample,13,0);
 lv_settings_icon(sample,"Settings",18,14);
 lv_settings_label(sample,"Device",52,14,&lv_font_instrument_sans_semibold_20,0x1D2B34);
 lv_settings_box(sample,18,52,508,1,0xE3E9ED);
 lv_settings_label(sample,"Language",18,66,&lv_font_instrument_sans_medium_16,0x1D2B34);
 language_preview=lv_settings_label(sample,"English",390,66,&lv_font_instrument_sans_medium_16,0x536B79);
 lv_settings_box(sample,18,101,508,1,0xE3E9ED);
 lv_settings_label(sample,"Date & time",18,115,&lv_font_instrument_sans_medium_16,0x1D2B34);
 lv_settings_label(sample,"24-hour",390,115,&lv_font_instrument_sans_medium_16,0x536B79);
 lv_settings_label(body,"The interface changes after Save.",666,207,&lv_font_instrument_sans_medium_12,0x536B79);
 lv_label_set_text(language_frame.message,"No changes");
 lv_settings_button(language_frame.footer,976,0,116,44,"Cancel",false,language_cancel,NULL);
 language_save=lv_settings_button(language_frame.footer,1102,0,130,44,"Save",true,language_apply,NULL);
 language_refresh();
 gesture_service_set_page_policy(UI_PAGE_LANGUAGE_SETTING,NULL,language_gesture);
}
void ui_page_21_set_language_destroy(void){
 gesture_service_clear_page_policy(UI_PAGE_LANGUAGE_SETTING);settings_detail_dialog_hide();
 if(language_frame.root)lv_obj_del(language_frame.root);
 memset(&language_frame,0,sizeof(language_frame));language_save=language_preview=NULL;
 memset(language_rows,0,sizeof(language_rows));memset(language_checks,0,sizeof(language_checks));
}
