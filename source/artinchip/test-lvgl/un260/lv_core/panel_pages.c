#include "panel_pages.h"
#include "panel_entry.h"
#include "page_02_list.h"
#include "ui_frame_commit.h"
#include "page_05_set_password.h"
#include "page_19_history.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "settings_detail_ui.h"
#include "lv_page_event.h"
#include "un260/lv_components/ui_panel.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/machine_state/machine_fault.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_export_data.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_system/backlight_service.h"
#include "un260/lv_system/app_clock.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/app_service/app_local_setting.h"
#include "un260/app_service/setting_service.h"
#include "un260/app_service/workspace_service.h"
#include "un260/storage/workspace_store.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_serial_query.h"
#include "un260/counting/counting_serial_text.h"
#include "un260/counting/counting_multi_extra.h"
#include "un260/currency/currency_state.h"
#include "un260/currency/currency_metadata.h"
#include "un260/machine_state/machine_state.h"
#include "un260/storage/standby_store.h"
#include "un260/device_info/device_info.h"
#include "un260/gesture/gesture_guide.h"
#include "lv_port_indev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
 A_BACK=1,A_MAIN,A_MENU,A_SET,A_OPERATOR,A_RECEIPT,A_PRINT,A_EXPORT,A_HISTORY,
 A_BATCH,A_MODE,A_SPEED,A_CURRENCY,A_SORT,A_BRIGHTNESS,A_STANDBY,A_SOUND,A_BEEP,
 A_PIN,A_LANGUAGE,A_SEARCH,A_RESET,A_VALUE,A_SERIAL,A_PREV,A_NEXT,A_FILTER_VALUE,
 A_FILTER_SERIAL,A_SERIAL_INPUT,A_SERIAL_CLEAR,A_SERIAL_APPLY,A_SERIAL_CANCEL,
 A_KEY_CLEAR,A_KEY_DELETE,A_COLLECTION,A_VERSIONS,A_PASSWORD_CHANGE,A_FAULT_HELP,
 A_DETECTION,A_CALIBRATION,A_SENSOR,A_MOTOR,A_INTERACTION,A_QUICK_GUIDE,A_CARE,A_FAULT_GUIDE,A_DISPLAY,A_REJECTS,
 A_CATEGORY_BASE=100,A_DENOM_BASE=200,A_MULTI_BASE=300,A_CHAR_BASE=400
};
enum { INK=0x243447,MUTED=0x71869A,LINE=0xD9E3ED,BLUE=0x216BD1 };
enum { LIST_SERIAL, LIST_VALUE, LIST_REJECTS };
typedef struct { lv_obj_t *object; char source[192]; } list_text_t;
typedef struct {
 lv_obj_t *table,*row[6],*filters,*editor,*prev,*next;
 list_text_t title,meta,head[3],cell[6][3],empty,shown,range,draft;
 uint32_t language,filter_hash;
 bool filters_valid;
 lv_point_t outside_point;
 bool outside_armed;
} list_view_t;
typedef struct {
 ui_page_t id;ui_panel_t panel;lv_obj_t *body;lv_timer_t *timer;
 unsigned category,tab,page,choice,input,revision;uint16_t trigger;
 bool dirty,reset,filter,denom_mode,editing,multi,animation,have_fault;
 machine_fault_key_t fault;
 int multi_currency;char code[4],draft[33];
 counting_serial_query_t serial_query,denom_query;
 uint16_t slots[COUNTING_DATA_MAX_ITEMS];unsigned count;
 int denoms[COUNTING_SERIAL_QUERY_MAX_DENOMS];unsigned denom_count;
 unsigned filtered_pcs[COUNTING_DENOM_MAX_ITEMS];
 uint32_t multi_revision,extra_revision,multi_generation;
 uint32_t projected_generation;
 bool reject_pending;
 list_view_t list;
} panel_page_t;
static panel_page_t *pages[3];
static int pending_currency=-1,pending_tab=-1;
static void render(panel_page_t *p);
static void perform(panel_page_t *p,unsigned action);
static int index_of(ui_page_t id)
{return id==UI_PAGE_MENU?0:id==UI_PAGE_LIST?1:id==UI_PAGE_SETTING?2:-1;}
static panel_page_t *page_for(ui_page_t id)
{int i=index_of(id);return i<0?NULL:pages[i];}
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int radius,bool border)
{
 lv_obj_t *o=lv_obj_create(parent);if(!o)return NULL;lv_obj_remove_style_all(o);
 lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
 lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_radius(o,radius,0);
 lv_obj_set_style_border_width(o,border?1:0,0);lv_obj_set_style_border_color(o,lv_color_hex(LINE),0);
 lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);return o;
}
static lv_obj_t *text(lv_obj_t *parent,int x,int y,int w,int h,const char *s,unsigned px,bool bold,uint32_t color,lv_text_align_t align)
{
 lv_obj_t *o=lv_label_create(parent);if(!o)return NULL;lv_obj_remove_style_all(o);
 lv_obj_set_width(o,w);lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);
 lv_obj_set_style_text_font(o,ui_panel_font(px,bold),0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);
 lv_obj_set_style_text_align(o,align,0);lv_obj_set_style_base_dir(o,LV_BASE_DIR_AUTO,0);
 lv_label_set_text(o,s?s:"");lv_obj_update_layout(o);
 lv_obj_set_pos(o,x,y+(h-lv_obj_get_height(o))/2);
 lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);return o;
}
static void click(lv_event_t *e)
{
 if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
 panel_page_t *p=lv_event_get_user_data(e);
 perform(p,(uintptr_t)lv_obj_get_user_data(lv_event_get_target(e)));
}
static lv_obj_t *button(panel_page_t *p,lv_obj_t *parent,int x,int y,int w,int h,const char *s,unsigned action,uint32_t color,unsigned px)
{
 lv_obj_t *o=box(parent,x,y,w,h,color,7,true);if(!o)return NULL;
 lv_obj_add_flag(o,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_user_data(o,(void *)(uintptr_t)action);
 lv_obj_add_event_cb(o,click,LV_EVENT_CLICKED,p);
 if(s)text(o,0,0,w,h,s,px,false,0x507599,LV_TEXT_ALIGN_CENTER);
 return o;
}
static void icon(lv_obj_t *parent,const char *name,int x,int y,unsigned px,uint32_t color)
{lv_obj_t *o=ui_panel_icon_create(parent,name,px,lv_color_hex(color));if(o)lv_obj_set_pos(o,x,y);}
static void comma(char *out,size_t cap,uint64_t value)
{
 char raw[32];snprintf(raw,sizeof(raw),"%llu",(unsigned long long)value);size_t n=strlen(raw),at=0;
 for(size_t i=0;i<n&&at+1<cap;i++){if(i&&(n-i)%3==0&&at+1<cap)out[at++]=',';if(at+1<cap)out[at++]=raw[i];}out[at]=0;
}
static const char *mode_text(void)
{return ui_text_get(machine_state_mode()==MODE_CNT?UI_TEXT_PAGE01_BOTTOM_MODE_CNT:machine_state_mode()==MODE_SDC?UI_TEXT_PAGE01_BOTTOM_MODE_SDC:UI_TEXT_PAGE01_BOTTOM_MODE_MDC);}
static const char *speed_text(void)
{return ui_text_get(machine_state_speed()==0?UI_TEXT_PAGE01_BOTTOM_SPEED_LOW:machine_state_speed()==1?UI_TEXT_PAGE01_BOTTOM_SPEED_MID:UI_TEXT_PAGE01_BOTTOM_SPEED_HIGH);}
static void notice(const char *message)
{ui_notice_post_text(UI_NOTICE_WARNING,"panel.operation",UI_N_("Menu"),message);}
static bool visible(panel_page_t *p)
{return p&&p->panel.root&&lv_obj_is_visible(p->panel.root);}
static void commit(void *context,uint32_t flags)
{(void)flags;panel_page_t *p=context;if(visible(p)){p->dirty=false;render(p);}}
static void refresh(panel_page_t *p,bool reset)
{if(!p)return;p->dirty=true;p->reset|=reset;if(visible(p)&&!ui_frame_commit_defer(commit,p,1))commit(p,1);}
static void entry_cancel(void *context)
{panel_page_t *p=context;if(p)refresh(p,false);}
static bool setting_busy(void)
{return app_command_runtime_count_start_busy()||workspace_service_applying();}
static bool number_confirm(const char *value,void *context)
{
 panel_page_t *p=context;char *end=NULL;unsigned long number=strtoul(value,&end,10);
 if(!*value||!end||*end)return false;
 bool ok=false;
 if(p->input==A_BATCH&&setting_busy()){panel_entry_set_error(ui_tr("Wait for the current operation, then retry."));return false;}
 if(p->input==A_BATCH)ok=setting_service_request_batch_switch(number!=200,(uint8_t)number,machine_state_batch_enabled(),machine_state_batch_num());
 else if(p->input==A_BRIGHTNESS){int max=backlight_service_max(),previous=backlight_service_level();
  if(max>0&&backlight_service_set((int)((number*max+50)/100))){ok=backlight_service_save();if(!ok)backlight_service_set(previous);}}
 else if(p->input==A_STANDBY){standby_config_t cfg=*standby_config();cfg.minutes=(uint16_t)number;ok=standby_store_save(&cfg);}
 if(!ok){panel_entry_set_error(ui_tr("Could not save. Try again."));return false;}
 refresh(p,false);return true;
}
static void open_number(panel_page_t *p,unsigned action)
{
 p->input=action;char initial[16];unsigned min=0,max=60,current=standby_config()->minutes;
 const char *title=UI_N_("Standby delay"),*unit=UI_N_("min"),*range="0–60";
 if(action==A_BATCH){min=1;max=199;current=machine_state_batch_enabled()?machine_state_batch_num():0;title=UI_N_("Batch size");unit=UI_N_("PCS");range="1–199";}
 else if(action==A_BRIGHTNESS){min=10;max=100;int n=backlight_service_max();current=n>0?(backlight_service_level()*100+n/2)/n:0;title=UI_N_("Brightness");unit="%";range="10–100";}
 snprintf(initial,sizeof(initial),"%u",current);
 panel_entry_config_t config={.kind=PANEL_ENTRY_NUMBER,.title=ui_tr("Value setting"),.field_label=ui_tr(title),
  .prompt=ui_tr("Enter a value, then confirm"),.unit=ui_tr(unit),.range=range,.initial=initial,
  .min=min,.max=max,.max_digits=3,.allow_off=action==A_BATCH,.off_value=200,
  .confirm=number_confirm,.cancel=entry_cancel,.user=p};
 if(!panel_entry_open(lv_layer_top(),&config))notice(UI_N_("Unavailable"));
}
static void mode_confirm(void *context)
{unsigned value=(uintptr_t)context;if(setting_busy()){notice(UI_N_("Wait for the current operation, then retry."));return;}if(!setting_service_request_mode((uint8_t)value))notice(UI_N_("Setting not sent. Finish the current operation and try again."));}
static bool choice_confirm(uint32_t value,void *context)
{
 panel_page_t *p=context;
 if(setting_busy()){panel_choice_set_error(ui_tr("Wait for the current operation, then retry."));return false;}
 if(p->choice==A_MODE){
  if(value==machine_state_mode())return true;
  settings_detail_dialog_show(ui_tr("Change count mode?"),ui_tr("The current result is cleared after confirmation. Saved history is unchanged."),ui_tr("Confirm"),ui_tr("Cancel"),mode_confirm,NULL,(void *)(uintptr_t)value);return true;
 }
 if(!setting_service_request_speed((uint8_t)value)){panel_choice_set_error(ui_tr("Setting not sent. Finish the current operation and try again."));return false;}return true;
}
static void open_choice(panel_page_t *p,unsigned action)
{
 p->choice=action;panel_choice_item_t items[3];
 static const unsigned modes[]={MODE_MDC,MODE_SDC,MODE_CNT};
 static const ui_text_id_t mode_ids[]={UI_TEXT_PAGE01_BOTTOM_MODE_MDC,UI_TEXT_PAGE01_BOTTOM_MODE_SDC,UI_TEXT_PAGE01_BOTTOM_MODE_CNT};
 static const ui_text_id_t speed_ids[]={UI_TEXT_PAGE01_BOTTOM_SPEED_LOW,UI_TEXT_PAGE01_BOTTOM_SPEED_MID,UI_TEXT_PAGE01_BOTTOM_SPEED_HIGH};
 for(unsigned i=0;i<3;i++){items[i]=(panel_choice_item_t){.value=action==A_MODE?modes[i]:i,.label=ui_text_get(action==A_MODE?mode_ids[i]:speed_ids[i]),.selected=action==A_MODE?machine_state_mode()==modes[i]:machine_state_speed()==i};}
 panel_choice_config_t cfg={.title=ui_tr(action==A_MODE?"Count mode":"Count speed"),.items=items,.count=3,.confirm=choice_confirm,.cancel=entry_cancel,.user=p};
 panel_choice_open(p->panel.content,&cfg);
}
static void header(panel_page_t *p,const char *title,const char *meta)
{
 lv_obj_t *c=p->body;box(c,28,56,736,1,LINE,0,false);box(c,28,362,736,1,LINE,0,false);
 lv_obj_t *back=lv_nav_back_text_button_create(c,28,7,ui_panel_font(15,false),click,p);
 if(back){lv_obj_set_user_data(back,(void *)A_BACK);lv_obj_set_style_bg_color(back,lv_color_hex(0xE3E9EF),0);}
 text(c,116,0,440,57,title,20,false,INK,LV_TEXT_ALIGN_LEFT);
 text(c,556,0,208,57,meta,12,false,MUTED,LV_TEXT_ALIGN_RIGHT);
}
static void footer(panel_page_t *p,const char *left,const char *right)
{text(p->body,28,363,490,37,left,12,false,MUTED,LV_TEXT_ALIGN_LEFT);text(p->body,518,363,246,37,right,12,false,MUTED,LV_TEXT_ALIGN_RIGHT);}
static ui_panel_key_t key(unsigned action,const char *label,const char *glyph,bool lit)
{return (ui_panel_key_t){.action=action,.label=label,.icon=glyph,.lit=lit,.motion=UI_PANEL_KEY_SWAP};}
static const char *key_text(const char *name)
{
 if(!strcmp(name,"Prefs"))return ui_trc("Panel key","Prefs");
 if(!strcmp(name,"Main"))return ui_trc("Panel key","Main");
 if(!strcmp(name,"Set"))return ui_trc("Panel key","Set");
 if(!strcmp(name,"Operator"))return ui_trc("Panel key","Operator");
 if(!strcmp(name,"Back to Main"))return ui_trc("Panel key","Back to Main");
 if(!strcmp(name,"Device"))return ui_trc("Panel key","Device");
 if(!strcmp(name,"Security"))return ui_trc("Panel key","Security");
 if(!strcmp(name,"Service"))return ui_trc("Panel key","Service");
 if(!strcmp(name,"About"))return ui_trc("Panel key","About");
 if(!strcmp(name,"Standby"))return ui_trc("Panel key","Standby");
 if(!strcmp(name,"Serial"))return ui_trc("Panel key","Serial");
 if(!strcmp(name,"Reset"))return ui_trc("Panel key","Reset");
 if(!strcmp(name,"Rejects"))return ui_trc("Panel key","Rejects");
 if(!strcmp(name,"PIN"))return ui_trc("Panel key","PIN");
 if(!strcmp(name,"Printer"))return ui_trc("Panel key","Printer");
 if(!strcmp(name,"Search"))return ui_trc("Panel key","Search");
 return ui_tr(name);
}
static bool update_fault(panel_page_t *p)
{
 machine_fault_record_t record;bool found=machine_fault_first_unread(&record);
 if(!found)found=machine_fault_at(0,&record);
 bool changed=found!=p->have_fault||(found&&!machine_fault_key_equal(record.key,p->fault));
 p->have_fault=found;if(found)p->fault=record.key;
 if(changed)p->revision++;
 return changed;
}
static void set_keys(panel_page_t *p)
{
 ui_panel_key_t left[6],right[5];char brightness[12],delay[24];
 int maximum=backlight_service_max();snprintf(brightness,sizeof(brightness),"%d%%",maximum>0?(backlight_service_level()*100+maximum/2)/maximum:0);
 if(standby_config()->minutes)snprintf(delay,sizeof(delay),key_text("%u min"),standby_config()->minutes);else snprintf(delay,sizeof(delay),"%s",key_text("Never"));
 if(p->id==UI_PAGE_MENU){
  const char *labels[]={UI_N_("Count"),UI_N_("Output"),UI_N_("Prefs"),UI_N_("Help"),UI_N_("Main"),UI_N_("Set")};
  const char *icons[]={"layers","printer","sliders-horizontal","circle-help","monitor","settings"};
  for(unsigned i=0;i<6;i++)left[i]=key(i<4?A_CATEGORY_BASE+i:i==4?A_MAIN:A_SET,key_text(labels[i]),icons[i],i<4&&p->category==i);
  right[0]=key(A_OPERATOR,key_text("Operator"),"user-round",false);right[1]=key(A_RECEIPT,key_text("Printer"),"file-text",false);
  right[2]=key(A_SET,key_text("Admin"),"lock-keyhole",false);right[3]=key(A_SET,key_text("Set"),"settings",false);
  right[4]=key(A_MAIN,key_text("Back to Main"),"monitor",false);
 }else if(p->id==UI_PAGE_LIST){
  left[0]=key(A_VALUE,key_text("Denom"),"banknote",p->tab==LIST_VALUE);left[1]=key(A_SERIAL,key_text("Serial"),"scan-line",p->tab==LIST_SERIAL);
  left[2]=key(A_SEARCH,key_text("Search"),"search",p->filter);left[2].disabled=p->tab==LIST_REJECTS||(p->multi&&p->multi_currency<0);
  left[3]=key(A_HISTORY,key_text("History"),"history",false);left[4]=key(A_PREV,key_text("Previous"),"chevron-left",false);
  left[5]=key(A_NEXT,key_text("Next"),"chevron-right",false);left[4].disabled=p->page==0;left[5].disabled=(p->page+1)*6>=p->count;
  right[0]=key(A_PRINT,key_text("Print"),"printer",false);right[1]=key(A_EXPORT,key_text("Export"),"usb",false);
  right[2]=key(A_RESET,key_text("Reset"),"rotate-ccw",false);right[3]=key(A_REJECTS,key_text("Rejects"),"circle-alert",p->tab==LIST_REJECTS);
  right[4]=key(A_MAIN,key_text("Back to Main"),"monitor",false);
 }else{
  const char *labels[]={UI_N_("Device"),UI_N_("Count"),UI_N_("Service"),UI_N_("Data"),UI_N_("About"),UI_N_("Security")};
  const char *icons[]={"monitor","banknote","wrench","database","info","shield-check"};
  for(unsigned i=0;i<6;i++)left[i]=key(A_CATEGORY_BASE+i,key_text(labels[i]),icons[i],p->category==i);
  right[0]=key(A_BRIGHTNESS,key_text("Brightness"),"sun",false);right[0].sub=brightness;
  right[1]=key(A_STANDBY,key_text("Standby"),"clock-3",false);right[1].sub=delay;
  right[2]=key(A_SOUND,key_text("Sound"),"volume-2",user_cfg_touch_sound_enabled());right[2].value=user_cfg_touch_sound_enabled();
  right[3]=key(A_PIN,key_text("PIN"),"lock-keyhole",false);right[4]=key(A_MENU,key_text("Back to Menu"),"menu",false);
 }
 if(p->have_fault)right[0]=key(A_FAULT_GUIDE,key_text("Guide"),"circle-help",false);
 right[4].primary=true;
 ui_panel_set_keys(&p->panel,UI_PANEL_LAYOUT_STANDARD,left,6,right,5,p->revision,p->trigger);
}
static void set_motion(panel_page_t *p)
{
 ui_panel_motion_t m={.revision=p->revision,.tone=UI_PANEL_TONE_NEUTRAL};char detail[60];
 if(p->id==UI_PAGE_LIST){m.scene=UI_PANEL_SCENE_SCAN;m.title=ui_tr(p->filter?"Record search":"Count records");snprintf(detail,sizeof(detail),ui_tr("%u records"),p->count);m.detail=detail;m.value=1000;}
 else if(p->id==UI_PAGE_MENU){const char *names[]={UI_N_("Counting"),UI_N_("Output"),UI_N_("Preferences"),UI_N_("Help")};
  const ui_panel_scene_t scenes[]={UI_PANEL_SCENE_STACK,UI_PANEL_SCENE_PRINT,UI_PANEL_SCENE_SLIDERS,UI_PANEL_SCENE_CHECK};m.scene=scenes[p->category];m.title=ui_tr(names[p->category]);m.detail=ui_tr("Choose an option");m.value=p->category==0?(machine_state_batch_enabled()?LV_MIN(1000,machine_state_batch_num()*1000U/199U):0):p->category==1||p->category==3?1000:0;}
 else{const char *names[]={UI_N_("Device settings"),UI_N_("Counting setup"),UI_N_("Maintenance"),UI_N_("Data management"),UI_N_("Device info"),UI_N_("Security")};
  const ui_panel_scene_t scenes[]={UI_PANEL_SCENE_SLIDERS,UI_PANEL_SCENE_STACK,UI_PANEL_SCENE_SLIDERS,UI_PANEL_SCENE_SCAN,UI_PANEL_SCENE_GRID,UI_PANEL_SCENE_LOCK};
  m.scene=scenes[p->category];m.title=ui_tr(names[p->category]);m.detail=ui_tr("Choose a setting");m.pin_digits=p->category==5?4:0;m.value=p->category==1?500:0;}
 if(p->trigger==A_SOUND||p->trigger==A_BEEP){m.scene=UI_PANEL_SCENE_SOUND;m.title=ui_tr("Sound");bool on=p->trigger==A_SOUND?user_cfg_touch_sound_enabled():machine_state_buzzer_enabled();m.detail=ui_tr(on?"On":"Off");m.value=on?1000:0;}
 if(p->have_fault){m.scene=UI_PANEL_SCENE_JAM;m.title=ui_tr("Error");m.detail=ui_tr("Open fault guide");m.value=0;m.running=false;m.tone=UI_PANEL_TONE_WARNING;}
 ui_panel_set_animation_enabled(&p->panel,p->animation);ui_panel_set_motion(&p->panel,&m);
}
static void menu_row(panel_page_t *p,unsigned i,const char *name,const char *glyph,const char *value,unsigned action)
{
 int y=72+(int)i*287/4,h=(i==3?350:y+63)-y;
 lv_obj_t *row=button(p,p->body,28,y,736,h,NULL,action,0xFFFFFF,17);if(!row)return;
 lv_obj_set_style_radius(row,11,0);lv_obj_t *tile=box(row,18,(h-35)/2,35,35,0xEDF3F9,9,false);
 icon(tile,glyph,7,7,21,0x557DA1);text(row,69,0,420,h,ui_tr(name),17,false,INK,LV_TEXT_ALIGN_LEFT);
 text(row,489,0,200,h,value,13,false,0x597B9B,LV_TEXT_ALIGN_RIGHT);icon(row,"chevron-right",703,(h-16)/2,16,0x91A4B5);
}
static void menu_render(panel_page_t *p)
{
 const char *cats[]={UI_N_("Counting"),UI_N_("Output"),UI_N_("Preferences"),UI_N_("Help")};header(p,ui_tr("Menu"),ui_tr(cats[p->category]));
 char batch[12];snprintf(batch,sizeof(batch),"%u",machine_state_batch_num());
 if(p->category==0){menu_row(p,0,UI_N_("Batch size"),"banknote",machine_state_batch_enabled()?batch:ui_tr("Off"),A_BATCH);menu_row(p,1,UI_N_("Count mode"),"layers",mode_text(),A_MODE);menu_row(p,2,UI_N_("Count speed"),"gauge",speed_text(),A_SPEED);menu_row(p,3,UI_N_("Detection"),"shield-check","",A_DETECTION);}
 else if(p->category==1){menu_row(p,0,UI_N_("Print report"),"printer","",A_PRINT);menu_row(p,1,UI_N_("Print settings"),"file-text","",A_RECEIPT);menu_row(p,2,UI_N_("Export records"),"usb","",A_EXPORT);menu_row(p,3,UI_N_("Current records"),"list","",A_SERIAL);}
 else if(p->category==2){menu_row(p,0,UI_N_("Operator"),"user-round","",A_OPERATOR);menu_row(p,1,UI_N_("Interaction"),"touchpad","",A_INTERACTION);menu_row(p,2,UI_N_("Display & sound"),"volume-2","",A_DISPLAY);menu_row(p,3,UI_N_("Standby"),"clock-3","",A_STANDBY);}
 else{menu_row(p,0,UI_N_("Gesture guide"),"circle-help","",A_QUICK_GUIDE);menu_row(p,1,UI_N_("Troubleshooting"),"wrench","",A_FAULT_HELP);menu_row(p,2,UI_N_("Device information"),"info","",A_VERSIONS);menu_row(p,3,UI_N_("Care"),"shield","",A_CARE);}
 const workspace_user_t *actor=workspace_active(workspace_store_get());char actor_text[96];
 snprintf(actor_text,sizeof(actor_text),"%s %02u",ui_tr("Operator"),actor?actor->id:0);
 footer(p,ui_tr("Select an item"),actor_text);
}
static void setting_row(panel_page_t *p,lv_obj_t *card,unsigned i,unsigned count,const char *name,const char *glyph,const char *value,unsigned action,int toggle)
{
 int top=(int)i*276/(int)count,bottom=(int)(i+1)*276/(int)count,height=bottom-top;
 lv_obj_t *row=box(card,18,top,698,height,0xFFFFFF,0,false);if(!row)return;
 if(i+1<count)box(row,0,height-1,698,1,0xE5EBF1,0,false);
 icon(row,glyph,0,(height-21)/2,21,0x6A88A4);text(row,34,0,445,height,ui_tr(name),16,false,0x354F66,LV_TEXT_ALIGN_LEFT);
 if(toggle>=0){lv_obj_t *b=button(p,row,659,(height-23)/2,39,23,NULL,action,toggle?0x347BCC:0xCAD4DF,13);lv_obj_set_style_border_width(b,0,0);lv_obj_set_style_radius(b,12,0);box(b,toggle?19:3,3,17,17,0xFFFFFF,9,false);}
 else if(action){lv_point_t size;lv_txt_get_size(&size,value?value:"",ui_panel_font(13,false),0,0,250,LV_TEXT_FLAG_NONE);int w=LV_MIN(230,LV_MAX(74,size.x+45));
  lv_obj_t *b=button(p,row,698-w,(height-32)/2,w,32,NULL,action,0xEEF3F8,13);text(b,11,0,w-36,32,value,13,false,0x507599,LV_TEXT_ALIGN_CENTER);icon(b,"chevron-right",w-23,9,14,0x8AA0B5);}
 else text(row,470,0,228,height,value,16,false,INK,LV_TEXT_ALIGN_RIGHT);
}
static void settings_render(panel_page_t *p)
{
 const char *titles[]={UI_N_("Device settings"),UI_N_("Counting setup"),UI_N_("Maintenance"),UI_N_("Data management"),UI_N_("About"),UI_N_("Security")};
 header(p,ui_tr(titles[p->category]),ui_tr("Management"));lv_obj_t *card=box(p->body,28,72,736,278,0xFFFFFF,13,true);if(!card)return;
 char brightness[16],delay[24],batch[12],code[4];int max=backlight_service_max();snprintf(brightness,sizeof(brightness),"%d%%",max>0?(backlight_service_level()*100+max/2)/max:0);
 snprintf(batch,sizeof(batch),"%u",machine_state_batch_num());currency_state_get_selected_code(code);
 if(standby_config()->minutes)snprintf(delay,sizeof(delay),ui_tr("%u min"),standby_config()->minutes);else snprintf(delay,sizeof(delay),"%s",ui_tr("Never"));
 if(p->category==0){setting_row(p,card,0,5,UI_N_("Brightness"),"sun",brightness,A_BRIGHTNESS,-1);setting_row(p,card,1,5,UI_N_("Language"),"globe",ui_lang_current()->name,A_LANGUAGE,-1);setting_row(p,card,2,5,UI_N_("Standby delay"),"clock-3",delay,A_STANDBY,-1);setting_row(p,card,3,5,UI_N_("Machine sound"),"volume-2","",A_BEEP,machine_state_buzzer_enabled());setting_row(p,card,4,5,UI_N_("Touch sound"),"touchpad","",A_SOUND,user_cfg_touch_sound_enabled());}
 else if(p->category==1){setting_row(p,card,0,5,UI_N_("Count mode"),"layers",mode_text(),A_MODE,-1);setting_row(p,card,1,5,UI_N_("Count speed"),"gauge",speed_text(),A_SPEED,-1);setting_row(p,card,2,5,UI_N_("Batch size"),"banknote",machine_state_batch_enabled()?batch:ui_tr("Off"),A_BATCH,-1);setting_row(p,card,3,5,UI_N_("Currency"),"globe",currency_state_display_code(code),A_CURRENCY,-1);setting_row(p,card,4,5,UI_N_("Sort"),"scan-line",ui_tr(machine_state_fo_mode()?"On":"Off"),A_SORT,-1);}
 else if(p->category==2){setting_row(p,card,0,4,UI_N_("Calibration"),"wrench",ui_tr("View"),A_CALIBRATION,-1);setting_row(p,card,1,4,UI_N_("Sensors"),"scan-line",ui_tr("View"),A_SENSOR,-1);setting_row(p,card,2,4,UI_N_("Motor test"),"shield-check",ui_tr("View"),A_MOTOR,-1);setting_row(p,card,3,4,UI_N_("Version"),"usb",ui_tr("View"),A_VERSIONS,-1);}
 else if(p->category==3){setting_row(p,card,0,4,UI_N_("Current records"),"list",ui_tr("View"),A_SERIAL,-1);setting_row(p,card,1,4,UI_N_("History"),"history",ui_tr("View"),A_HISTORY,-1);setting_row(p,card,2,4,UI_N_("Export"),"usb","USB",A_EXPORT,-1);setting_row(p,card,3,4,UI_N_("Print report"),"printer",ui_tr("Print"),A_PRINT,-1);}
 else if(p->category==4){setting_row(p,card,0,4,UI_N_("Model"),"monitor","UN260",0,-1);setting_row(p,card,1,4,UI_N_("Screen"),"monitor","1280 × 400",0,-1);setting_row(p,card,2,4,UI_N_("Display"),"monitor","792 × 400",0,-1);setting_row(p,card,3,4,UI_N_("Version"),"info",device_info_display_app(),A_VERSIONS,-1);}
 else{setting_row(p,card,0,4,UI_N_("Admin PIN"),"lock-keyhole",ui_tr("Verify PIN"),A_PIN,-1);setting_row(p,card,1,4,UI_N_("PIN length"),"shield","4",0,-1);setting_row(p,card,2,4,UI_N_("Change password"),"lock-keyhole",ui_tr("Change"),A_PASSWORD_CHANGE,-1);setting_row(p,card,3,4,UI_N_("Device information"),"info",ui_tr("View"),A_VERSIONS,-1);}
 footer(p,ui_tr("Select a category"),"");
}
static bool denom_selected(const panel_page_t *p,int value)
{for(unsigned i=0;i<p->denom_query.denomination_count;i++)if(p->denom_query.denominations[i]==value)return true;return false;}
static void reject_request(panel_page_t *p)
{
 if(!p->multi||p->tab!=LIST_REJECTS||!p->reject_pending)return;
 if(app_command_runtime_count_start_busy()||counting_multi_current()->counting||
    counting_multi_query_busy()||counting_multi_extra_busy())return;
 p->reject_pending=false;
 (void)counting_multi_reject_request(app_clock_uptime_ms());
}
static void detail_request(panel_page_t *p)
{
 if(!p->multi||p->tab==LIST_REJECTS||p->multi_currency<0)return;
 const counting_multi_t *m=counting_multi_current();unsigned index=(unsigned)p->multi_currency;
 if(index>=m->count||m->counting||app_command_runtime_count_start_busy()||counting_multi_query_busy()||counting_multi_extra_busy())return;
 bool serial_needed=p->tab==LIST_SERIAL||(!p->denom_mode&&(p->filter||p->serial_query.text[0]));
 bool values_needed=p->tab==LIST_VALUE||(p->filter&&p->denom_mode);
 /* NONE stays queued while either service owns the shared transport. Failure
  * and timeout remain visible until a new result, never a polling retry loop. */
 if(values_needed&&m->currencies[index].status==MULTI_DETAIL_NONE){(void)counting_multi_request(index,app_clock_uptime_ms());return;}
 const multi_serial_cache_t *serials=counting_multi_serials(index);
 if(serial_needed&&serials&&serials->status==MULTI_DETAIL_NONE)(void)counting_multi_serial_request(index,app_clock_uptime_ms());
}
static void list_project(panel_page_t *p)
{
 const counting_sim_t *d=counting_data_current();p->multi=currency_state_multi_selected();char code[4];
 if(pending_tab>=0){p->multi_currency=pending_currency;p->tab=(unsigned)pending_tab;
  p->reject_pending=p->tab==LIST_REJECTS;pending_currency=pending_tab=-1;}
 const counting_multi_t *current_multi=counting_multi_current();
 if(p->multi&&p->multi_currency>=(int)current_multi->count)p->multi_currency=-1;
 if(p->multi)snprintf(code,sizeof(code),"%s",p->multi_currency<0?"MUL":current_multi->currencies[p->multi_currency].code);
 else currency_state_get_effective_code(code);
 if(strcmp(code,p->code)||(p->multi&&p->projected_generation!=current_multi->generation)){
  memcpy(p->code,code,4);memset(&p->serial_query,0,sizeof(p->serial_query));memset(&p->denom_query,0,sizeof(p->denom_query));p->page=0;p->draft[0]=0;p->editing=false;p->denom_mode=true;
  if(p->multi&&p->tab==LIST_REJECTS)p->reject_pending=true;
 }
 p->projected_generation=current_multi->generation;
 p->count=p->denom_count=0;
 reject_request(p);
 detail_request(p);
 if(p->tab==LIST_REJECTS){
  if(p->multi){const multi_reject_cache_t *rejects=counting_multi_rejects();
   if(rejects&&rejects->status==MULTI_DETAIL_READY)p->count=LV_MIN(rejects->count,254);}
  else if(d)p->count=LV_MIN(counting_data_error_detail_count(d),COUNTING_DATA_MAX_ITEMS);
  for(unsigned i=0;i<p->count;i++)p->slots[i]=i;
 }else if(p->multi){const counting_multi_t *m=counting_multi_current();if(p->multi_currency>=(int)m->count)p->multi_currency=-1;
  if(p->multi_currency<0){p->count=m->count;}
  else{const multi_currency_t *c=&m->currencies[p->multi_currency];
   if(p->tab==LIST_VALUE&&c->status==MULTI_DETAIL_READY){
    bool serial_filter=!p->denom_mode&&p->serial_query.text[0];
    memset(p->filtered_pcs,0,sizeof(p->filtered_pcs));
    if(serial_filter){const multi_serial_cache_t *serials=counting_multi_serials((unsigned)p->multi_currency);
     if(serials&&serials->status==MULTI_DETAIL_READY)for(unsigned row=0;row<serials->count;row++)
      if(counting_serial_text_matches(serials->rows[row].text,p->serial_query.text,strlen(p->serial_query.text),COUNTING_SERIAL_TEXT_CONTAINS))
       for(unsigned i=0;i<c->denom_count&&i<COUNTING_DENOM_MAX_ITEMS;i++)if(c->denom[i].value==serials->rows[row].value){p->filtered_pcs[i]++;break;}}
    for(unsigned i=0;i<c->denom_count&&i<COUNTING_DENOM_MAX_ITEMS;i++){
     if(!serial_filter)p->filtered_pcs[i]=c->denom[i].pcs;
     if((!p->denom_mode||!p->denom_query.denomination_count||denom_selected(p,(int)c->denom[i].value))&&(!serial_filter||p->filtered_pcs[i]))p->slots[p->count++]=i;}
   }
   else if(p->tab==LIST_SERIAL){const multi_serial_cache_t *s=counting_multi_serials((unsigned)p->multi_currency);if(s&&s->status==MULTI_DETAIL_READY)for(unsigned i=0;i<s->count;i++){
    bool match=p->denom_mode?(!p->denom_query.denomination_count||denom_selected(p,(int)s->rows[i].value)):counting_serial_text_matches(s->rows[i].text,p->serial_query.text,strlen(p->serial_query.text),COUNTING_SERIAL_TEXT_CONTAINS);
    if(match)p->slots[p->count++]=i;}}
   if(c->status==MULTI_DETAIL_READY)for(unsigned i=0;i<c->denom_count&&p->denom_count<COUNTING_SERIAL_QUERY_MAX_DENOMS;i++)if(c->denom[i].value)p->denoms[p->denom_count++]=(int)c->denom[i].value;
  }
 }else if(d){p->denom_count=(unsigned)LV_MIN(COUNTING_SERIAL_QUERY_MAX_DENOMS,counting_serial_query_denominations(d,p->denoms,COUNTING_SERIAL_QUERY_MAX_DENOMS));
  if(p->tab==LIST_SERIAL){counting_serial_query_result_t r=counting_serial_query_build(d,p->denom_mode?&p->denom_query:&p->serial_query,p->slots,COUNTING_DATA_MAX_ITEMS);p->count=r.written_count;}
  else if(counting_data_monetary_result_supported(d)){
   memset(p->filtered_pcs,0,sizeof(p->filtered_pcs));
   bool serial_filter=!p->denom_mode&&p->serial_query.text[0];
   if(serial_filter){counting_serial_query_result_t result=counting_serial_query_build(d,&p->serial_query,p->slots,COUNTING_DATA_MAX_ITEMS);
    for(unsigned slot=0;slot<result.written_count;slot++)for(unsigned i=0;i<d->denom_number&&i<COUNTING_DENOM_MAX_ITEMS;i++)
     if(d->denom[i].value==d->denom_mix[p->slots[slot]]){p->filtered_pcs[i]++;break;}}
   for(unsigned i=0;i<d->denom_number&&i<COUNTING_DENOM_MAX_ITEMS;i++){
    if(!serial_filter)p->filtered_pcs[i]=d->denom[i].pcs;
    if(d->denom[i].value>0&&(!p->denom_mode||!p->denom_query.denomination_count||denom_selected(p,d->denom[i].value))&&(!serial_filter||p->filtered_pcs[i]))p->slots[p->count++]=i;}
  }
 }
 if(p->reset){p->page=0;p->reset=false;}if(p->count&&p->page*6>=p->count)p->page=(p->count-1)/6;if(!p->count)p->page=0;
}
static void list_text_make(list_text_t *t,lv_obj_t *parent,int x,int y,int w,int h,unsigned px,uint32_t color,lv_text_align_t align)
{t->object=text(parent,x,y,w,h,"",px,false,color,align);t->source[0]=0;}
static void list_text_update(list_text_t *t,const char *s,int x,int y,int w,int h,unsigned px,uint32_t color,lv_text_align_t align)
{
 if(!t->object)return;
 if(strcmp(t->source,s)){snprintf(t->source,sizeof(t->source),"%s",s);lv_label_set_text(t->object,t->source);}
 lv_obj_set_width(t->object,w);lv_obj_set_style_text_font(t->object,ui_panel_font(px,false),0);
 lv_obj_set_style_text_color(t->object,lv_color_hex(color),0);lv_obj_set_style_text_align(t->object,align,0);
 lv_obj_update_layout(t->object);lv_obj_set_pos(t->object,x,y+(h-lv_obj_get_height(t->object))/2);
}
static void list_filters(panel_page_t *p)
{
 /* A separate subtree changes only when the available filter controls change. */
 uint32_t hash=2166136261U;
 #define FILTER_HASH(v) do{hash^=(uint32_t)(v);hash*=16777619U;}while(0)
 FILTER_HASH(p->filter);FILTER_HASH(p->denom_mode);FILTER_HASH(p->tab);FILTER_HASH(p->multi_currency);
 for(unsigned i=0;i<p->denom_count;i++){FILTER_HASH(p->denoms[i]);FILTER_HASH(denom_selected(p,p->denoms[i]));}
 for(const unsigned char *s=(const unsigned char *)p->serial_query.text;*s;s++)FILTER_HASH(*s);
 #undef FILTER_HASH
 if(p->list.filters_valid&&p->list.filter_hash==hash)return;
 p->list.filter_hash=hash;p->list.filters_valid=true;
 if(p->list.filters)lv_obj_del(p->list.filters);
 lv_obj_t *c=p->list.filters=box(p->body,0,0,792,400,0,0,false);lv_obj_set_style_bg_opa(c,LV_OPA_TRANSP,0);
 if(p->tab==LIST_REJECTS){text(c,28,70,736,36,ui_tr("Rejects"),14,false,MUTED,LV_TEXT_ALIGN_LEFT);return;}
 lv_obj_t *base=box(c,28,70,143,36,0xE4EAF0,8,true);if(!base)return;
 bool selected=p->filter?p->denom_mode:p->tab==LIST_SERIAL;
 button(p,base,3,3,59,29,ui_tr(p->filter?"Denom":"SN"),p->filter?A_FILTER_VALUE:A_SERIAL,selected?0xFFFFFF:0xE4EAF0,12);
 button(p,base,65,3,74,29,ui_tr(p->filter?"SN":"Denom"),p->filter?A_FILTER_SERIAL:A_VALUE,!selected?0xFFFFFF:0xE4EAF0,12);
 if(!p->filter){text(c,465,70,299,36,ui_tr("Original records"),11,false,MUTED,LV_TEXT_ALIGN_RIGHT);return;}
 if(p->denom_mode){lv_obj_t *strip=box(c,185,70,579,36,0xF3F5F8,0,false);lv_obj_add_flag(strip,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(strip,LV_DIR_HOR);lv_obj_set_scrollbar_mode(strip,LV_SCROLLBAR_MODE_AUTO);
  int x=0;for(unsigned i=0;i<p->denom_count;i++){char s[20];snprintf(s,sizeof(s),"%d",p->denoms[i]);int w=LV_MAX(42,(int)strlen(s)*8+22);button(p,strip,x,2,w,32,s,A_DENOM_BASE+i,denom_selected(p,p->denoms[i])?0xE0EDFA:0xFFFFFF,12);x+=w+5;}}
 else{button(p,c,420,72,270,32,p->serial_query.text[0]?p->serial_query.text:ui_tr("Enter serial"),A_SERIAL_INPUT,0xFFFFFF,14);button(p,c,700,72,64,32,ui_tr("Clear"),A_SERIAL_CLEAR,0xFFFFFF,12);}
}
static void editor_outside(lv_event_t *event)
{
 panel_page_t *p=lv_event_get_user_data(event);
 if(!p->editing||lv_event_get_target(event)!=p->list.editor)return;
 lv_event_code_t code=lv_event_get_code(event);lv_indev_t *indev=lv_indev_get_act();
 lv_point_t point={0};if(indev)lv_indev_get_point(indev,&point);
 if(code==LV_EVENT_PRESSED){p->list.outside_point=point;p->list.outside_armed=lv_port_indev_touch_count()<=1;return;}
 int dx=point.x-p->list.outside_point.x,dy=point.y-p->list.outside_point.y;
 if(code==LV_EVENT_PRESS_LOST||code==LV_EVENT_LONG_PRESSED||dx*dx+dy*dy>100||lv_port_indev_touch_count()>1)p->list.outside_armed=false;
 if(code!=LV_EVENT_RELEASED)return;
 bool accepted=p->list.outside_armed&&(!indev||(!lv_indev_get_scroll_obj(indev)&&lv_indev_get_gesture_dir(indev)==LV_DIR_NONE));
 p->list.outside_armed=false;
 if(accepted){p->editing=false;refresh(p,false);}
}
static void text_editor(panel_page_t *p)
{
 if(!p->editing){if(p->list.editor)lv_obj_add_flag(p->list.editor,LV_OBJ_FLAG_HIDDEN);return;}
 if(!p->list.editor){
  /* Sibling of the projection: live result updates never destroy an active key. */
  lv_obj_t *backdrop=p->list.editor=box(p->panel.content,0,0,792,400,0,0,false);
  lv_obj_set_style_bg_opa(backdrop,LV_OPA_TRANSP,0);lv_obj_add_flag(backdrop,LV_OBJ_FLAG_CLICKABLE);
  const lv_event_code_t events[]={LV_EVENT_PRESSED,LV_EVENT_PRESSING,LV_EVENT_PRESS_LOST,LV_EVENT_LONG_PRESSED,LV_EVENT_RELEASED};
  for(unsigned i=0;i<sizeof(events)/sizeof(events[0]);i++)lv_obj_add_event_cb(backdrop,editor_outside,events[i],p);
  lv_obj_t *sheet=box(backdrop,28,70,736,280,0xFFFFFF,13,true);lv_obj_add_flag(sheet,LV_OBJ_FLAG_CLICKABLE);
  list_text_make(&p->list.draft,sheet,18,8,430,34,20,INK,LV_TEXT_ALIGN_LEFT);
  button(p,sheet,501,8,100,34,ui_tr("Search"),A_SERIAL_APPLY,0xE0EDFA,14);
  button(p,sheet,610,8,108,34,ui_tr("Cancel"),A_SERIAL_CANCEL,0xEEF3F8,14);
  static const char chars[]="1234567890QWERTYUIOPASDFGHJKLZXCVBNM";
  for(unsigned i=0;i<sizeof(chars)-1;i++){char label[2]={chars[i],0};button(p,sheet,18+(i%10)*70,52+(i/10)*50,62,42,label,A_CHAR_BASE+(unsigned char)chars[i],0xF3F5F8,17);}
  button(p,sheet,438,202,132,42,ui_tr("Delete"),A_KEY_DELETE,0xEEF3F8,14);
  button(p,sheet,578,202,140,42,ui_tr("Clear"),A_KEY_CLEAR,0xEEF3F8,14);
 }
 list_text_update(&p->list.draft,p->draft,18,8,430,34,20,INK,LV_TEXT_ALIGN_LEFT);
 lv_obj_clear_flag(p->list.editor,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(p->list.editor);
}
static void list_build(panel_page_t *p)
{
 list_view_t *v=&p->list;
 if(p->body&&v->language==ui_lang_generation())return;
 if(v->editor)lv_obj_del(v->editor);
 if(p->body)lv_obj_del(p->body);
 memset(v,0,sizeof(*v));v->language=ui_lang_generation();
 p->body=box(p->panel.content,0,0,792,400,0xF3F5F8,0,false);
 box(p->body,28,56,736,1,LINE,0,false);box(p->body,28,362,736,1,LINE,0,false);
 lv_obj_t *back=lv_nav_back_text_button_create(p->body,28,7,ui_panel_font(15,false),click,p);
 if(back){lv_obj_set_user_data(back,(void *)A_BACK);lv_obj_set_style_bg_color(back,lv_color_hex(0xE3E9EF),0);}
 list_text_make(&v->title,p->body,116,0,440,57,20,INK,LV_TEXT_ALIGN_LEFT);
 list_text_make(&v->meta,p->body,556,0,208,57,12,MUTED,LV_TEXT_ALIGN_RIGHT);
 v->table=box(p->body,28,115,736,219,0xF3F5F8,7,false);
 lv_obj_t *head=box(v->table,0,0,736,27,0xE7EDF4,0,false);
 for(unsigned col=0;col<3;col++)list_text_make(&v->head[col],head,0,0,100,27,11,0x668099,LV_TEXT_ALIGN_LEFT);
 for(unsigned row=0;row<6;row++){
  v->row[row]=box(v->table,0,27+row*32,736,32,row%2?0xEDF2F7:0xFFFFFF,0,false);
  lv_obj_add_event_cb(v->row[row],click,LV_EVENT_CLICKED,p);
  for(unsigned col=0;col<3;col++)list_text_make(&v->cell[row][col],v->row[row],0,0,100,32,17,0x35546F,LV_TEXT_ALIGN_LEFT);
 }
 list_text_make(&v->empty,v->table,12,27,712,120,14,MUTED,LV_TEXT_ALIGN_CENTER);
 list_text_make(&v->shown,p->body,28,363,490,37,12,MUTED,LV_TEXT_ALIGN_LEFT);
 v->prev=button(p,p->body,646,369,30,26,NULL,A_PREV,0xFFFFFF,12);icon(v->prev,"chevron-left",7,5,15,0x5F7E98);
 list_text_make(&v->range,p->body,680,363,50,37,12,0x3B5871,LV_TEXT_ALIGN_CENTER);
 v->next=button(p,p->body,734,369,30,26,NULL,A_NEXT,0xFFFFFF,12);icon(v->next,"chevron-right",7,5,15,0x5F7E98);
}
static const char *list_empty(const panel_page_t *p)
{
 if(p->tab!=LIST_REJECTS){
  if(p->multi&&p->multi_currency>=0){
   const multi_currency_t *c=&counting_multi_current()->currencies[p->multi_currency];
   const multi_serial_cache_t *s=counting_multi_serials((unsigned)p->multi_currency);
   multi_detail_status_t status=p->tab==LIST_SERIAL||(!p->denom_mode&&p->serial_query.text[0])?(s?s->status:MULTI_DETAIL_NONE):c->status;
   if(status==MULTI_DETAIL_NONE||status==MULTI_DETAIL_LOADING)return ui_text_get(UI_TEXT_MULTI_RECEIVING);
   if(status==MULTI_DETAIL_TIMEOUT)return ui_text_get(UI_TEXT_MULTI_WAIT_END);
   if(status==MULTI_DETAIL_INVALID)return ui_text_get(UI_TEXT_MULTI_INVALID);
  }
  return ui_tr("No matching records");
 }
 if(!p->multi)return ui_text_get(UI_TEXT_LIST_NO_REJECT_DETAILS);
 const multi_reject_cache_t *r=counting_multi_rejects();
 if(p->reject_pending||!r||r->status==MULTI_DETAIL_NONE)return ui_text_get(UI_TEXT_MULTI_RECEIVING);
 if(r->status==MULTI_DETAIL_LOADING)return ui_text_get(UI_TEXT_MULTI_RECEIVING);
 if(r->status==MULTI_DETAIL_TIMEOUT)return ui_text_get(UI_TEXT_MULTI_WAIT_END);
 if(r->status==MULTI_DETAIL_INVALID)return ui_text_get(UI_TEXT_MULTI_INVALID);
 return ui_text_get(UI_TEXT_MULTI_NO_RECORDS);
}
static void list_render(panel_page_t *p)
{
 list_project(p);list_build(p);list_view_t *v=&p->list;
 const counting_sim_t *d=counting_data_current();const counting_multi_t *m=counting_multi_current();char meta[80],amount[40],title[128];
 bool rejects=p->tab==LIST_REJECTS,overview=p->multi&&p->multi_currency<0&&!rejects,value=p->tab==LIST_VALUE||overview;
 const char *code=p->multi?(p->multi_currency<0?"MULTI":m->currencies[p->multi_currency].code):currency_state_display_code(p->code);
 snprintf(title,sizeof(title),"%s  %s",ui_tr(rejects?"Rejects":"Count records"),code);unsigned pcs=d?(unsigned)LV_MAX(0,d->total_pcs):0;
 bool monetary=d&&counting_data_monetary_result_supported(d);uint64_t total=d&&d->total_amount>0?(uint64_t)d->total_amount:0;
 if(p->multi){pcs=p->multi_currency<0?m->total_pcs:m->currencies[p->multi_currency].pcs;monetary=p->multi_currency>=0;total=monetary?m->currencies[p->multi_currency].amount:0;}
 if(rejects){pcs=p->multi?m->reject:counting_data_reject_pcs_count(d);monetary=false;}
 comma(amount,sizeof(amount),total);const char *symbol=currency_metadata_symbol(code);
 snprintf(meta,sizeof(meta),"%u %s%s%s%s",pcs,ui_tr("PCS"),monetary?"   ":"",monetary?(symbol?symbol:code):"",monetary?amount:"");
 list_text_update(&v->title,title,116,0,440,57,20,INK,LV_TEXT_ALIGN_LEFT);
 list_text_update(&v->meta,meta,556,0,208,57,12,MUTED,LV_TEXT_ALIGN_RIGHT);
 list_filters(p);
 int cols[3]={12,value?257:130,value?503:571},widths[3]={value?221:106,value?221:429,value?221:153};
 if(rejects){cols[1]=130;cols[2]=257;widths[0]=106;widths[1]=106;widths[2]=467;}
 const char *heads[3]={overview?ui_tr("Currency"):value?ui_tr("Denom"):rejects&&p->multi?ui_text_get(UI_TEXT_MULTI_ERROR_CODE):"NO.",ui_tr(value||rejects?"PCS":"Serial number"),rejects?ui_tr("Reason"):ui_tr(value?"Amount":"Denom")};
 for(unsigned col=0;col<3;col++)list_text_update(&v->head[col],heads[col],cols[col],0,widths[col],27,11,0x668099,col==2&&!rejects?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT);
 for(unsigned row=0;row<6;row++){
  unsigned ix=p->page*6+row;if(ix>=p->count){lv_obj_add_flag(v->row[row],LV_OBJ_FLAG_HIDDEN);continue;}
  lv_obj_clear_flag(v->row[row],LV_OBJ_FLAG_HIDDEN);
  if(overview){lv_obj_add_flag(v->row[row],LV_OBJ_FLAG_CLICKABLE);lv_obj_set_user_data(v->row[row],(void *)(uintptr_t)(A_MULTI_BASE+ix));}
  else lv_obj_clear_flag(v->row[row],LV_OBJ_FLAG_CLICKABLE);
  unsigned slot=p->slots[ix];char a[64],b[64],c[192];
  if(rejects){unsigned err_code,err_pcs;
   if(p->multi){const multi_reject_t *r=&counting_multi_rejects()->rows[slot];err_code=r->code;err_pcs=r->pcs;snprintf(a,sizeof(a),"0x%02X",err_code);}
   else{err_code=d->err_code[slot];err_pcs=d->err_pcs[slot];snprintf(a,sizeof(a),"%03u",slot+1);}
   snprintf(b,sizeof(b),"%u",err_pcs);snprintf(c,sizeof(c),"%s",ui_text_counting_reject_reason(err_code));
  }else if(overview){const multi_currency_t *currency=&m->currencies[ix];snprintf(a,sizeof(a),"%s",currency->code);snprintf(b,sizeof(b),"%u",currency->pcs);comma(c,sizeof(c),currency->amount);}
  else if(p->multi){const multi_currency_t *currency=&m->currencies[p->multi_currency];if(value){snprintf(a,sizeof(a),"%u",currency->denom[slot].value);snprintf(b,sizeof(b),"%u",p->filtered_pcs[slot]);comma(c,sizeof(c),(uint64_t)currency->denom[slot].value*p->filtered_pcs[slot]);}else{const multi_serial_t *s=&counting_multi_serials((unsigned)p->multi_currency)->rows[slot];snprintf(a,sizeof(a),"%03u",s->number);snprintf(b,sizeof(b),"%s",s->text);snprintf(c,sizeof(c),"%u",s->value);}}
  else if(value){snprintf(a,sizeof(a),"%s %d",symbol?symbol:"",d->denom[slot].value);snprintf(b,sizeof(b),"%u",p->filtered_pcs[slot]);comma(c,sizeof(c),(uint64_t)d->denom[slot].value*p->filtered_pcs[slot]);}
  else{snprintf(a,sizeof(a),"%03u",slot+1);snprintf(b,sizeof(b),"%s",d->sn_str[slot]);
   if(counting_data_monetary_result_supported(d))snprintf(c,sizeof(c),"%d",d->denom_mix[slot]);else snprintf(c,sizeof(c),"--");}
  const char *cells[3]={a,b,c};unsigned sizes[3]={value?17:12,value||rejects?17:20,rejects?14:17};
  for(unsigned col=0;col<3;col++){
   lv_obj_set_style_text_letter_space(v->cell[row][col].object,col==1&&!value&&!rejects?1:0,0);
   list_text_update(&v->cell[row][col],cells[col],cols[col],0,widths[col],32,sizes[col],col==0&&!value?0x7B90A3:0x35546F,col==2&&!rejects?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT);
  }
 }
 if(p->count)lv_obj_add_flag(v->empty.object,LV_OBJ_FLAG_HIDDEN);
 else{list_text_update(&v->empty,list_empty(p),12,27,712,120,14,MUTED,LV_TEXT_ALIGN_CENTER);lv_obj_clear_flag(v->empty.object,LV_OBJ_FLAG_HIDDEN);}
 char shown[128],range[40];snprintf(shown,sizeof(shown),ui_tr("Showing %u records"),p->count);unsigned page_count=LV_MAX(1,(p->count+5)/6);snprintf(range,sizeof(range),"%u / %u",p->page+1,page_count);
 list_text_update(&v->shown,shown,28,363,490,37,12,MUTED,LV_TEXT_ALIGN_LEFT);
 list_text_update(&v->range,range,680,363,50,37,12,0x3B5871,LV_TEXT_ALIGN_CENTER);
 if(p->page)lv_obj_clear_state(v->prev,LV_STATE_DISABLED);else lv_obj_add_state(v->prev,LV_STATE_DISABLED);
 if(p->page+1<page_count)lv_obj_clear_state(v->next,LV_STATE_DISABLED);else lv_obj_add_state(v->next,LV_STATE_DISABLED);
 text_editor(p);
}

static void render(panel_page_t *p)
{
 if(!p||!p->panel.content)return;
 update_fault(p);
 /* A central choice owns its current draft until dismissed, even on a reply. */
 if(panel_choice_is_open_for(p)){set_keys(p);set_motion(p);return;}
 if(p->id==UI_PAGE_LIST)list_render(p);
 else{
  if(p->body)lv_obj_del(p->body);
  p->body=box(p->panel.content,0,0,792,400,0xF3F5F8,0,false);if(!p->body)return;
  if(p->id==UI_PAGE_MENU)menu_render(p);else settings_render(p);
 }
 set_keys(p);set_motion(p);p->trigger=0;
}
static void panel_action(void *context,uint16_t action,ui_panel_event_t event)
{if(event==UI_PANEL_EVENT_CLICK)perform(context,action);}
static void perform(panel_page_t *p,unsigned action)
{
 if(!visible(p))return;
 if((action==A_BATCH||action==A_MODE||action==A_SPEED||action==A_CURRENCY||action==A_SORT)&&setting_busy()){
  notice(UI_N_("Wait for the current operation, then retry."));return;
 }
 p->revision++;p->trigger=(uint16_t)action;
 if(action>=A_CATEGORY_BASE&&action<A_CATEGORY_BASE+(p->id==UI_PAGE_MENU?4:6)){p->category=action-A_CATEGORY_BASE;refresh(p,false);return;}
 if(action>=A_CHAR_BASE&&action<A_CHAR_BASE+128){size_t n=strlen(p->draft);if(n+1<sizeof(p->serial_query.text)){p->draft[n]=(char)(action-A_CHAR_BASE);p->draft[n+1]=0;}text_editor(p);return;}
 if(action>=A_DENOM_BASE&&action<A_DENOM_BASE+p->denom_count){int value=p->denoms[action-A_DENOM_BASE];unsigned i=0;while(i<p->denom_query.denomination_count&&p->denom_query.denominations[i]!=value)i++;
  if(i<p->denom_query.denomination_count){memmove(&p->denom_query.denominations[i],&p->denom_query.denominations[i+1],(p->denom_query.denomination_count-i-1)*sizeof(int));p->denom_query.denomination_count--;}
  else if(i<COUNTING_SERIAL_QUERY_MAX_DENOMS)p->denom_query.denominations[p->denom_query.denomination_count++]=value;
  refresh(p,true);return;}
 if(action>=A_MULTI_BASE&&action<A_MULTI_BASE+COUNTING_MULTI_MAX){p->multi_currency=(int)(action-A_MULTI_BASE);memset(&p->serial_query,0,sizeof(p->serial_query));memset(&p->denom_query,0,sizeof(p->denom_query));refresh(p,true);return;}
 switch(action){
 case A_BACK:if(p->id==UI_PAGE_LIST&&p->multi&&p->multi_currency!=PAGE_02_MULTI_OVERVIEW){p->multi_currency=PAGE_02_MULTI_OVERVIEW;p->tab=LIST_VALUE;p->filter=false;p->editing=false;refresh(p,true);return;}ui_manager_clear_stack();ui_manager_switch(p->id==UI_PAGE_SETTING?UI_PAGE_MENU:UI_PAGE_MAIN);return;
 case A_MAIN:ui_manager_clear_stack();ui_manager_switch(UI_PAGE_MAIN);return;
 case A_MENU:ui_manager_switch(UI_PAGE_MENU);return;
 case A_SET:case A_PIN:ui_page_05_set_password_open();return;
 case A_OPERATOR:ui_manager_push_page(UI_PAGE_OPERATORS);return;
 case A_RECEIPT:ui_manager_push_page(UI_PAGE_PRINT_SETTING);return;
 case A_HISTORY:ui_page_19_history_open_all();return;
 case A_PRINT:page_01_action_run(PAGE_01_ACTION_PRINT);break;
 case A_FAULT_GUIDE:if(p->have_fault)(void)fault_popup_show_key(p->fault);return;
 case A_EXPORT:if(!ui_export_data_request())notice(UI_N_("Export request failed. Check the USB drive."));break;
 case A_BATCH:case A_BRIGHTNESS:case A_STANDBY:open_number(p,action);return;
 case A_MODE:case A_SPEED:open_choice(p,action);return;
 case A_CURRENCY:ui_manager_push_page(UI_PAGE_CURR);return;
 case A_SORT:if(!setting_service_request_fo_mode((machine_state_fo_mode()+1)%FO_MODE))notice(UI_N_("Setting not sent. Finish the current operation and try again."));break;
 case A_SOUND:app_local_setting_apply(APP_LOCAL_TOUCH_SOUND,!user_cfg_touch_sound_enabled(),0);break;
 case A_BEEP:if(!setting_service_request_beep(!machine_state_buzzer_enabled()))notice(UI_N_("Setting not sent. Finish the current operation and try again."));break;
 case A_LANGUAGE:ui_manager_push_page(UI_PAGE_LANGUAGE_SETTING);return;
 case A_PASSWORD_CHANGE:ui_manager_push_page(UI_PAGE_PASSWORD_CHANGE);return;
 case A_SEARCH:if(p->tab==LIST_REJECTS||(p->multi&&p->multi_currency<0))return;p->filter=!p->filter;p->editing=false;break;
 case A_RESET:memset(&p->serial_query,0,sizeof(p->serial_query));memset(&p->denom_query,0,sizeof(p->denom_query));p->page=0;break;
 case A_REJECTS:p->tab=LIST_REJECTS;p->multi_currency=PAGE_02_MULTI_REJECTS;p->filter=false;p->editing=false;p->reject_pending=true;p->page=0;break;
 case A_VALUE:case A_SERIAL:if(p->id!=UI_PAGE_LIST){ui_manager_push_page(UI_PAGE_LIST);return;}p->tab=action==A_VALUE?LIST_VALUE:LIST_SERIAL;if(p->multi_currency==PAGE_02_MULTI_REJECTS)p->multi_currency=PAGE_02_MULTI_OVERVIEW;p->page=0;break;
 case A_PREV:if(p->page)p->page--;break;
 case A_NEXT:if((p->page+1)*6<p->count)p->page++;break;
 case A_FILTER_VALUE:p->denom_mode=true;p->page=0;p->editing=false;break;
 case A_FILTER_SERIAL:p->denom_mode=false;p->page=0;break;
 case A_SERIAL_INPUT:snprintf(p->draft,sizeof(p->draft),"%s",p->serial_query.text);p->editing=true;break;
 case A_SERIAL_CLEAR:p->serial_query.text[0]=0;p->page=0;break;
 case A_SERIAL_APPLY:snprintf(p->serial_query.text,sizeof(p->serial_query.text),"%.*s",(int)sizeof(p->serial_query.text)-1,p->draft);p->editing=false;p->page=0;break;
 case A_SERIAL_CANCEL:p->editing=false;break;
 case A_KEY_CLEAR:p->draft[0]=0;text_editor(p);return;
 case A_KEY_DELETE:{size_t n=strlen(p->draft);if(n)p->draft[n-1]=0;text_editor(p);return;}
 case A_DETECTION:ui_page_05_set_password_open_for(UI_PAGE_CFD_LEVEL_SETTING);return;
 case A_COLLECTION:page_06_settings_open_detail("collection");return;
 case A_VERSIONS:if(p->id==UI_PAGE_MENU){page_03_menu_open_detail(5,2);return;}page_06_settings_open_detail("versions");return;
 case A_CALIBRATION:page_06_settings_open_detail("calibration");return;
 case A_SENSOR:ui_manager_push_page(UI_PAGE_SENSOR);return;
 case A_MOTOR:ui_manager_push_page(UI_PAGE_MOTOR_TEST);return;
 case A_INTERACTION:page_03_menu_open_detail(4,1);return;
 case A_DISPLAY:page_03_menu_open_detail(4,2);return;
 case A_QUICK_GUIDE:gesture_guide_show();return;
 case A_CARE:page_03_menu_open_detail(5,1);return;
 case A_FAULT_HELP:page_03_menu_open_detail(5,0);return;
 default:return;
 }
 refresh(p,false);
}
static void poll(lv_timer_t *timer)
{
 panel_page_t *p=timer->user_data;if(!visible(p))return;
 if(update_fault(p)){set_keys(p);set_motion(p);p->trigger=0;}
 if(p->id==UI_PAGE_LIST&&currency_state_multi_selected()){
  reject_request(p);
  detail_request(p);
  const counting_multi_t *m=counting_multi_current();uint32_t extra=counting_multi_extra_revision();
  if(m->revision!=p->multi_revision||m->generation!=p->multi_generation||extra!=p->extra_revision){p->multi_revision=m->revision;p->multi_generation=m->generation;p->extra_revision=extra;refresh(p,false);}
 }
}
static void page_root_deleted(lv_event_t *event)
{
 panel_page_t *p=lv_event_get_user_data(event);
 if(lv_event_get_code(event)!=LV_EVENT_DELETE)return;
 ui_frame_commit_cancel(commit,p);panel_entry_close_for(p);panel_choice_close_for(p);
 if(p->timer)lv_timer_del(p->timer);
 int index=index_of(p->id);if(index>=0&&pages[index]==p)pages[index]=NULL;
 lv_mem_free(p);
}
bool panel_pages_create(ui_page_t id,lv_obj_t *parent)
{
 int index=index_of(id);if(index<0)return false;if(pages[index]){if(pages[index]->panel.root)return true;panel_pages_destroy(id);}
 panel_page_t *p=lv_mem_alloc(sizeof(*p));if(!p)return false;memset(p,0,sizeof(*p));p->id=id;p->multi_currency=-1;p->denom_mode=true;p->animation=true;
 if(!ui_panel_create(&p->panel,parent?parent:lv_scr_act(),panel_action,p)){lv_mem_free(p);return false;}
 pages[index]=p;lv_obj_add_event_cb(p->panel.root,page_root_deleted,LV_EVENT_DELETE,p);p->timer=lv_timer_create(poll,200,p);render(p);return true;
}
bool panel_pages_created(ui_page_t id){panel_page_t *p=page_for(id);return p&&p->panel.root;}
bool panel_pages_visible(ui_page_t id){return visible(page_for(id));}
bool panel_pages_resume(ui_page_t id)
{panel_page_t *p=page_for(id);if(!p)return false;ui_panel_set_visible(&p->panel,true);lv_obj_move_foreground(p->panel.root);if(p->timer)lv_timer_resume(p->timer);refresh(p,false);return true;}
void panel_pages_suspend(ui_page_t id)
{panel_page_t *p=page_for(id);if(!p)return;ui_frame_commit_cancel(commit,p);panel_entry_close_for(p);panel_choice_close_for(p);p->editing=false;if(p->timer)lv_timer_pause(p->timer);ui_panel_set_visible(&p->panel,false);}
void panel_pages_destroy(ui_page_t id)
{panel_page_t *p=page_for(id);if(!p)return;panel_pages_suspend(id);ui_panel_destroy(&p->panel);}
void panel_pages_refresh(ui_page_t id,bool reset){refresh(page_for(id),reset);}
void panel_pages_list_select(int currency,int tab)
{
 pending_currency=currency;
 pending_tab=currency==PAGE_02_MULTI_REJECTS?LIST_REJECTS:tab==PAGE_02_MULTI_TAB_SERIAL?LIST_SERIAL:LIST_VALUE;
 refresh(page_for(UI_PAGE_LIST),true);
}
bool panel_pages_settings_category(unsigned category)
{panel_page_t *p=page_for(UI_PAGE_SETTING);if(!p||category>=6)return false;p->category=category;refresh(p,false);return true;}
