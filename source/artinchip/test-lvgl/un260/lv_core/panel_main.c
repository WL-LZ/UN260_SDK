#include "panel_main.h"
#include "panel_entry.h"
#include "page_01_main.h"
#include "page_02_list.h"
#include "lv_page_event.h"
#include "un260/lv_components/ui_panel.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_state_runtime.h"
#include "un260/lv_system/counting_ui_runtime.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_multi.h"
#include "un260/machine_state/machine_state.h"
#include "un260/machine_state/machine_fault.h"
#include "un260/currency/currency_state.h"
#include "un260/currency/currency_metadata.h"
#include "un260/currency/currency_service.h"
#include "un260/app_service/setting_service.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/app_service/app_fault_recovery.h"
#include "un260/app_service/workspace_service.h"
#include "un260/storage/workspace_store.h"
#include "un260/workspace/workspace_model.h"
#include "un260/protocol/protocol_send.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { A_MENU=1,A_CURRENCY,A_MODE,A_BATCH,A_SPEED,A_SORT,A_ADD,A_LIST,A_CLEAR,A_PRINT,A_START,
       A_SETTINGS,A_WORK,A_MODE_PICK,A_FAULT_GUIDE,A_FAULT_CHECK,A_REJECT_LIST,A_TAB_A=30,A_TAB_B,A_TAB_C,A_CURRENCY_DETAIL=40 };
typedef enum { PICK_NONE,PICK_SPEED,PICK_MODE,PICK_CURRENCY,PICK_CLEAR } picker_t;
typedef struct {lv_obj_t *object;unsigned size;bool semibold;} font_binding_t;
typedef struct {
    ui_panel_t panel;
    lv_obj_t *currency,*currency_chevron,*work,*mode,*set,*amount_title,*amount,*symbol,*pcs_title,*pcs;
    lv_obj_t *tabs[3],*tab_text[3],*reject_hint,*detail,*empty;
    lv_obj_t *cells[COUNTING_MULTI_MAX],*denom[COUNTING_MULTI_MAX];
    lv_obj_t *denom_pcs[COUNTING_MULTI_MAX],*denom_unit[COUNTING_MULTI_MAX],*denom_amount[COUNTING_MULTI_MAX];
    lv_obj_t *serial_no[4],*serial_text[4],*reject_text;
    lv_obj_t *footer_batch,*footer_add,*footer_cfd,*footer_operator;
    bool visible,running,have_fault;
    machine_fault_key_t fault;
    picker_t picker;
    uint32_t revision;
    uint16_t trigger;
    page_01_detail_section_t section;
    font_binding_t fonts[180];unsigned font_count;uint32_t language_generation;
} main_context_t;
static main_context_t main_view;
static void refresh_keys(void);
static void refresh_motion(void);
static void action(void *user,uint16_t id,ui_panel_event_t event);

