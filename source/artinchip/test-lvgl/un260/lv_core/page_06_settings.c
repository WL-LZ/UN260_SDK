#include "page_collection.h"
#include "un260/font/ui_message_font.h"
#include "un260/lv_system/ui_i18n.h"
/* Settings navigation owns layout; device actions remain in their existing services. */
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_06_settings.h"

#include "settings_catalog.h"
#include "un260/app_service/touch_calibration_service.h"
#include "settings_detail_ui.h"
#include "lv_page_manager.h"
#include "lv_page_event.h"
#include "page_09_cis_cala.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_resources/ui_page_background.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/backlight_service.h"
#include "un260/storage/standby_store.h"
#include "un260/machine_state/machine_state.h"
#include "un260/device_info/device_info.h"
#include "un260/lv_system/user_cfg.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define SETTINGS_NODE_MAX 96
static lv_obj_t *settings_page,*sidebar,*view,*grid,*count_label;
static lv_timer_t *settings_timer;
static const settings_node_t *catalog,*scope;
static size_t catalog_count;
static lv_coord_t scroll_positions[SETTINGS_NODE_MAX];
static lv_obj_t *value_labels[SETTINGS_NODE_MAX],*version_value_labels[6];
static void render(void);
static void refresh_values(void);
static bool scope_is(const char *id){return scope&&!strcmp(scope->id,id);}
static bool settings_page_is_visible(void){return settings_page&&lv_obj_is_visible(settings_page);}
static const settings_node_t *find(const char *id){return settings_catalog_find(catalog,catalog_count,id);}
static const settings_node_t *default_scope(void){
 const settings_node_t *preferred=find("device");
 if(preferred&&preferred->kind==SETTINGS_CATEGORY)return preferred;
 for(size_t i=0;i<catalog_count;i++)if(catalog[i].kind==SETTINGS_CATEGORY)return catalog+i;
 return NULL;
}
static void style_plain(lv_obj_t *o){lv_obj_remove_style_all(o);lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scrollbar_mode(o,LV_SCROLLBAR_MODE_OFF);}
static lv_color_t color_primary(void){return lv_color_hex(0x1462CC);}
static lv_color_t color_line(void){return lv_color_hex(0xE3E9ED);}
static lv_color_t color_text(void){return lv_color_hex(0x1D2B34);}
static lv_obj_t *create_label(lv_obj_t *p,const char *t,const lv_font_t *f,lv_color_t c){
 lv_obj_t *l=lv_label_create(p);lv_label_set_text(l,t);lv_obj_set_style_text_font(l,ui_message_font(f),0);lv_obj_set_style_text_color(l,c,0);
 lv_obj_clear_flag(l,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);return l;
}
static void capture_scroll(void){if(grid&&scope)scroll_positions[scope-catalog]=lv_obj_get_scroll_y(grid);}
static void reset_detail_refs(void){
 memset(version_value_labels,0,sizeof(version_value_labels));memset(value_labels,0,sizeof(value_labels));

}
static void back_event_cb(lv_event_t *e){
 (void)e;
 if(page_06_settings_back_sub_page())return;
 ui_manager_clear_stack();ui_manager_switch(UI_PAGE_MAIN);
}
static void home_event_cb(lv_event_t *e){
 (void)e;ui_manager_clear_stack();ui_manager_switch(UI_PAGE_MAIN);
}
static void activate(lv_event_t *e){
 const settings_node_t *n=lv_event_get_user_data(e);
 if(!n||settings_detail_overlay_is_open())return;
 capture_scroll();
 if(n->kind!=SETTINGS_DETAIL||n->page==UI_PAGE_SETTING){scope=n;render();return;}
 if(n->page==UI_PAGE_CIS_CALIB)ui_page_cis_calib_select(!strcmp(n->id,"whiteBalance"));
 ui_manager_push_page((ui_page_t)n->page);
}
static const settings_node_t *category(void){
 const settings_node_t *n=scope;while(n&&n->parent)n=find(n->parent);return n;
}
static void create_sidebar(void){
 sidebar=lv_settings_box(view,12,12,216,376,0xF7F9FB);lv_obj_set_style_radius(sidebar,16,0);
 lv_settings_label(sidebar,ui_tr("Settings"),18,18,&lv_font_instrument_sans_semibold_24,0x1D2B34);
 lv_settings_label(sidebar,"UN260",153,27,&lv_font_instrument_sans_medium_12,0x586B78);
 const settings_node_t *selected=category();
 lv_obj_t *selected_button=NULL;
 lv_obj_t *list=lv_settings_grid(sidebar,4,56,208,314);
 lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN);
 lv_obj_set_style_pad_all(list,4,0);lv_obj_set_style_pad_row(list,4,0);
 for(size_t i=0;i<catalog_count;i++)if(catalog[i].kind==SETTINGS_CATEGORY){
  bool active=selected==catalog+i;
  lv_obj_t *b=lv_settings_button(list,0,0,200,47,"",false,activate,(void*)(catalog+i));
  lv_obj_set_style_border_width(b,0,0);
  lv_obj_set_style_bg_color(b,lv_color_hex(active?0xFFFFFF:0xF7F9FB),0);
  if(catalog[i].icon){
   char icon[48];snprintf(icon,sizeof(icon),"%s%s",catalog[i].icon,active?"-active":"");
   lv_settings_icon(b,icon,12,12);
  }
  int title_x=catalog[i].icon?48:16;
  lv_obj_t *title=lv_settings_label(b,ui_tr(catalog[i].title),title_x,15,&lv_font_instrument_sans_medium_16,active?0x1462CC:0x1D2B34);
  lv_obj_set_width(title,192-title_x);lv_label_set_long_mode(title,LV_LABEL_LONG_DOT);
  if(active){
   selected_button=b;
   lv_obj_set_style_shadow_width(b,8,0);lv_obj_set_style_shadow_opa(b,LV_OPA_10,0);
   lv_obj_set_style_shadow_color(b,lv_color_hex(0x536B79),0);lv_obj_set_style_shadow_ofs_y(b,2,0);
   lv_obj_t *rail=lv_settings_box(b,0,13,3,22,0x1462CC);lv_obj_set_style_radius(rail,2,0);
  }
 }
 if(selected_button)lv_obj_scroll_to_view(selected_button,LV_ANIM_OFF);
 lv_obj_t *home=lv_settings_back(list,0,0,200,47,home_event_cb,NULL);
 lv_obj_set_style_border_width(home,0,0);
 lv_obj_t *home_text=lv_obj_get_child(home,0);
 lv_label_set_text(home_text,ui_tr("Back to count"));
 lv_obj_set_width(home_text,144);lv_obj_set_style_text_align(home_text,LV_TEXT_ALIGN_LEFT,0);
 lv_obj_align(home_text,LV_ALIGN_LEFT_MID,48,0);
 lv_obj_t *home_icon=lv_obj_get_child(home,1);
 if(home_icon)lv_obj_align(home_icon,LV_ALIGN_LEFT_MID,15,0);
}
static void value_for(const settings_node_t *n,char *out,size_t cap){
 out[0]=0;
 if(!strcmp(n->id,"language"))snprintf(out,cap,"%s",ui_lang_current()->name);
 else if(!strcmp(n->id,"time"))snprintf(out,cap,"%s",ui_tr("24-hour"));
 else if(!strcmp(n->id,"brightness")){
  int max=backlight_service_max();
  if(max>0)snprintf(out,cap,"%d%%",backlight_service_level()*100/max);
  else snprintf(out,cap,"%s",ui_tr("Unavailable"));
 }
 else if(!strcmp(n->id,"standby")){unsigned m=standby_config()->minutes;snprintf(out,cap,m?ui_tr("%u min"):ui_tr("Never"),m);}
 else if(!strcmp(n->id,"touch"))snprintf(out,cap,"%s",ui_tr(touch_calibration_calibrated()?touch_calibration_enabled()?"Enabled":"Disabled":"Not calibrated"));
 else if(!strcmp(n->id,"reject"))snprintf(out,cap,ui_tr("%u PCS"),machine_state_reject_pocket_max());
 else if(!strcmp(n->id,"double"))snprintf(out,cap,ui_tr("Level %u"),machine_state_double_note_level());
}
static void refresh_values(void){
 for(size_t i=0;i<catalog_count;i++)if(value_labels[i]){
  char value[48];value_for(catalog+i,value,sizeof(value));
  if(strcmp(lv_label_get_text(value_labels[i]),value))lv_label_set_text(value_labels[i],value);
 }
}
static void create_version_page_content(lv_obj_t *parent){
 static const char *names[]={UI_N_("Controller"),UI_N_("Image board"),UI_N_("Display & logic")};
 static const char *hints[]={UI_N_("Machine control"),UI_N_("Image processing"),UI_N_("Interface and FPGA")};
 static const int indexes[3][2]={{0,3},{1,4},{5,2}};
 lv_obj_set_style_bg_opa(parent,0,0);lv_obj_set_style_border_width(parent,0,0);
 for(int i=0;i<3;i++){
  lv_obj_t *card=lv_settings_panel(parent,i*416,0,400,242);
  lv_settings_label(card,ui_tr(names[i]),22,20,&lv_font_instrument_sans_semibold_22,0x1D2B34);
  lv_settings_label(card,ui_tr(hints[i]),22,49,&lv_font_instrument_sans_medium_14,0x586B78);
  lv_settings_box(card,22,80,356,1,0xE3E9ED);
  for(int j=0;j<2;j++){
   const int index=indexes[i][j],y=98+j*71;
   lv_settings_label(card,j?(i==2?"FPGA":ui_tr("Bootloader")):(i==2?ui_tr("UI software"):ui_tr("Application")),22,y,&lv_font_instrument_sans_medium_14,0x586B78);
   version_value_labels[index]=lv_settings_label(card,ui_tr("Waiting"),22,y+22,&lv_font_instrument_sans_semibold_22,0x1D2B34);
   lv_obj_set_width(version_value_labels[index],356);lv_label_set_long_mode(version_value_labels[index],LV_LABEL_LONG_DOT);
  }
 }
 ui_page_06_settings_refresh_data(UI_DATA_TOPIC_DEVICE_VERSION);
}
void ui_page_06_settings_refresh_data(uint32_t topics){
 if(!settings_page_is_visible())return;
 refresh_values();
 if(!(topics&UI_DATA_TOPIC_DEVICE_VERSION))return;
 bool valid=device_info_is_valid();const char *unknown=ui_text_get(UI_TEXT_SETTINGS_NOT_AVAILABLE);
 const char *values[]={valid?device_info_main_app():unknown,valid?device_info_image_app():unknown,
 valid?device_info_fpga():unknown,valid?device_info_main_boot():unknown,valid?device_info_image_boot():unknown,
 device_info_display_app()[0]?device_info_display_app():unknown};
 for(int i=0;i<6;i++)if(version_value_labels[i]&&strcmp(lv_label_get_text(version_value_labels[i]),values[i]))
  lv_label_set_text(version_value_labels[i],values[i]);
}
void page_06_data_collection_refresh(void){page_collection_refresh();}
void page_06_data_collection_on_reply(data_collection_reply_result_t result){(void)result;page_collection_refresh();}

