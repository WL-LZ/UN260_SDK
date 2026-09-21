/* Settings navigation owns layout; device actions remain in their existing services. */
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_06_settings.h"
#include "settings_catalog.h"
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
static bool data_collection_dirty=true,data_collection_snapshot_valid;
static data_collect_mode_t data_collection_last_mode;
static int data_collection_last_pcs;
static char data_collection_last_status[128];
static lv_obj_t *dc_btn_all,*dc_btn_false,*dc_btn_start,*dc_btn_disable,*dc_btn_retry;
static lv_obj_t *dc_label_all,*dc_label_false,*dc_check_all,*dc_check_false;
static lv_obj_t *dc_mode_value_label,*dc_pcs_label,*dc_status_label,*dc_gate_label;
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
 lv_obj_t *l=lv_label_create(p);lv_label_set_text(l,t);lv_obj_set_style_text_font(l,f,0);lv_obj_set_style_text_color(l,c,0);
 lv_obj_clear_flag(l,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);return l;
}
static void capture_scroll(void){if(grid&&scope)scroll_positions[scope-catalog]=lv_obj_get_scroll_y(grid);}
static void reset_detail_refs(void){
 memset(version_value_labels,0,sizeof(version_value_labels));memset(value_labels,0,sizeof(value_labels));
 dc_btn_all=dc_btn_false=dc_btn_start=dc_btn_disable=dc_btn_retry=NULL;
 dc_label_all=dc_label_false=dc_check_all=dc_check_false=NULL;
 dc_mode_value_label=dc_pcs_label=dc_status_label=dc_gate_label=NULL;
 data_collection_dirty=true;data_collection_snapshot_valid=false;
}
static void back_event_cb(lv_event_t *e){
 (void)e;
 if(scope&&scope->parent){capture_scroll();scope=find(scope->parent);render();return;}
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
 lv_settings_label(sidebar,"Settings",18,18,&lv_font_instrument_sans_semibold_24,0x1D2B34);
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
  lv_obj_t *title=lv_settings_label(b,catalog[i].title,title_x,15,&lv_font_instrument_sans_medium_16,active?0x1462CC:0x1D2B34);
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
 lv_label_set_text(home_text,"Back to count");
 lv_obj_set_width(home_text,144);lv_obj_set_style_text_align(home_text,LV_TEXT_ALIGN_LEFT,0);
 lv_obj_align(home_text,LV_ALIGN_LEFT_MID,48,0);
 lv_obj_t *home_icon=lv_obj_get_child(home,1);
 if(home_icon)lv_obj_align(home_icon,LV_ALIGN_LEFT_MID,15,0);
}
static void value_for(const settings_node_t *n,char *out,size_t cap){
 out[0]=0;
 if(!strcmp(n->id,"language"))snprintf(out,cap,"%s",ui_lang_get()==LANGUAGE_EN?"English":"Unavailable");
 else if(!strcmp(n->id,"time"))snprintf(out,cap,"24-hour");
 else if(!strcmp(n->id,"brightness")){
  int max=backlight_service_max();
  if(max>0)snprintf(out,cap,"%d%%",backlight_service_level()*100/max);
  else snprintf(out,cap,"Unavailable");
 }
 else if(!strcmp(n->id,"standby")){unsigned m=standby_config()->minutes;snprintf(out,cap,m?"%u min":"Never",m);}
 else if(!strcmp(n->id,"reject"))snprintf(out,cap,"%u PCS",machine_state_reject_pocket_max());
 else if(!strcmp(n->id,"double"))snprintf(out,cap,"Level %u",machine_state_double_note_level());
}
static void refresh_values(void){
 for(size_t i=0;i<catalog_count;i++)if(value_labels[i]){
  char value[48];value_for(catalog+i,value,sizeof(value));
  if(strcmp(lv_label_get_text(value_labels[i]),value))lv_label_set_text(value_labels[i],value);
 }
}
static void create_version_page_content(lv_obj_t *parent){
 static const char *names[]={"Controller","Image board","Display & logic"};
 static const char *hints[]={"Machine control","Image processing","Interface and FPGA"};
 static const int indexes[3][2]={{0,3},{1,4},{5,2}};
 lv_obj_set_style_bg_opa(parent,0,0);lv_obj_set_style_border_width(parent,0,0);
 for(int i=0;i<3;i++){
  lv_obj_t *card=lv_settings_panel(parent,i*416,0,400,242);
  lv_settings_label(card,names[i],22,20,&lv_font_instrument_sans_semibold_22,0x1D2B34);
  lv_settings_label(card,hints[i],22,49,&lv_font_instrument_sans_medium_14,0x586B78);
  lv_settings_box(card,22,80,356,1,0xE3E9ED);
  for(int j=0;j<2;j++){
   const int index=indexes[i][j],y=98+j*71;
   lv_settings_label(card,j?(i==2?"FPGA":"Bootloader"):(i==2?"UI software":"Application"),22,y,&lv_font_instrument_sans_medium_14,0x586B78);
   version_value_labels[index]=lv_settings_label(card,"Waiting",22,y+22,&lv_font_instrument_sans_semibold_22,0x1D2B34);
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
static const char* get_data_collect_mode_name(data_collect_mode_t mode)
{
    switch (mode) {
    case DATA_COLLECT_MODE_ALL:
        return "All notes";
    case DATA_COLLECT_MODE_FALSE:
        return "Rejected notes";
    default:
        return "Not selected";
    }
}

static void update_data_collect_btn_style(lv_obj_t* btn, lv_obj_t* label, lv_obj_t* check, bool selected)
{
    if (!btn || !label || !check) {
        return;
    }

    lv_obj_set_style_bg_color(btn, lv_color_hex(selected ? 0xEDF4FF : 0xF1F4F5), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, selected ? lv_color_hex(0x8CACDA) : color_line(), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_text_color(label, selected ? color_primary() : color_text(), 0);
    if(selected)lv_obj_clear_flag(check,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(check,LV_OBJ_FLAG_HIDDEN);
}

void page_06_data_collection_refresh(void)
{
    data_collect_mode_t mode;
    bool request_pending;
    int pcs;
    const char *status;
    bool mode_changed;
    bool pcs_changed;
    bool status_changed;

    if (!scope_is("collection") || !dc_btn_all || !settings_page_is_visible()) {
        data_collection_dirty = true;
        return;
    }

    mode = data_collection_state_mode();
    request_pending = data_collection_request_pending();
    pcs = data_collection_state_pcs();
    status = data_collection_state_status();
    if (status == NULL) status = "";
    mode_changed = data_collection_dirty || !data_collection_snapshot_valid ||
                   mode != data_collection_last_mode;
    pcs_changed = data_collection_dirty || !data_collection_snapshot_valid ||
                  pcs != data_collection_last_pcs;
    status_changed = data_collection_dirty || !data_collection_snapshot_valid ||
                     strcmp(status, data_collection_last_status) != 0;

    bool ready=work_mode_service_diagnostic_ready();
    bool busy=app_command_runtime_count_start_busy();
    lv_obj_t *actions[]={dc_btn_all,dc_btn_false,dc_btn_start,dc_btn_disable};
    for(unsigned i=0;i<4;i++){
        bool enabled=!request_pending&&!busy&&(i!=2||ready)&&
                     (i<2||mode!=DATA_COLLECT_MODE_NONE);
        if(enabled)settings_detail_action_block(actions[i], NULL);
        else settings_detail_action_block(actions[i], request_pending ? "Wait for the current collection request to finish." : busy ? "Stop counting before changing collection." : !ready ? work_mode_service_status_text() : "Select All notes or Rejected notes first.");
    }
    work_mode_snapshot_t gate;
    work_mode_service_get_snapshot(&gate);
    if(dc_btn_retry){
        if(gate.phase==WORK_MODE_FAILED)lv_obj_clear_flag(dc_btn_retry,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(dc_btn_retry,LV_OBJ_FLAG_HIDDEN);
    }
    if(dc_gate_label&&strcmp(lv_label_get_text(dc_gate_label),work_mode_service_status_text()))
        lv_label_set_text(dc_gate_label,work_mode_service_status_text());

    if (mode_changed) {
        update_data_collect_btn_style(dc_btn_all, dc_label_all, dc_check_all,
                                      mode == DATA_COLLECT_MODE_ALL);
        update_data_collect_btn_style(dc_btn_false, dc_label_false, dc_check_false,
                                      mode == DATA_COLLECT_MODE_FALSE);
        if (dc_mode_value_label) {
            lv_label_set_text(dc_mode_value_label, get_data_collect_mode_name(mode));
        }
    }

    if (pcs_changed && dc_pcs_label) {
        lv_label_set_text_fmt(dc_pcs_label, "%d", pcs);
    }

    if (status_changed && dc_status_label) {
        lv_label_set_text(dc_status_label, status);
    }

    data_collection_last_mode = mode;
    data_collection_last_pcs = pcs;
    snprintf(data_collection_last_status, sizeof(data_collection_last_status),
             "%s", status);
    data_collection_snapshot_valid = true;
    data_collection_dirty = false;
}

void page_06_data_collection_on_reply(data_collection_reply_result_t result)
{
    (void)result;
    page_06_data_collection_refresh();
}

static void data_collect_mode_btn_event_cb(lv_event_t* e)
{
    uint8_t sub = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    data_collect_mode_t mode;
    const char* status = NULL;
    if(data_collection_request_pending()||app_command_runtime_count_start_busy())return;

    if (sub == 0x01) {
        mode = DATA_COLLECT_MODE_ALL;
        status = "Requesting all-note collection";
    } else if (sub == 0x02) {
        mode = DATA_COLLECT_MODE_FALSE;
        status = "Requesting rejected-note collection";
    } else {
        return;
    }

    if (!data_collection_request_begin(mode, status,
                                       app_clock_uptime_ms())) {
        return;
    }
    if (!settings_detail_send_command(0xC0, &sub, 1)) {
        data_collection_request_cancel();
        data_collection_state_set_status("Command not sent. Check the controller connection.");
        page_06_data_collection_refresh();
        return;
    }
    page_06_data_collection_refresh();
}

static void data_collect_start_btn_event_cb(lv_event_t* e)
{
    (void)e;
    if(!work_mode_service_diagnostic_ready()||data_collection_request_pending()||app_command_runtime_count_start_busy())return;

    if (data_collection_state_mode() == DATA_COLLECT_MODE_NONE) {
        data_collection_state_set_status("Please select a collection mode first");
        page_06_data_collection_refresh();
        return;
    }

    if (app_command_runtime_request_diagnostic_run()) {
        data_collection_state_reset_pcs();
        data_collection_state_set_status("Counting command sent. Waiting for controller reply...");
        page_06_data_collection_refresh();
    } else {
        data_collection_state_set_status("Start was not accepted. Check the controller.");
        page_06_data_collection_refresh();
    }
}

static void data_collect_disable_btn_event_cb(lv_event_t* e)
{
    uint8_t sub = 0xFF;
    (void)e;
    if(app_command_runtime_count_start_busy()||data_collection_state_mode()==DATA_COLLECT_MODE_NONE)return;

    if (!data_collection_request_begin(DATA_COLLECT_MODE_NONE,
                                       "Exiting collection mode...",
                                       app_clock_uptime_ms())) {
        return;
    }
    if (!settings_detail_send_command(0xC0, &sub, 1)) {
        data_collection_request_cancel();
        data_collection_state_set_status("Command not sent. Check the controller connection.");
        page_06_data_collection_refresh();
        return;
    }
    page_06_data_collection_refresh();
}

static lv_obj_t* create_dc_mode_button(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                       const char* text, uint8_t sub,
                                       lv_obj_t** out_label, lv_obj_t** out_check)
{
    lv_obj_t* btn = lv_obj_create(parent);
    style_plain(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, 380, 62);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_settings_action_guard_init(btn);
    lv_obj_add_event_cb(btn, data_collect_mode_btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)sub);

    lv_obj_set_style_bg_color(btn,lv_color_hex(0xE2E9EE),LV_STATE_PRESSED);
    lv_obj_t* label = create_label(btn, text, &lv_font_instrument_sans_medium_18, color_text());
    lv_obj_set_pos(label, 18, 21);

    lv_obj_t* check = lv_settings_icon(btn,"Check-active",0,0);
    lv_obj_align(check, LV_ALIGN_RIGHT_MID, -16, 0);

    if (out_label) {
        *out_label = label;
    }
    if (out_check) {
        *out_check = check;
    }

    return btn;
}

static void data_collection_retry(lv_event_t *event)
{
    (void)event;
    work_mode_service_retry();
    page_06_data_collection_refresh();
}

static void create_data_collection_page_content(lv_settings_frame_t *frame)
{
    lv_obj_t *parent=frame->body;
    lv_obj_set_style_bg_opa(parent,0,0);lv_obj_set_style_border_width(parent,0,0);
    lv_obj_t *modes=lv_settings_panel(parent,0,0,424,242);
    lv_settings_label(modes,"Collect",22,19,&lv_font_instrument_sans_semibold_18,0x1D2B34);
    dc_btn_all = create_dc_mode_button(modes, 22, 52, "All notes",
                                       0x01, &dc_label_all, &dc_check_all);
    dc_btn_false = create_dc_mode_button(modes, 22, 126, "Rejected notes",
                                         0x02, &dc_label_false, &dc_check_false);
    lv_settings_label(modes,"Choose a mode before starting.",22,207,&lv_font_instrument_sans_medium_14,0x586B78);
    lv_obj_t *card=lv_settings_panel(parent,440,0,792,242);
    lv_settings_label(card,"Collection session",24,19,&lv_font_instrument_sans_semibold_18,0x1D2B34);
    lv_settings_box(card,24,52,744,1,0xE3E9ED);
    lv_settings_label(card,"Confirmed mode",24,73,&lv_font_instrument_sans_medium_14,0x586B78);
    dc_mode_value_label=lv_settings_label(card,"Not selected",24,98,&lv_font_instrument_sans_semibold_24,0x1D2B34);
    lv_settings_label(card,"Notes collected",548,73,&lv_font_instrument_sans_medium_14,0x586B78);
    dc_pcs_label=lv_settings_label(card,"0",548,98,&lv_font_instrument_sans_semibold_40,0x1D2B34);
    lv_settings_box(card,24,160,744,1,0xE3E9ED);
    dc_status_label=lv_settings_label(card,"Select a collection mode",24,184,&lv_font_instrument_sans_medium_16,0x586B78);
    lv_obj_set_width(dc_status_label,744);
    lv_label_set_long_mode(dc_status_label, LV_LABEL_LONG_WRAP);
    dc_btn_disable=lv_settings_button(frame->footer,898,0,164,44,"End collection",false,data_collect_disable_btn_event_cb,NULL);
    dc_btn_start=lv_settings_button(frame->footer,1076,0,156,44,"RUN",true,data_collect_start_btn_event_cb,NULL);
    dc_btn_retry=lv_settings_button(frame->footer,756,0,128,44,"Retry",false,data_collection_retry,NULL);
    lv_obj_add_flag(dc_btn_retry,LV_OBJ_FLAG_HIDDEN);
    dc_gate_label=frame->message;
    lv_obj_set_width(dc_gate_label,736);
    lv_label_set_text(dc_gate_label,work_mode_service_status_text());
    page_06_data_collection_refresh();
}


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
 snprintf(text,sizeof(text),"%u %s%s",visible,visible==1?"setting":"settings",more?"  /  Swipe for more":above?"  /  Swipe to return":"");
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
  lv_settings_header_t h={.title=scope->title,.subtitle="Settings",.back=back_event_cb};
  lv_settings_frame_t f=lv_settings_frame_create(settings_page,&h);view=f.root;
  if(scope_is("versions")){
   create_version_page_content(f.body);
   lv_label_set_text(f.message,"Controller versions are reported by the machine. UI version is local.");
  }
  else if(scope_is("collection")){
   create_data_collection_page_content(&f);
  }
  return;
 }
 view=lv_settings_box(settings_page,0,0,1280,400,0);lv_obj_set_style_bg_opa(view,0,0);
 create_sidebar();
 const settings_node_t *cat=category();
 const char *subtitle=scope->kind==SETTINGS_CATEGORY?scope->hint:cat->title;
 lv_settings_header_t h={.title=scope->title,.subtitle=subtitle,.back=back_event_cb};
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
  lv_settings_item_t cfg={.title=n->title,.hint=n->hint,.value=value,.icon=n->icon,.grouped=true,
   .activate=activate,.user_data=(void*)n,.value_label=&value_labels[i]};
  lv_settings_item(group,&cfg);
 }
 count_label=lv_settings_label(view,"",256,369,&lv_font_instrument_sans_medium_14,0x586B78);
 if(scope_is("calibration"))lv_settings_label(view,"Select a calibration to prepare it.",754,369,&lv_font_instrument_sans_medium_14,0x586B78);
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
bool page_06_settings_is_collection(void){return scope_is("collection");}
bool page_06_settings_switch_menu(page_06_settings_menu_t menu){
 static const char *ids[]={"device","maintenance","counting","about","data"};
 if(!settings_page||menu<0||menu>=PAGE_06_SETTINGS_MENU_COUNT)return false;
 capture_scroll();scope=find(ids[menu]);if(!scope)return false;render();return true;
}
bool page_06_settings_back_sub_page(void){
 if(!settings_page||!scope||!scope->parent)return false;
 capture_scroll();scope=find(scope->parent);render();return true;
}
