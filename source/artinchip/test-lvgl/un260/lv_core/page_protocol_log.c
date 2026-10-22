#include "page_protocol_log.h"
#include "lv_page_manager.h"
#include "page_01_main.h"
#include "settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/font/manrope_fonts.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/storage/protocol_log.h"
#include <string.h>
static lv_obj_t *empty,*root,*indicator,*toggle,*status,*export_current,*rows[5],*names[5],*sizes[5],*exports[5],*deletes[5];
static uint64_t ids[5],delete_id;
static ui_page_t owner;
static uint32_t revision=~0U;
static int locale=-1;
static void notice(void){ui_notice_post_text(UI_NOTICE_WARNING,"debug.export",UI_N_("Protocol logs"),UI_N_("Log operation unavailable. Wait for storage and try again."));}
void page_protocol_log_close(void){if(root)lv_obj_del(root);root=NULL;settings_detail_dialog_hide();}
static void back(lv_event_t *e){(void)e;page_protocol_log_close();}
static void switch_click(lv_event_t *e){(void)e;protocol_log_snapshot_t s;protocol_log_snapshot(&s);if(!(s.active?protocol_log_stop():protocol_log_start()))notice();}
static void export_click(lv_event_t *e){unsigned i=(uintptr_t)lv_event_get_user_data(e);protocol_log_snapshot_t s;protocol_log_snapshot(&s);if(!protocol_log_export(i<5?ids[i]:s.current))notice();}
static void remove_confirmed(void *context){(void)context;if(!protocol_log_delete(delete_id))notice();}
static void remove_click(lv_event_t *e){unsigned i=(uintptr_t)lv_event_get_user_data(e);if(i>=5)return;delete_id=ids[i];settings_detail_dialog_show_ex(SETTINGS_DIALOG_WARNING,ui_tr("Delete log"),ui_tr("Delete this saved recording?"),ui_tr("Delete"),ui_tr("Cancel"),remove_confirmed,NULL,NULL);}
static const char *state_text(const protocol_log_snapshot_t *s){return s->busy?UI_N_("Saving or exporting. Please wait."):s->active?UI_N_("Recording serial traffic in the background."):s->state==PLOG_ERROR?UI_N_("Log operation failed or recording limit reached. Local logs are retained."):s->state==PLOG_EXPORTED?UI_N_("Log exported to USB. Recording is off."):UI_N_("Recording is off. Saved logs remain available.");}
static void render(void)
{
 if(root)lv_obj_del(root);
 lv_settings_header_t h={.title=ui_tr("Protocol logs"),.subtitle=ui_tr("Keep the latest five recordings. Starting a sixth replaces the oldest."),.back=back};
 lv_settings_frame_t f=lv_settings_frame_create(lv_layer_top(),&h);root=f.root;lv_obj_add_flag(root,LV_OBJ_FLAG_CLICKABLE);
 lv_obj_set_style_bg_opa(f.body,0,0);lv_obj_set_style_border_width(f.body,0,0);
 lv_obj_t *left=lv_settings_panel(f.body,0,0,450,300),*right=lv_settings_panel(f.body,466,0,766,300);
 lv_obj_t *heading=lv_settings_label(left,ui_tr("Background recording"),24,24,&lv_font_instrument_sans_semibold_24,0x1D2B34);lv_obj_set_width(heading,326);lv_label_set_long_mode(heading,LV_LABEL_LONG_DOT);
 toggle=lv_settings_toggle(left,366,26,false,switch_click,NULL);
 lv_settings_align_row(heading,NULL,toggle,12,56,0);
 status=lv_settings_label(left,"",24,88,&lv_font_instrument_sans_medium_16,0x586B78);lv_obj_set_width(status,400);
 lv_obj_t *hint=lv_settings_label(left,ui_tr("You can leave Debug while recording. Exporting the current log stops recording."),24,151,&lv_font_instrument_sans_medium_14,0x586B78);lv_obj_set_width(hint,398);lv_label_set_long_mode(hint,LV_LABEL_LONG_WRAP);
 export_current=lv_settings_button(left,24,244,402,44,ui_tr("Export current log"),true,export_click,(void*)5);
 lv_obj_t *limit=lv_settings_label(left,ui_tr("Recording stops at 16 MB per log."),24,206,&lv_font_instrument_sans_medium_12,0x586B78);lv_obj_set_width(limit,400);
 empty=lv_settings_label(right,ui_tr("No recordings yet"),24,126,&lv_font_instrument_sans_medium_18,0x586B78);
 for(unsigned i=0;i<5;i++){
   rows[i]=lv_settings_box(right,14,8+i*57,738,56,0xFFFFFF);
   names[i]=lv_settings_label(rows[i],"",12,8,&lv_font_instrument_sans_medium_16,0x1D2B34);
   sizes[i]=lv_settings_label(rows[i],"",12,32,&lv_font_instrument_sans_medium_12,0x586B78);
   exports[i]=lv_settings_button(rows[i],416,6,146,44,ui_tr("Export"),false,export_click,(void*)(uintptr_t)i);
   deletes[i]=lv_settings_button(rows[i],578,6,146,44,ui_tr("Delete"),false,remove_click,(void*)(uintptr_t)i);
   if(i<4)lv_settings_box(rows[i],12,55,714,1,0xE3E9ED);
 }
 if(indicator)lv_obj_move_foreground(indicator);
 revision=~0U;
}
void page_protocol_log_open(void)
{
 if(root)return;protocol_log_init();owner=ui_manager_get_current_page();render();page_protocol_log_poll();
}
void page_protocol_log_poll(void)
{
 static uint32_t last;uint32_t now=lv_tick_get();if(now-last<200)return;last=now;
 protocol_log_snapshot_t s;protocol_log_snapshot(&s);
 static protocol_log_state_t previous;
 if(s.state==PLOG_ERROR&&previous!=PLOG_ERROR)ui_notice_post_text(UI_NOTICE_ERROR,"debug.export",UI_N_("Protocol logs"),UI_N_("Log operation failed or recording limit reached. Local logs are retained."));
 previous=s.state;
 if(!indicator&&(s.active||s.busy)){
   /* Status only, in the outer screen gutter. The Debug page owns the entry;
    * recording must never reserve toolbar space or intercept a page's taps. */
   indicator=lv_settings_box(lv_layer_top(),1270,2,8,8,0xD06D45);
   lv_obj_set_style_radius(indicator,LV_RADIUS_CIRCLE,0);
   lv_obj_clear_flag(indicator,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
 }
 if(indicator){if(s.active||s.busy){lv_obj_clear_flag(indicator,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(indicator);}else lv_obj_add_flag(indicator,LV_OBJ_FLAG_HIDDEN);}
 if(root&&ui_manager_get_current_page()!=owner){page_protocol_log_close();return;}
 if(!root)return;
 if(locale!=(int)ui_lang_get()){locale=ui_lang_get();render();}
 if(revision==s.revision)return;revision=s.revision;
 lv_settings_toggle_set(toggle,s.active,false);if(s.busy)lv_obj_add_state(toggle,LV_STATE_DISABLED);else lv_obj_clear_state(toggle,LV_STATE_DISABLED);
 lv_label_set_text(status,ui_tr(state_text(&s)));lv_label_set_long_mode(status,LV_LABEL_LONG_WRAP);
 if(s.active&&!s.busy)lv_obj_clear_state(export_current,LV_STATE_DISABLED);else lv_obj_add_state(export_current,LV_STATE_DISABLED);
 if(s.count)lv_obj_add_flag(empty,LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(empty,LV_OBJ_FLAG_HIDDEN);
 for(unsigned i=0;i<5;i++){
   if(i>=s.count){lv_obj_add_flag(rows[i],LV_OBJ_FLAG_HIDDEN);continue;}lv_obj_clear_flag(rows[i],LV_OBJ_FLAG_HIDDEN);ids[i]=s.files[i].id;
   lv_label_set_text(names[i],s.files[i].name);char line[80];lv_snprintf(line,sizeof(line),"%llu KB%s",(unsigned long long)(s.files[i].bytes/1024),s.active&&ids[i]==s.current?" *":"");lv_label_set_text(sizes[i],line);
   lv_settings_align_row(names[i],sizes[i],exports[i],0,56,3);lv_obj_set_y(deletes[i],lv_obj_get_y(exports[i]));
   if(s.busy||s.active){lv_obj_add_state(exports[i],LV_STATE_DISABLED);lv_obj_add_state(deletes[i],LV_STATE_DISABLED);}else{lv_obj_clear_state(exports[i],LV_STATE_DISABLED);lv_obj_clear_state(deletes[i],LV_STATE_DISABLED);}
 }
}
