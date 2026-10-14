#include "un260/font/manrope_fonts.h"
#include "page_collection.h"
#include "page_06_settings.h"
#include "page_01_main.h"
#include "lv_page_manager.h"
#include "settings_detail_ui.h"
#include "un260/app_service/app_collection.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/smart_island/smart_island_activity.h"
#include <string.h>
static lv_obj_t *body,*toggles[2],*mode_label,*status_label,*pcs_label,*end_button,*retry_button,*badge,*badge_all,*badge_reject;
static uint32_t success_id,success_tick;
static const char *mode_title(data_collect_mode_t m){return m==DATA_COLLECT_MODE_ALL?UI_N_("All capture"):m==DATA_COLLECT_MODE_FALSE?UI_N_("False-reject capture"):UI_N_("Not selected");}
static void text(lv_obj_t *label,const char *s){if(label&&strcmp(lv_label_get_text(label),s))lv_label_set_text(label,s);}
static void info(const char *key){settings_detail_dialog_show(ui_tr("Data collection"),ui_tr(key),ui_tr("OK"),NULL,NULL,NULL,NULL);}
static void request_close(void *p){(void)p;(void)app_collection_request(DATA_COLLECT_MODE_NONE);page_collection_refresh();}
static void close_click(lv_event_t *e)
{
 (void)e;
 if(app_command_runtime_count_start_busy()){info(UI_N_("Stop counting before changing collection."));return;}
 data_collection_snapshot_t s;data_collection_snapshot(&s);
 bool incomplete=s.phase==DATA_COLLECTION_WAITING||s.phase==DATA_COLLECTION_COLLECTING||s.phase==DATA_COLLECTION_ERROR;
 settings_detail_dialog_show_ex(incomplete?SETTINGS_DIALOG_WARNING:SETTINGS_DIALOG_INFO,ui_tr("End collection"),
   ui_tr(incomplete?UI_N_("Capture completion is unconfirmed. Disabling now may leave incomplete data."):UI_N_("Disable capture for subsequent counts? Existing data is retained.")),
   ui_tr("End collection"),ui_tr("Cancel"),request_close,NULL,NULL);
}
static void retry(lv_event_t *e){(void)e;data_collection_snapshot_t s;data_collection_snapshot(&s);if(s.pending&&s.uncertain)(void)app_collection_request(s.target);page_collection_refresh();}
static void toggle(lv_event_t *e)
{
 data_collect_mode_t target=(data_collect_mode_t)(uintptr_t)lv_event_get_user_data(e);
 data_collection_snapshot_t s;data_collection_snapshot(&s);
 if(s.mode==target){close_click(e);return;}
 if(!app_collection_request(target))info(UI_N_("Wait for the current collection request to finish."));
 page_collection_refresh();
}
static void main_click(lv_event_t *e){(void)e;ui_manager_clear_stack();ui_manager_switch(UI_PAGE_MAIN);}
static void badge_click(lv_event_t *e){(void)e;if(app_command_runtime_count_start_busy())return;ui_manager_push_page(UI_PAGE_SETTING);page_06_settings_open_collection();}
/* Small banknote + inspection lens, without abbreviations or warning text. */
static lv_obj_t *capture_icon(lv_obj_t *parent,int x,int y,bool rejected,uint32_t color)
{
 lv_obj_t *root=lv_settings_box(parent,x,y,28,28,0);lv_obj_set_style_bg_opa(root,0,0);
 lv_obj_t *note=lv_settings_box(root,2,rejected?4:7,21,14,0);lv_obj_set_style_bg_opa(note,0,0);lv_obj_set_style_border_width(note,2,0);lv_obj_set_style_border_color(note,lv_color_hex(color),0);lv_obj_set_style_radius(note,3,0);
 if(rejected){
   lv_obj_t *lens=lv_settings_box(root,12,12,11,11,0xEDF3F8);lv_obj_set_style_border_width(lens,2,0);lv_obj_set_style_border_color(lens,lv_color_hex(color),0);lv_obj_set_style_radius(lens,LV_RADIUS_CIRCLE,0);
   static const lv_point_t pts[]={{0,0},{5,5}};lv_obj_t *line=lv_line_create(root);lv_line_set_points(line,pts,2);lv_obj_set_pos(line,21,21);lv_obj_set_style_line_width(line,2,0);lv_obj_set_style_line_color(line,lv_color_hex(color),0);
 }else{lv_obj_t *stack=lv_settings_box(root,6,2,18,2,color);lv_obj_set_style_radius(stack,1,0);lv_obj_t *dot=lv_settings_box(note,8,4,4,4,color);lv_obj_set_style_radius(dot,LV_RADIUS_CIRCLE,0);}
 return root;
}
static void deleted(lv_event_t *e){(void)e;body=mode_label=status_label=pcs_label=end_button=retry_button=NULL;toggles[0]=toggles[1]=NULL;}
static void badge_deleted(lv_event_t *e){(void)e;badge=badge_all=badge_reject=NULL;}
void page_collection_main_badge_create(lv_obj_t *parent)
{
 badge=lv_settings_box(parent,755,348,42,44,0xEDF3F8);lv_obj_set_style_radius(badge,12,0);
 lv_obj_set_style_border_width(badge,1,0);lv_obj_set_style_border_color(badge,lv_color_hex(0xCDDBE9),0);lv_obj_add_flag(badge,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_HIDDEN);
 lv_obj_add_event_cb(badge,badge_deleted,LV_EVENT_DELETE,NULL);lv_obj_add_event_cb(badge,badge_click,LV_EVENT_CLICKED,NULL);
 badge_all=capture_icon(badge,7,8,false,0x49799F);badge_reject=capture_icon(badge,7,8,true,0x49799F);
}
void page_collection_create(lv_settings_frame_t *f)
{
 body=f->body;lv_obj_set_style_bg_opa(body,0,0);lv_obj_set_style_border_width(body,0,0);lv_obj_add_event_cb(body,deleted,LV_EVENT_DELETE,NULL);
 lv_obj_t *left=lv_settings_panel(body,0,0,700,300),*right=lv_settings_panel(body,716,0,516,300);
 const char *names[]={UI_N_("All capture"),UI_N_("False-reject capture")};
 const char *hints[]={UI_N_("Collect data for every banknote."),UI_N_("Collect rejected-note data for analysis.")};
 for(unsigned i=0;i<2;i++){
   lv_obj_t *art=lv_settings_box(left,24,26+i*116,64,64,0xF1F5F8);lv_obj_set_style_radius(art,15,0);capture_icon(art,18,18,i,0x49799F);
   lv_obj_t *title=lv_settings_label(left,ui_tr(names[i]),106,0,&lv_font_instrument_sans_medium_22,0x1D2B34);lv_obj_set_width(title,486);lv_label_set_long_mode(title,LV_LABEL_LONG_CLIP);
   lv_obj_t *hint=lv_settings_label(left,ui_tr(hints[i]),106,69+i*116,&lv_font_instrument_sans_medium_14,0x586B78);lv_obj_set_width(hint,486);lv_label_set_long_mode(hint,LV_LABEL_LONG_WRAP);
   toggles[i]=lv_settings_toggle(left,620,0,false,toggle,(void *)(uintptr_t)(i+1));
   lv_settings_align_row(title,hint,toggles[i],i*116,116,10);
   lv_obj_set_y(art,i*116+(116-lv_obj_get_height(art))/2);
 }
 lv_settings_box(left,24,116,652,1,0xE3E9ED);lv_settings_box(left,24,232,652,1,0xE3E9ED);
 lv_obj_t *hint=lv_settings_label(left,ui_tr("Disable the current mode before switching."),24,255,&lv_font_instrument_sans_medium_14,0x586B78);lv_obj_set_width(hint,650);
 lv_settings_label(right,ui_tr("Confirmed mode"),26,22,&lv_font_instrument_sans_medium_14,0x586B78);
 mode_label=lv_settings_label(right,"",26,60,&lv_font_instrument_sans_semibold_24,0x1D2B34);lv_obj_set_width(mode_label,464);lv_label_set_long_mode(mode_label,LV_LABEL_LONG_DOT);
 status_label=lv_settings_label(right,"",26,104,&lv_font_instrument_sans_medium_16,0x586B78);lv_obj_set_width(status_label,464);lv_label_set_long_mode(status_label,LV_LABEL_LONG_WRAP);
 lv_settings_box(right,26,214,464,1,0xE3E9ED);
 lv_obj_t *notes=lv_settings_label(right,ui_tr("Notes"),26,229,&lv_font_instrument_sans_medium_12,0x586B78);
 pcs_label=lv_settings_label(right,"0",26,250,&lv_font_instrument_sans_semibold_24,0x1D2B34);
 end_button=lv_settings_button(right,260,234,228,44,ui_tr("End collection"),false,close_click,NULL);
 lv_settings_align_row(notes,pcs_label,end_button,226,60,4);
 retry_button=lv_settings_button(f->footer,0,0,128,44,ui_tr("Retry"),false,retry,NULL);
 lv_settings_button(f->footer,0,0,190,44,ui_tr("Count on Main"),true,main_click,NULL);
 lv_label_set_text(f->message,ui_tr("Leaving this page keeps capture enabled."));
 page_collection_refresh();
}
void page_collection_refresh(void)
{
 if(!body||!lv_obj_is_visible(body))return;
 data_collection_snapshot_t s;data_collection_snapshot(&s);
 for(unsigned i=0;i<2;i++){lv_settings_toggle_set(toggles[i],s.mode==(data_collect_mode_t)(i+1),false);
   if(s.pending||s.counting||(s.mode&&s.mode!=(data_collect_mode_t)(i+1)))lv_obj_add_state(toggles[i],LV_STATE_DISABLED);else lv_obj_clear_state(toggles[i],LV_STATE_DISABLED);}
 text(mode_label,ui_tr(mode_title(s.mode)));text(status_label,data_collection_state_status());
 char number[16];lv_snprintf(number,sizeof(number),"%u",s.pcs);text(pcs_label,number);
 if(s.pending&&s.uncertain)lv_obj_clear_flag(retry_button,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(retry_button,LV_OBJ_FLAG_HIDDEN);
 if(s.mode||s.pending)lv_obj_clear_flag(end_button,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(end_button,LV_OBJ_FLAG_HIDDEN);
 if(s.counting||(s.pending&&!s.uncertain))lv_obj_add_state(end_button,LV_STATE_DISABLED);else lv_obj_clear_state(end_button,LV_STATE_DISABLED);
}
void page_collection_poll(void)
{
 static uint32_t last;uint32_t now=lv_tick_get();if((uint32_t)(now-last)<100)return;last=now;
 data_collection_snapshot_t s;data_collection_snapshot(&s);
 if(badge){
   if((s.mode||s.pending)&&!s.counting&&smart_island_activity_slot_available())lv_obj_clear_flag(badge,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(badge,LV_OBJ_FLAG_HIDDEN);
   bool all=(s.mode?s.mode:s.target)==DATA_COLLECT_MODE_ALL;
   lv_obj_add_flag(all?badge_reject:badge_all,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(all?badge_all:badge_reject,LV_OBJ_FLAG_HIDDEN);
   lv_obj_set_style_bg_color(badge,lv_color_hex(s.uncertain?0xF5EADB:0xEDF3F8),0);
 }
 si_activity_t activity=SI_ACTIVITY_NONE;const char *title="",*sub=UI_N_("Capture remains enabled");
 if(s.phase==DATA_COLLECTION_COLLECTING||s.phase==DATA_COLLECTION_WAITING){activity=SI_ACTIVITY_WORKING;title=UI_N_("Collecting data");}
 if(s.pending){activity=s.uncertain?SI_ACTIVITY_UNCERTAIN:SI_ACTIVITY_WORKING;title=s.target==DATA_COLLECT_MODE_NONE?(s.uncertain?UI_N_("Disable unconfirmed"):UI_N_("Disabling capture")):(s.uncertain?UI_N_("Enable unconfirmed"):UI_N_("Enabling capture"));sub=UI_N_("Waiting for controller confirmation");}
 if(s.phase==DATA_COLLECTION_ERROR){activity=SI_ACTIVITY_ERROR;title=UI_N_("Capture error");sub=UI_N_("Check USB storage");}
 if(s.phase==DATA_COLLECTION_SUCCESS){
   if(success_id!=s.completed){success_id=s.completed;success_tick=0;}
   if(!success_tick||(uint32_t)(lv_tick_get()-success_tick)<3000){activity=SI_ACTIVITY_SUCCESS;title=UI_N_("Capture complete");}
 }
 if(s.counting)activity=SI_ACTIVITY_NONE;
 bool shown=smart_island_activity_set(activity,ui_tr(title),ui_tr(sub),badge_click);
 if(shown&&activity==SI_ACTIVITY_SUCCESS&&!success_tick)success_tick=lv_tick_get();
}