static void refresh_directory_count(void){
 if(!grid||!count_label)return;
 unsigned visible=0;
 for(uint32_t i=0;i<lv_obj_get_child_cnt(grid);i++){
  lv_obj_t *group=lv_obj_get_child(grid,i);
  if(lv_obj_has_flag(group,LV_OBJ_FLAG_HIDDEN))continue;
  for(uint32_t j=0;j<lv_obj_get_child_cnt(group);j++)
   if(!lv_obj_has_flag(lv_obj_get_child(group,j),LV_OBJ_FLAG_HIDDEN))visible++;
 }
 char text[80];bool more=lv_obj_get_scroll_bottom(grid)>2,above=lv_obj_get_scroll_y(grid)>2;
 snprintf(text,sizeof(text),"%u %s%s",visible,visible==1?ui_tr("setting"):ui_tr("settings"),more?ui_tr("  /  Swipe for more"):above?ui_tr("  /  Swipe to return"):"");
 if(strcmp(lv_label_get_text(count_label),text))lv_label_set_text(count_label,text);
}
static void settings_poll(lv_timer_t *timer){
 (void)timer;if(!settings_page_is_visible())return;
 refresh_directory_count();refresh_values();
 if(scope_is("collection"))page_06_data_collection_refresh();
}
static void render(void){
 if(!settings_page||!scope)return;
 if(ui_manager_get_current_page()==UI_PAGE_SETTING)
  work_mode_service_set_diagnostic(scope_is("collection"));
 grid=sidebar=count_label=NULL;reset_detail_refs();
 if(view)lv_obj_del(view);
 if(scope->kind==SETTINGS_DETAIL){
  lv_settings_header_t h={.title=ui_tr(scope->title),.subtitle=ui_tr("Settings"),.back=back_event_cb};
  lv_settings_frame_t f=lv_settings_frame_create(settings_page,&h);view=f.root;
  if(scope_is("versions")){
   create_version_page_content(f.body);
   lv_label_set_text(f.message,ui_tr("Controller versions are reported by the machine. UI version is local."));
  }
  else if(scope_is("collection")){
   page_collection_create(&f);
  }
  return;
 }
 view=lv_settings_box(settings_page,0,0,1280,400,0);lv_obj_set_style_bg_opa(view,0,0);
 create_sidebar();
 const settings_node_t *cat=category();
 const char *subtitle=scope->kind==SETTINGS_CATEGORY?ui_tr(scope->hint):ui_tr(cat->title);
 lv_settings_header_t h={.title=ui_tr(scope->title),.subtitle=subtitle,.back=back_event_cb};
 lv_settings_header(view,256,18,1000,&h);
 grid=lv_settings_list(view,253,84,1006,274);
 lv_obj_t *groups[SETTINGS_NODE_MAX]={0};
 for(size_t i=0;i<catalog_count;i++)if(catalog[i].parent&&!strcmp(catalog[i].parent,scope->id)){
  const settings_node_t *n=catalog+i;char value[48];value_for(n,value,sizeof(value));
  lv_obj_t *group=NULL;
  if(n->group)for(size_t j=0;j<i;j++)if(groups[j]&&catalog[j].parent&&
    !strcmp(catalog[j].parent,n->parent)&&catalog[j].group&&!strcmp(catalog[j].group,n->group)){group=groups[j];break;}
  if(!group)group=lv_settings_group(grid);
  groups[i]=group;
  lv_settings_item_t cfg={.title=ui_tr(n->title),.hint=n->hint?ui_tr(n->hint):NULL,.value=value,.icon=n->icon,.grouped=true,
   .activate=activate,.user_data=(void*)n,.value_label=&value_labels[i]};
  lv_settings_item(group,&cfg);
 }
 count_label=lv_settings_label(view,"",256,369,&lv_font_instrument_sans_medium_14,0x586B78);
 if(scope_is("calibration"))lv_settings_label(view,ui_tr("Select a calibration to prepare it."),754,369,&lv_font_instrument_sans_medium_14,0x586B78);
 lv_obj_update_layout(grid);lv_obj_scroll_to_y(grid,scroll_positions[scope-catalog],LV_ANIM_OFF);
 refresh_directory_count();
}
void ui_page_06_settings_create(lv_obj_t *parent){
 if(settings_page)return;
 if(!catalog)catalog=settings_catalog(&catalog_count);
 if(!settings_catalog_validate(catalog,catalog_count)||catalog_count>SETTINGS_NODE_MAX)return;
 if(!scope)scope=default_scope();
 settings_page=lv_settings_box(parent?parent:lv_scr_act(),0,0,1280,400,0xF1F4F5);
 ui_page_background_apply(settings_page,UI_BACKGROUND_SETTINGS);render();
 settings_timer=lv_timer_create(settings_poll,250,NULL);
}
bool ui_page_06_settings_resume(void){
 if(!settings_page||!lv_obj_is_valid(settings_page))return false;
 lv_obj_clear_flag(settings_page,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(settings_page);
 page_06_data_collection_refresh();ui_page_06_settings_refresh_data(UI_DATA_TOPIC_DEVICE_VERSION);
 if(settings_timer)lv_timer_resume(settings_timer);
 settings_poll(NULL);
 return true;
}
void ui_page_06_settings_suspend(void){
 capture_scroll();settings_detail_dialog_hide();settings_detail_keyboard_hide();
 if(settings_timer)lv_timer_pause(settings_timer);
 if(settings_page)lv_obj_add_flag(settings_page,LV_OBJ_FLAG_HIDDEN);
}
void ui_page_06_settings_reset_navigation(void){
 memset(scroll_positions,0,sizeof(scroll_positions));scope=default_scope();
 if(settings_page)render();
}
void ui_page_06_settings_destroy(void){
 settings_detail_dialog_hide();settings_detail_keyboard_hide();capture_scroll();
 if(settings_page)lv_obj_del(settings_page);
 if(settings_timer)lv_timer_del(settings_timer);
 settings_timer=NULL;
 settings_page=sidebar=view=grid=count_label=NULL;reset_detail_refs();
}
bool page_06_settings_is_collection(void){
 return scope_is("collection");
}
bool page_06_settings_switch_menu(page_06_settings_menu_t menu){
 static const char *ids[]={"device","maintenance","counting","about","data"};
 if(!settings_page||menu<0||menu>=PAGE_06_SETTINGS_MENU_COUNT)return false;
 capture_scroll();scope=find(ids[menu]);if(!scope)return false;render();return true;
}
bool page_06_settings_back_sub_page(void){
 if(!settings_page||!scope)return false;
 const settings_node_t *parent=scope->parent?find(scope->parent):NULL;
 if(!parent)return false;
 capture_scroll();scope=parent;render();return true;
}

void page_06_settings_open_collection(void){
 if(!catalog)catalog=settings_catalog(&catalog_count);
 const settings_node_t *target=find("collection");if(!target)return;
 capture_scroll();scope=target;if(settings_page)render();
}