static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *object=lv_obj_create(parent);lv_obj_remove_style_all(object);
    lv_obj_set_pos(object,x,y);lv_obj_set_size(object,w,h);
    lv_obj_set_style_bg_color(object,lv_color_hex(color),0);lv_obj_set_style_bg_opa(object,LV_OPA_COVER,0);
    lv_obj_set_style_radius(object,radius,0);lv_obj_clear_flag(object,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    return object;
}
static lv_obj_t *label(lv_obj_t *parent,const char *text,int x,int y,int w,unsigned px,uint32_t color,bool semibold)
{
    lv_obj_t *object=lv_label_create(parent);lv_obj_remove_style_all(object);
    lv_obj_set_style_text_font(object,ui_panel_font(px,semibold),0);
    lv_obj_set_style_text_color(object,lv_color_hex(color),0);
    lv_label_set_long_mode(object,LV_LABEL_LONG_CLIP);lv_label_set_text(object,text?text:"");
    lv_obj_set_pos(object,x,y);lv_obj_set_width(object,w);
    if(main_view.font_count<sizeof(main_view.fonts)/sizeof(main_view.fonts[0]))
        main_view.fonts[main_view.font_count++]=(font_binding_t){object,px,semibold};
    return object;
}
static void refresh_fonts(void)
{
    uint32_t generation=ui_lang_generation();if(main_view.language_generation==generation)return;
    main_view.language_generation=generation;
    for(unsigned i=0;i<main_view.font_count;i++){
        const font_binding_t *binding=&main_view.fonts[i];
        lv_obj_set_style_text_font(binding->object,ui_panel_font(binding->size,binding->semibold),0);
    }
    for(unsigned i=0;i<3;i++)lv_obj_center(main_view.tab_text[i]);
    lv_obj_center(lv_obj_get_child(main_view.set,0));
}
static void set_text(lv_obj_t *object,const char *text)
{
    if(object && strcmp(lv_label_get_text(object),text?text:""))lv_label_set_text(object,text?text:"");
}
static void set_visible(lv_obj_t *object,bool visible)
{if(visible)lv_obj_clear_flag(object,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(object,LV_OBJ_FLAG_HIDDEN);}
static void click(lv_event_t *event)
{action(NULL,(uint16_t)(uintptr_t)lv_event_get_user_data(event),UI_PANEL_EVENT_CLICK);}
static lv_obj_t *button(lv_obj_t *parent,const char *text,int x,int y,int w,int h,uint16_t id,unsigned px)
{
    lv_obj_t *object=box(parent,x,y,w,h,0xFFFFFF,7);lv_obj_add_flag(object,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(object,click,LV_EVENT_CLICKED,(void *)(uintptr_t)id);
    lv_obj_t *caption=label(object,text,0,0,w-8,px,0x476581,false);
    lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(caption);return object;
}
static const char *mode_text(uint8_t mode)
{
    return ui_text_get(mode==MODE_SDC?UI_TEXT_PAGE01_BOTTOM_MODE_SDC:
        mode==MODE_CNT?UI_TEXT_PAGE01_BOTTOM_MODE_CNT:UI_TEXT_PAGE01_BOTTOM_MODE_MDC);
}
static const char *speed_text(uint8_t speed)
{
    return ui_text_get(speed==1?UI_TEXT_PAGE01_BOTTOM_SPEED_MID:
        speed==2?UI_TEXT_PAGE01_BOTTOM_SPEED_HIGH:UI_TEXT_PAGE01_BOTTOM_SPEED_LOW);
}
static void close_picker(void *user)
{(void)user;main_view.picker=PICK_NONE;refresh_motion();}
static bool setting_blocked(void)
{
    if(!app_command_runtime_count_start_busy() && !workspace_service_applying())return false;
    ui_notice_post_text(UI_NOTICE_WARNING,"panel.settings",UI_N_("Settings"),
        UI_N_("Finish the current operation before changing settings."));
    return true;
}
static bool picker_confirm(uint32_t value,void *user)
{
    (void)user;bool sent=false;
    if(setting_blocked()){panel_choice_set_error(ui_tr("Finish the current operation first."));return false;}
    if(main_view.picker==PICK_SPEED)sent=setting_service_request_speed((uint8_t)value);
    else if(main_view.picker==PICK_MODE)sent=setting_service_request_mode((uint8_t)value);
    else if(main_view.picker==PICK_CLEAR){
        if(value==0){close_picker(NULL);return true;}
        sent=app_command_runtime_clear_counting_data("panel clear");
    }else if(main_view.picker==PICK_CURRENCY){
        char code[4],current[4];currency_state_get_selected_code(current);
        if(!currency_state_get_code((uint8_t)value,code))return false;
        if(!strcmp(current,code)){close_picker(NULL);return true;}
        if(!setting_service_mode_is_pending() && currency_service_request_switch((uint8_t)value,code)){
            sent=protocol_send(0x03,(const uint8_t *)code,3)>=0;
            if(!sent)currency_service_cancel_switch();
            else {ui_message_t info;ui_message_literal(&info,code);
                ui_notice_post_message(UI_NOTICE_PROGRESS,"settings.currency",UI_N_("Changing currency"),&info);}
        }
    }
    if(!sent){panel_choice_set_error(ui_tr("Wait for the current operation, then retry."));return false;}
    close_picker(NULL);return true;
}
static void open_picker(picker_t kind)
{
    panel_choice_item_t items[MAX_CURRENCIES];char codes[MAX_CURRENCIES][8];size_t count=0;
    const char *title="";
    if(kind==PICK_SPEED){title=ui_tr("Count speed");for(unsigned i=0;i<3;i++)
        items[count++]=(panel_choice_item_t){i,speed_text(i),machine_state_speed()==i,false};}
    else if(kind==PICK_MODE){const uint8_t modes[]={MODE_MDC,MODE_SDC,MODE_CNT};title=ui_tr("Count mode");
        for(unsigned i=0;i<3;i++)items[count++]=(panel_choice_item_t){modes[i],mode_text(modes[i]),machine_state_mode()==modes[i],false};}
    else if(kind==PICK_CLEAR){title=ui_tr("Clear the current count?");
        items[count++]=(panel_choice_item_t){0,ui_tr("Cancel"),false,false};
        items[count++]=(panel_choice_item_t){1,ui_tr("Confirm Clear"),false,false};}
    else if(kind==PICK_CURRENCY){
        char selected[4];currency_state_get_selected_code(selected);title=ui_tr("Currency");
        for(unsigned i=0;i<currency_state_count() && count<MAX_CURRENCIES;i++){
            char code[4];if(!currency_state_get_code(i,code))continue;
            snprintf(codes[count],sizeof(codes[count]),"%s",currency_state_display_code(code));
            items[count]=(panel_choice_item_t){i,codes[count],!strcmp(code,selected),false};count++;
        }
    }
    if(!count){page_01_action_run(PAGE_01_ACTION_CURRENCY);return;}
    panel_choice_config_t config={title,items,count,picker_confirm,close_picker,&main_view};
    if(panel_choice_open(ui_panel_content(&main_view.panel),&config)){main_view.picker=kind;refresh_motion();}
}
static bool batch_confirm(const char *text,void *user)
{
    (void)user;unsigned long value=strtoul(text,NULL,10);
    if(setting_blocked()){panel_entry_set_error(ui_tr("Finish the current operation first."));return false;}
    if(value<1 || value>200){panel_entry_set_error(ui_tr("Check the value"));return false;}
    bool sent=setting_service_request_batch_switch(value!=200,(uint8_t)value,
        machine_state_batch_enabled(),machine_state_batch_num());
    if(!sent)panel_entry_set_error(ui_tr("Wait for the current operation, then retry."));
    return sent;
}
static void open_batch(void)
{
    if(ui_state_main_batch_action_get()==UI_MAIN_BATCH_CYCLE){page_01_action_run(PAGE_01_ACTION_BATCH);return;}
    panel_entry_config_t config={.kind=PANEL_ENTRY_NUMBER,.title=ui_tr("Value setting"),
        .field_label=ui_tr("Batch size"),.prompt=ui_tr("Enter a value, then confirm"),
        .unit=ui_tr("PCS"),.range=ui_tr("Range 1-199"),.min=1,.max=199,.max_digits=3,
        .allow_off=true,.off_value=200,.confirm=batch_confirm,.user=&main_view};
    (void)panel_entry_open(lv_layer_top(),&config);
}
static void action(void *user,uint16_t id,ui_panel_event_t event)
{
    (void)user;if(!main_view.visible)return;
    if(event==UI_PANEL_EVENT_LONG_PRESS){if(id==A_MODE)page_01_main_toggle_display_mode();return;}
    main_view.revision++;main_view.trigger=id;
    if(id>=A_CURRENCY_DETAIL && id<A_CURRENCY_DETAIL+COUNTING_MULTI_MAX){
        const counting_multi_t *multi=counting_multi_current();unsigned index=id-A_CURRENCY_DETAIL;
        if((currency_state_multi_selected() || counting_data_current()->multi_currency_result) && index<multi->count){
            page_02_list_multi_open((int)index,main_view.section==PAGE_01_DETAIL_SECTION_B?PAGE_02_MULTI_TAB_SERIAL:PAGE_02_MULTI_TAB_VALUE);
            page_01_action_run(PAGE_01_ACTION_LIST);
        }
        return;
    }
    if(id>=A_TAB_A && id<=A_TAB_C){
        main_view.section=(page_01_detail_section_t)(id-A_TAB_A);
        page_01_detail_section_set(main_view.section,false);
        ui_state_save_page01_detail_section();panel_main_refresh(PAGE_01_MAIN_DIRTY_COUNTING);return;
    }
    if((id==A_BATCH || id==A_SPEED || id==A_CURRENCY || id==A_MODE_PICK ||
        id==A_MODE || id==A_SORT || id==A_ADD || id==A_WORK || id==A_CLEAR) && setting_blocked())return;
    if(id==A_BATCH){open_batch();return;}
    if(id==A_SPEED){open_picker(PICK_SPEED);return;}
    if(id==A_CURRENCY){open_picker(PICK_CURRENCY);return;}
    if(id==A_MODE_PICK){open_picker(PICK_MODE);return;}
    if(id==A_CLEAR){open_picker(PICK_CLEAR);return;}
    if(id==A_FAULT_GUIDE){(void)fault_popup_show_key(main_view.fault);return;}
    if(id==A_FAULT_CHECK){if(!app_fault_recovery_request_clear(main_view.fault))(void)fault_popup_show_key(main_view.fault);return;}
    switch(id){
    case A_MENU:page_01_action_run(PAGE_01_ACTION_MENU);return;
    case A_LIST:
        if(main_view.section==PAGE_01_DETAIL_SECTION_C)
            page_02_list_multi_open(PAGE_02_MULTI_REJECTS,PAGE_02_MULTI_TAB_VALUE);
        else if(currency_state_multi_selected() || counting_data_current()->multi_currency_result)
            page_02_list_multi_open(PAGE_02_MULTI_OVERVIEW,PAGE_02_MULTI_TAB_VALUE);
        page_01_action_run(PAGE_01_ACTION_LIST);return;
    case A_REJECT_LIST:
        page_02_list_multi_open(PAGE_02_MULTI_REJECTS,PAGE_02_MULTI_TAB_VALUE);
        page_01_action_run(PAGE_01_ACTION_LIST);return;
    case A_SETTINGS:page_01_action_run(PAGE_01_ACTION_SETTINGS);return;
    case A_MODE:page_01_mode_cycle();break;
    case A_SORT:page_01_action_run(PAGE_01_ACTION_SORT);break;
    case A_ADD:page_01_action_run(PAGE_01_ACTION_ADD);break;
    case A_WORK:page_01_action_run(PAGE_01_ACTION_WORK);break;
    case A_PRINT:page_01_action_run(PAGE_01_ACTION_PRINT);break;
    case A_START:page_01_action_run(PAGE_01_ACTION_START);break;
    default:return;
    }
    refresh_keys();refresh_motion();
}
static void refresh_keys(void)
{
    if(!main_view.panel.root)return;
    machine_state_snapshot_t machine;machine_state_get_snapshot(&machine);
    char code[4],batch[16];currency_state_get_effective_code(code);
    if(!code[0])currency_state_get_selected_code(code);
    snprintf(batch,sizeof(batch),"%u",machine.batch_num);
    ui_panel_key_t left[6]={
        {.action=A_MENU,.icon="menu",.label=ui_tr("Menu")},
        {.action=A_CURRENCY,.icon="globe",.label=currency_state_display_code(code)},
        {.action=A_MODE,.icon=machine.mode==MODE_CNT||machine.mode==MODE_SDC?"banknote":"layers",
         .label=mode_text(machine.mode),.lit=true,.long_press=true,.value=machine.mode},
        {.action=A_BATCH,.icon="banknote",.label=ui_trc("Panel key","Batch"),.sub=machine.batch_enabled?batch:ui_tr("OFF"),.value=machine.batch_num},
        {.action=A_SPEED,.label=ui_trc("Panel key","Speed"),.sub=speed_text(machine.speed),.value=machine.speed},
        {.action=A_SORT,.icon="scan-line",.label=ui_trc("Panel key","Sort"),.lit=machine.fo_mode!=0,.value=machine.fo_mode,.motion=UI_PANEL_KEY_SCAN}
    };
    ui_panel_key_t right[5]={
        {.action=A_ADD,.icon=machine.add_enabled?"layers":"plus",.label=ui_trc("Panel key","ADD"),.lit=machine.add_enabled,.value=machine.add_enabled},
        {.action=A_LIST,.icon="list",.label=ui_tr("List")},
        {.action=A_CLEAR,.icon="rotate-ccw",.label=ui_tr("Clear"),.motion=UI_PANEL_KEY_TURN},
        {.action=A_PRINT,.icon="printer",.label=ui_tr("Print"),.motion=UI_PANEL_KEY_FEED},
        {.action=A_START,.icon="play",.label=ui_tr("Start"),.primary=true,.lit=main_view.running,
         .disabled=app_command_runtime_count_start_busy()}
    };
    /* Firmware has no general STOP command. While counting the original Start
     * command remains guarded instead of presenting a fictitious Stop action. */
    if(main_view.have_fault){
        right[0]=(ui_panel_key_t){.action=A_FAULT_GUIDE,.icon="circle-help",.label=ui_tr("Guide")};
        right[4]=(ui_panel_key_t){.action=A_FAULT_CHECK,.icon="shield-check",.label=ui_tr("Check"),.danger=true};
    }
    ui_panel_set_keys(&main_view.panel,UI_PANEL_LAYOUT_STANDARD,left,6,right,5,main_view.revision,main_view.trigger);
    main_view.trigger=0;
}
static void refresh_motion(void)
{
    if(!main_view.panel.root)return;
    ui_panel_motion_t motion={.scene=UI_PANEL_SCENE_TRANSPORT,.title=ui_tr("Ready"),
        .detail=ui_tr("Insert notes"),.revision=main_view.revision};
    if(main_view.have_fault){motion.scene=UI_PANEL_SCENE_JAM;motion.title=ui_tr("Error");
        motion.detail=ui_tr("Open fault guide");motion.tone=UI_PANEL_TONE_WARNING;}
    else if(main_view.picker!=PICK_NONE){
        motion.scene=main_view.picker==PICK_SPEED?UI_PANEL_SCENE_DIAL:main_view.picker==PICK_MODE?UI_PANEL_SCENE_SORT:
            main_view.picker==PICK_CLEAR?UI_PANEL_SCENE_CLEAR:UI_PANEL_SCENE_STACK;
        motion.title=ui_tr(main_view.picker==PICK_SPEED?"Count speed":main_view.picker==PICK_MODE?"Count mode":
            main_view.picker==PICK_CLEAR?"Clear count":"Currency");
        motion.detail=ui_tr("Choose an option");motion.value=(uint16_t)(machine_state_speed()*500U);
    }else if(main_view.running){motion.title=ui_tr("Counting");motion.detail=speed_text(machine_state_speed());
        motion.running=true;motion.tone=UI_PANEL_TONE_SUCCESS;motion.value=(uint16_t)(500+machine_state_speed()*250U);}
    else if(app_command_runtime_count_start_busy()){motion.title=ui_tr("Waiting for device");motion.detail=ui_tr("Please wait");}
    ui_panel_set_motion(&main_view.panel,&motion);
}
static void update_fault(void)
{
    machine_fault_record_t fault;bool found=machine_fault_first_unread(&fault);
    if(!found)found=machine_fault_at(0,&fault);
    if(found!=main_view.have_fault || (found && !machine_fault_key_equal(fault.key,main_view.fault)))main_view.revision++;
    main_view.have_fault=found;if(found)main_view.fault=fault.key;
}
static void refresh_header(void)
{
    char code[4];currency_state_get_effective_code(code);
    if(!code[0])currency_state_get_selected_code(code);
    set_text(main_view.currency,currency_state_display_code(code));
    lv_obj_set_width(main_view.currency,LV_SIZE_CONTENT);lv_obj_update_layout(main_view.currency);
    lv_obj_set_x(main_view.currency_chevron,37+lv_obj_get_width(main_view.currency));
    set_text(main_view.mode,mode_text(machine_state_mode()));
    set_text(main_view.work,ui_text_get(machine_state_work_mode()?UI_TEXT_PAGE01_BOTTOM_WORK_MANUAL:UI_TEXT_PAGE01_BOTTOM_WORK_AUTO));
    lv_obj_set_width(main_view.mode,LV_SIZE_CONTENT);lv_obj_set_width(main_view.work,LV_SIZE_CONTENT);
    lv_obj_update_layout(main_view.mode);lv_obj_update_layout(main_view.work);
    lv_obj_set_pos(main_view.mode,703-lv_obj_get_width(main_view.mode),(57-lv_obj_get_height(main_view.mode))/2);
    lv_obj_set_pos(main_view.work,687-lv_obj_get_width(main_view.mode)-lv_obj_get_width(main_view.work),(57-lv_obj_get_height(main_view.work))/2);
    lv_obj_set_y(main_view.currency,(57-lv_obj_get_height(main_view.currency))/2);
    set_text(lv_obj_get_child(main_view.set,0),ui_tr("Set"));lv_obj_center(lv_obj_get_child(main_view.set,0));
    set_text(main_view.amount_title,ui_tr("Total amount"));set_text(main_view.pcs_title,ui_tr("PCS"));
}
static void refresh_footer(const counting_sim_t *data)
{
    char text[160];
    unsigned total=currency_state_multi_selected()||data->multi_currency_result?counting_multi_current()->total_pcs:(unsigned)data->total_pcs;
    if(machine_state_batch_enabled())snprintf(text,sizeof(text),"%s %u / %u",ui_tr("Batch"),total,machine_state_batch_num());
    else snprintf(text,sizeof(text),"%s %s",ui_tr("Batch"),ui_tr("OFF"));
    set_text(main_view.footer_batch,text);
    const workspace_model_t *workspace=workspace_store_get();const workspace_user_t *actor=workspace_active(workspace);
    if(actor && actor->name[0])snprintf(text,sizeof(text),"%s %s",ui_tr("Operator"),actor->name);
    else snprintf(text,sizeof(text),"%s",ui_tr("Operator"));
    set_text(main_view.footer_operator,text);
    lv_obj_set_width(main_view.footer_operator,LV_SIZE_CONTENT);lv_obj_update_layout(main_view.footer_operator);
    int width=lv_obj_get_width(main_view.footer_operator);if(width>195){width=195;lv_obj_set_width(main_view.footer_operator,width);}
    lv_obj_set_x(main_view.footer_operator,764-width);
    snprintf(text,sizeof(text),"CFD %s",ui_tr(machine_state_cfd_mode()?"ON":"OFF"));set_text(main_view.footer_cfd,text);
    lv_obj_set_width(main_view.footer_cfd,LV_SIZE_CONTENT);lv_obj_update_layout(main_view.footer_cfd);
    lv_obj_set_x(main_view.footer_cfd,750-width-lv_obj_get_width(main_view.footer_cfd));
    snprintf(text,sizeof(text),"ADD %s",ui_tr(machine_state_add_enabled()?"ON":"OFF"));set_text(main_view.footer_add,text);
    lv_obj_set_width(main_view.footer_add,LV_SIZE_CONTENT);lv_obj_update_layout(main_view.footer_add);
    lv_obj_set_x(main_view.footer_add,lv_obj_get_x(main_view.footer_cfd)-14-lv_obj_get_width(main_view.footer_add));
}
static void refresh_details(const counting_sim_t *data,const char *symbol)
{
    const bool multi_result=currency_state_multi_selected() || data->multi_currency_result;
    const counting_multi_t *multi=counting_multi_current();
    const char *titles[]={ui_tr("Values"),ui_tr("Serials"),ui_tr("Rejects")};
    for(unsigned i=0;i<3;i++){
        set_text(main_view.tab_text[i],titles[i]);
        lv_obj_set_style_bg_opa(main_view.tabs[i],main_view.section==i?LV_OPA_COVER:LV_OPA_TRANSP,0);
        lv_obj_set_style_text_color(main_view.tab_text[i],lv_color_hex(main_view.section==i?0x255E96:0x6B8195),0);
    }
    int rejects=multi_result?multi->reject:counting_data_reject_pcs_count(data);
    char text[128];snprintf(text,sizeof(text),"%s %d",ui_tr("Rejected"),rejects);
    set_text(main_view.reject_hint,text);
    bool monetary=counting_data_monetary_result_supported(data);
    unsigned count=multi_result?multi->count:data->denom_number;
    if(count>(multi_result?COUNTING_MULTI_MAX:COUNTING_DENOM_MAX_ITEMS))count=multi_result?COUNTING_MULTI_MAX:COUNTING_DENOM_MAX_ITEMS;
    for(unsigned i=0;i<COUNTING_MULTI_MAX;i++){
        bool visible=(main_view.section==PAGE_01_DETAIL_SECTION_A || (multi_result&&main_view.section==PAGE_01_DETAIL_SECTION_B)) &&
            (monetary||multi_result) && i<count;
        set_visible(main_view.cells[i],visible);if(!visible)continue;
        if(multi_result)lv_obj_add_flag(main_view.cells[i],LV_OBJ_FLAG_CLICKABLE);
        else lv_obj_clear_flag(main_view.cells[i],LV_OBJ_FLAG_CLICKABLE);
        if(multi_result)snprintf(text,sizeof(text),"%s",multi->currencies[i].code);
        else snprintf(text,sizeof(text),"%s %d",symbol?symbol:"",data->denom[i].value);
        set_text(main_view.denom[i],text);
        snprintf(text,sizeof(text),"%u",multi_result?multi->currencies[i].pcs:data->denom[i].pcs);set_text(main_view.denom_pcs[i],text);
        set_text(main_view.denom_unit[i],ui_tr("PCS"));
        char amount[64];format_amount_with_comma(amount,sizeof(amount),multi_result?multi->currencies[i].amount:data->denom[i].amount);
        const char *currency_symbol=multi_result?currency_metadata_symbol(multi->currencies[i].code):symbol;
        snprintf(text,sizeof(text),"%s %s",currency_symbol?currency_symbol:"",amount);set_text(main_view.denom_amount[i],text);
    }
    int valid=counting_data_serial_valid_count(data),start=valid>4?valid-4:0;
    for(unsigned i=0;i<4;i++){
        bool visible=!multi_result && main_view.section==PAGE_01_DETAIL_SECTION_B && start+(int)i<valid;
        set_visible(main_view.serial_no[i],visible);set_visible(main_view.serial_text[i],visible);
        if(visible){int index=counting_data_serial_nth_valid_index(data,start+(int)i);
            snprintf(text,sizeof(text),"%03d",index+1);set_text(main_view.serial_no[i],text);
            set_text(main_view.serial_text[i],index>=0?data->sn_str[index]:"");}
    }
    bool rejected=main_view.section==PAGE_01_DETAIL_SECTION_C && rejects>0;
    set_visible(main_view.reject_text,rejected);
    if(rejected){snprintf(text,sizeof(text),"%s %d  ·  %s",ui_tr("Rejected"),rejects,ui_tr("Open records for details"));set_text(main_view.reject_text,text);}
    bool empty=(main_view.section==PAGE_01_DETAIL_SECTION_A && !count) ||
        (main_view.section==PAGE_01_DETAIL_SECTION_B && (multi_result?!count:!valid)) || (main_view.section==PAGE_01_DETAIL_SECTION_C && !rejected);
    set_visible(main_view.empty,empty);
    if(empty)set_text(main_view.empty,ui_tr(main_view.section==PAGE_01_DETAIL_SECTION_C?
        "No rejected notes":main_view.section==PAGE_01_DETAIL_SECTION_B?"No serial records":"No notes counted"));
    if(main_view.section!=PAGE_01_DETAIL_SECTION_A && !multi_result)lv_obj_scroll_to_x(main_view.detail,0,LV_ANIM_OFF);
}
void panel_main_refresh(uint32_t dirty_flags)
{
    (void)dirty_flags;if(!main_view.panel.root || !main_view.visible)return;
    const counting_sim_t *data=counting_data_current();if(!data)return;
    main_view.section=page_01_detail_section_get();
    refresh_fonts();update_fault();refresh_header();
    char amount[64],pcs[32],code[4];currency_state_get_effective_code(code);
    const char *symbol=currency_metadata_symbol(code);
    bool multi_result=currency_state_multi_selected() || data->multi_currency_result;
    const counting_multi_t *multi=counting_multi_current();
    bool monetary=!multi_result && machine_state_mode()!=MODE_CNT;
    if(monetary)format_amount_with_comma(amount,sizeof(amount),data->total_amount);
    else snprintf(amount,sizeof(amount),"%u",multi_result?multi->total_pcs:(unsigned)data->total_pcs);
    set_text(main_view.amount_title,ui_tr(monetary?"Total amount":"Total count"));
    set_text(main_view.pcs_title,ui_tr(multi_result?"Currencies":monetary?"PCS":"Rejected"));
    snprintf(pcs,sizeof(pcs),"%u",multi_result?multi->count:monetary?(unsigned)data->total_pcs:(unsigned)counting_data_reject_pcs_count(data));
    set_text(main_view.amount,amount);set_text(main_view.pcs,pcs);
    set_text(main_view.symbol,monetary&&symbol?symbol:"");
    unsigned size=strlen(amount)>7?62:76;
    lv_obj_set_style_text_font(main_view.amount,ui_panel_font(size,true),0);
    lv_obj_set_style_text_letter_space(main_view.amount,-3,0);
    lv_obj_set_width(main_view.symbol,LV_SIZE_CONTENT);lv_obj_update_layout(main_view.symbol);
    lv_obj_set_x(main_view.amount,symbol&&monetary?59+lv_obj_get_width(main_view.symbol):49);
    lv_obj_set_y(main_view.amount,109);
    lv_obj_set_width(main_view.amount,558-lv_obj_get_x(main_view.amount)-18);
    refresh_details(data,symbol);refresh_footer(data);panel_main_refresh_start_state();
}
void panel_main_refresh_start_state(void)
{
    if(!main_view.panel.root || !main_view.visible)return;
    bool running=app_command_runtime_counting_confirmed();
    if(running!=main_view.running){main_view.running=running;main_view.revision++;}
    update_fault();refresh_keys();refresh_motion();
}
static void deleted(lv_event_t *event)
{
    if(lv_event_get_code(event)!=LV_EVENT_DELETE)return;
    panel_entry_close_for(&main_view);panel_choice_close_for(&main_view);
    main_view.visible=false;
}
bool panel_main_create(lv_obj_t *parent)
{
    if(main_view.panel.root)return true;
    memset(&main_view,0,sizeof(main_view));main_view.section=page_01_detail_section_get();
    if(!ui_panel_create(&main_view.panel,parent,action,&main_view))return false;
    lv_obj_add_event_cb(main_view.panel.root,deleted,LV_EVENT_DELETE,NULL);
    lv_obj_t *content=ui_panel_content(&main_view.panel);
    box(content,28,56,736,1,0xDCE4EC,0);box(content,28,362,736,1,0xDCE4EC,0);
    main_view.currency=label(content,"",28,15,135,21,0x294D6B,true);
    lv_obj_add_flag(main_view.currency,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(main_view.currency,click,LV_EVENT_CLICKED,(void *)A_CURRENCY);
    main_view.currency_chevron=ui_panel_icon_create(content,"chevrons-up-down",14,lv_color_hex(0x7792AA));
    lv_obj_set_pos(main_view.currency_chevron,90,21);
    main_view.work=label(content,"",575,21,100,12,0x6A7D8F,false);
    lv_obj_add_flag(main_view.work,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(main_view.work,click,LV_EVENT_CLICKED,(void *)A_WORK);
    main_view.mode=label(content,"",667,21,40,12,0x3E566D,false);
    lv_obj_add_flag(main_view.mode,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(main_view.mode,click,LV_EVENT_CLICKED,(void *)A_MODE_PICK);
    main_view.set=button(content,ui_tr("Set"),719,13,45,30,A_SETTINGS,12);
    lv_obj_set_style_border_width(main_view.set,1,0);lv_obj_set_style_border_color(main_view.set,lv_color_hex(0xD3DEE9),0);
    lv_obj_t *card=box(content,28,72,736,145,0xFFFFFF,14);
    lv_obj_set_style_border_width(card,1,0);lv_obj_set_style_border_color(card,lv_color_hex(0xE1E8EF),0);
    lv_obj_set_style_shadow_width(card,6,0);lv_obj_set_style_shadow_ofs_y(card,2,0);
    lv_obj_set_style_shadow_color(card,lv_color_hex(0x304A66),0);lv_obj_set_style_shadow_opa(card,16,0);
    main_view.amount_title=label(content,ui_tr("Total amount"),49,90,480,13,0x6A7D8F,false);
    main_view.symbol=label(content,"",49,147,32,35,0x216BD1,false);
    main_view.amount=label(content,"0",81,109,460,76,0x1D354D,true);
    lv_obj_set_style_text_letter_space(main_view.amount,-3,0);
    box(content,558,91,1,107,0xE0E7EE,0);
    main_view.pcs_title=label(content,ui_tr("PCS"),583,91,160,13,0x6A7D8F,false);
    main_view.pcs=label(content,"0",583,115,160,59,0x326491,false);
    lv_obj_set_style_text_letter_space(main_view.pcs,-2,0);
    lv_obj_t *tabbase=box(content,28,229,204,33,0xE4EAF0,8);
    lv_obj_set_style_border_width(tabbase,1,0);lv_obj_set_style_border_color(tabbase,lv_color_hex(0xD7E0E8),0);
    for(unsigned i=0;i<3;i++){
        main_view.tabs[i]=button(tabbase,"",4+(int)i*66,4,64,25,A_TAB_A+i,12);
        main_view.tab_text[i]=lv_obj_get_child(main_view.tabs[i],0);
        lv_obj_set_style_radius(main_view.tabs[i],5,0);
    }
    main_view.reject_hint=label(content,"",534,238,230,11,0x6A7D8F,false);
    lv_obj_set_style_text_align(main_view.reject_hint,LV_TEXT_ALIGN_RIGHT,0);
    main_view.detail=box(content,28,271,736,79,0xFFFFFF,12);
    lv_obj_set_style_clip_corner(main_view.detail,true,0);
    lv_obj_set_style_border_width(main_view.detail,1,0);lv_obj_set_style_border_color(main_view.detail,lv_color_hex(0xE1E8EF),0);
    lv_obj_add_flag(main_view.detail,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(main_view.detail,LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(main_view.detail,LV_SCROLLBAR_MODE_OFF);
    for(unsigned i=0;i<COUNTING_MULTI_MAX;i++){
        main_view.cells[i]=box(main_view.detail,(int)i*184,0,184,77,0xFFFFFF,0);
        lv_obj_add_event_cb(main_view.cells[i],click,LV_EVENT_CLICKED,(void *)(uintptr_t)(A_CURRENCY_DETAIL+i));
        if(i+1<COUNTING_MULTI_MAX)box(main_view.cells[i],183,0,1,77,0xE6ECF1,0);
        main_view.denom[i]=label(main_view.cells[i],"",16,14,83,13,0x63819A,false);
        main_view.denom_pcs[i]=label(main_view.cells[i],"",95,14,57,22,0x304C63,false);
        lv_obj_set_style_text_align(main_view.denom_pcs[i],LV_TEXT_ALIGN_RIGHT,0);
        main_view.denom_unit[i]=label(main_view.cells[i],ui_tr("PCS"),155,23,27,11,0x6A7D8F,false);
        main_view.denom_amount[i]=label(main_view.cells[i],"",16,48,151,12,0x7A8E9F,false);
        lv_obj_set_style_text_align(main_view.denom_amount[i],LV_TEXT_ALIGN_RIGHT,0);
    }
    for(unsigned i=0;i<4;i++){
        int x=16+(int)(i%2)*369,y=12+(int)(i/2)*28;
        main_view.serial_no[i]=label(main_view.detail,"",x,y,55,12,0x7B8D9E,false);
        main_view.serial_text[i]=label(main_view.detail,"",x+62,y-2,277,16,0x3A5972,false);
        lv_obj_set_style_text_align(main_view.serial_text[i],LV_TEXT_ALIGN_RIGHT,0);
    }
    main_view.empty=label(main_view.detail,"",16,28,702,13,0x6A7D8F,false);
    lv_obj_set_style_text_align(main_view.empty,LV_TEXT_ALIGN_CENTER,0);
    main_view.reject_text=label(main_view.detail,"",16,28,702,13,0x6A7D8F,false);
    lv_obj_set_style_text_align(main_view.reject_text,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_add_flag(main_view.reject_text,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(main_view.reject_text,click,LV_EVENT_CLICKED,(void *)A_REJECT_LIST);
    main_view.footer_batch=label(content,"",28,374,300,12,0x6A7D8F,false);
    main_view.footer_add=label(content,"",520,374,80,12,0x6A7D8F,false);
    main_view.footer_cfd=label(content,"",600,374,80,12,0x6A7D8F,false);
    main_view.footer_operator=label(content,"",680,374,84,12,0x6A7D8F,false);
    main_view.visible=true;panel_main_refresh(PAGE_01_MAIN_DIRTY_ALL);return true;
}
void panel_main_destroy(void)
{
    panel_entry_close_for(&main_view);panel_choice_close_for(&main_view);
    ui_panel_destroy(&main_view.panel);memset(&main_view,0,sizeof(main_view));
}
void panel_main_set_visible(bool visible)
{
    if(!main_view.panel.root)return;
    if(!visible){panel_entry_close_for(&main_view);panel_choice_close_for(&main_view);main_view.picker=PICK_NONE;}
    main_view.visible=visible;ui_panel_set_visible(&main_view.panel,visible);
    if(visible)panel_main_refresh(PAGE_01_MAIN_DIRTY_ALL);
}
lv_obj_t *panel_main_root(void){return main_view.panel.root;}
lv_obj_t *panel_main_scroll_obj(void){return main_view.detail;}
