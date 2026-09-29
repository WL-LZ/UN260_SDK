#include "lv_fault_popup.h"
#include "fault_guide/fault_guide_catalog.h"
#include "fault_guide/machine_fault_view.h"
#include "ui_notice.h"
#include "smart_island.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/font/manrope_fonts.h"
#include "un260/font/ui_message_font.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    lv_obj_t *overlay,*card,*title,*code,*location,*step_title,*step_body;
    lv_obj_t *step_base,*steps[3],*step_labels[3],*previous,*next,*step_count;
    lv_obj_t *view_label,*pause_label,*queue_button,*queue_label,*queue;
    machine_fault_view_t model;
    machine_fault_key_t key;
    mf_guide_t guide;
    uint8_t step;
    lv_obj_t *safety,*confirm_label;
} fault_popup_t;
static fault_popup_t popup;
static bool auto_enabled=true;
static fault_popup_confirm_handler_t confirm_handler;
void fault_popup_set_confirm_handler(fault_popup_confirm_handler_t handler)
{
    confirm_handler=handler;
}
static void present(machine_fault_key_t key);
static void render_step(bool restart_animation);
static const char *text(mf_text_t value) {return fault_guide_text(value);}
static void post_island_notice(machine_fault_key_t key)
{
    mf_guide_t guide;fault_guide_lookup(key,&guide);
    const char *description = key.source == MACHINE_FAULT_START && key.type == 2 ? machine_start_error_desc(key.code) :
        key.source == MACHINE_FAULT_RUNTIME ? machine_runtime_error_desc(key.code) : NULL;
    smart_island_notify_fault(description ? ui_tr(description) : text(guide.title),key);
}
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_radius(o,radius,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;
}
static lv_obj_t *label(lv_obj_t *parent,const char *value,int x,int y,int w,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *o=lv_label_create(parent);lv_label_set_text(o,value);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,ui_message_font(font),0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);return o;
}
static lv_obj_t *button(lv_obj_t *parent,int x,int y,int w,int h,const char *value,bool primary,lv_event_cb_t cb,void *data,lv_obj_t **copy)
{
    lv_obj_t *o=lv_btn_create(parent);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_settings_action_style(o,primary?LV_SETTINGS_ACTION_PRIMARY:LV_SETTINGS_ACTION_SECONDARY);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_radius(o,12,0);lv_obj_set_style_pad_all(o,0,0);
    lv_obj_t *l=label(o,value,0,0,w,&lv_font_instrument_sans_medium_18,primary?0xFFFFFF:0x586B78);
    lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(l);
    if(cb)lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,data);
    if(copy)*copy=l;
    return o;
}
static void icon(lv_obj_t *parent,const char *value,int x,int y,int w,int h,lv_event_cb_t cb,lv_obj_t **out)
{
    lv_obj_t *copy;button(parent,x,y,w,h,value,false,cb,NULL,&copy);
    lv_obj_set_style_text_font(copy,&lv_font_montserrat_16,0);if(out)*out=copy;
}
static void deleted(lv_event_t *event)
{
    (void)event;
    if(popup.model.timer)lv_timer_del(popup.model.timer);
    memset(&popup,0,sizeof(popup));
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_FAULT,false);
}
void hide_fault_popup(void)
{
    if(!popup.overlay)return;
    machine_fault_view_destroy(&popup.model);
    lv_obj_del(popup.overlay);memset(&popup,0,sizeof(popup));
}
bool fault_popup_is_showing(void) {return popup.overlay!=NULL;}
static void confirm(lv_event_t *event)
{
    (void)event;
    if(!popup.overlay)return;
    machine_fault_key_t confirmed=popup.key;
    machine_fault_acknowledge(confirmed);
    if(confirm_handler && confirm_handler(confirmed))return;
    machine_fault_record_t next_record;
    if(machine_fault_first_unread(&next_record))present(next_record.key);else {
        machine_fault_key_t key=popup.key;
        hide_fault_popup();
        post_island_notice(key);
    }
}
static void select_step(lv_event_t *event)
{
    unsigned step=(unsigned)(uintptr_t)lv_event_get_user_data(event);
    if(step>=popup.guide.step_count || step==popup.step)return;
    popup.step=step;render_step(true);
}
static void previous(lv_event_t *event) {(void)event;if(popup.step){--popup.step;render_step(true);}}
static void next(lv_event_t *event) {(void)event;if(popup.step+1<popup.guide.step_count){++popup.step;render_step(true);}}
static void pause_play(lv_event_t *event)
{
    (void)event;machine_fault_view_play(&popup.model,!popup.model.playing);
    lv_label_set_text(popup.pause_label,popup.model.playing?LV_SYMBOL_PAUSE:LV_SYMBOL_PLAY);
}
static void replay(lv_event_t *event)
{
    (void)event;machine_fault_view_restart(&popup.model);lv_label_set_text(popup.pause_label,LV_SYMBOL_PAUSE);
}
static void select_issue(lv_event_t *event)
{
    machine_fault_record_t record;
    if(machine_fault_at((size_t)(uintptr_t)lv_event_get_user_data(event),&record))present(record.key);
}
static void toggle_queue(lv_event_t *event)
{
    (void)event;
    if(popup.queue){lv_obj_del(popup.queue);popup.queue=NULL;return;}
    popup.queue=box(popup.card,756,79,426,244,0xFFFFFF,14);
    lv_obj_set_style_border_width(popup.queue,1,0);lv_obj_set_style_border_color(popup.queue,lv_color_hex(0xDCE4E8),0);
    lv_obj_add_flag(popup.queue,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(popup.queue,LV_DIR_VER);
    machine_fault_record_t record;
    for(size_t i=0;machine_fault_at(i,&record);++i) {
        mf_guide_t guide;fault_guide_lookup(record.key,&guide);
        lv_obj_t *copy;button(popup.queue,8,8+(int)i*48,410,42,text(guide.title),false,select_issue,(void *)(uintptr_t)i,&copy);
        lv_obj_set_style_text_font(copy,ui_message_font(&lv_font_instrument_sans_medium_16),0);
    }
    lv_obj_move_foreground(popup.queue);
}
static void update_queue(void)
{
    if(popup.queue){lv_obj_del(popup.queue);popup.queue=NULL;}
    size_t count=machine_fault_count();
    if(count<2){lv_obj_add_flag(popup.queue_button,LV_OBJ_FLAG_HIDDEN);return;}
    char buf[32];snprintf(buf,sizeof(buf),ui_tr("%u issues"),(unsigned)count);
    lv_label_set_text(popup.queue_label,buf);lv_obj_center(popup.queue_label);lv_obj_clear_flag(popup.queue_button,LV_OBJ_FLAG_HIDDEN);
}
static void render_step(bool restart_animation)
{
    const mf_step_t *s=&popup.guide.steps[popup.step];
    bool single=popup.guide.step_count==1;
    if(single)lv_obj_add_flag(popup.step_base,LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(popup.step_base,LV_OBJ_FLAG_HIDDEN);
    for(unsigned i=0;i<3;++i) {
        if(i>=popup.guide.step_count){lv_obj_add_flag(popup.steps[i],LV_OBJ_FLAG_HIDDEN);continue;}
        lv_obj_clear_flag(popup.steps[i],LV_OBJ_FLAG_HIDDEN);
        int width=(616-8-(popup.guide.step_count-1)*4)/popup.guide.step_count;
        lv_obj_set_pos(popup.steps[i],4+(width+4)*i,4);lv_obj_set_size(popup.steps[i],width,40);
        lv_obj_set_style_bg_color(popup.steps[i],lv_color_hex(i==popup.step?0xFFFFFF:0xE7EDF0),0);
        lv_obj_set_style_text_color(popup.step_labels[i],lv_color_hex(i==popup.step?0x1559B7:0x586B78),0);
        lv_obj_set_width(popup.step_labels[i],width);
        char buf[72];snprintf(buf,sizeof(buf),"%02u  %s",i+1,text(popup.guide.steps[i].short_title));
        lv_label_set_text(popup.step_labels[i],buf);lv_obj_center(popup.step_labels[i]);
    }
    lv_label_set_text(popup.step_title,text(s->title));lv_label_set_text(popup.step_body,text(s->body));
    lv_obj_set_y(popup.step_title,single?112:177);lv_obj_set_y(popup.step_body,single?153:218);
    const char *location=text(popup.guide.location);
    if(popup.key.source==MACHINE_FAULT_START && popup.key.code==8)
        location=s->zone==MF_REJECT?ui_tr("Upper front · Reject pocket"):ui_tr("Lower front · Stacker pocket");
    else if(popup.key.source==MACHINE_FAULT_START && popup.key.code==9)
        location=s->view==MF_REAR?ui_tr("Rear · Lower passage"):ui_tr("Top · Upper passage");
    lv_label_set_text(popup.location,location);
    static const char *const names[]={UI_N_("FRONT"),UI_N_("UPPER PASSAGE"),UI_N_("REAR"),UI_N_("INTERNAL SIDE")};
    lv_label_set_text(popup.view_label,ui_tr(names[s->view]));
    lv_obj_t *pager[]={popup.previous,popup.next,popup.step_count};
    for(unsigned i=0;i<3;++i){if(single)lv_obj_add_flag(pager[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(pager[i],LV_OBJ_FLAG_HIDDEN);}
    if(popup.step==0)lv_obj_add_state(popup.previous,LV_STATE_DISABLED);else lv_obj_clear_state(popup.previous,LV_STATE_DISABLED);
    if(popup.step+1==popup.guide.step_count)lv_obj_add_state(popup.next,LV_STATE_DISABLED);else lv_obj_clear_state(popup.next,LV_STATE_DISABLED);
    lv_label_set_text_fmt(popup.step_count,"%02u / %02u",popup.step+1,popup.guide.step_count);
    if(restart_animation){machine_fault_view_set_step(&popup.model,s);lv_label_set_text(popup.pause_label,LV_SYMBOL_PAUSE);}
}
static void present(machine_fault_key_t key)
{
    if(popup.overlay && machine_fault_key_equal(popup.key,key)) {update_queue();return;}
    hide_fault_popup();popup.key=key;fault_guide_lookup(key,&popup.guide);
    ui_notice_set_suspended(UI_NOTICE_SUSPEND_FAULT,true);
    popup.overlay=box(lv_layer_top(),0,0,lv_disp_get_hor_res(NULL),lv_disp_get_ver_res(NULL),0x243C52,0);
    lv_obj_set_style_bg_opa(popup.overlay,97,0);lv_obj_add_flag(popup.overlay,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(popup.overlay,deleted,LV_EVENT_DELETE,NULL);
    popup.card=box(popup.overlay,0,0,1208,370,0xFFFFFF,22);lv_obj_center(popup.card);
    lv_obj_t *stage=box(popup.card,26,20,510,273,0xF1F5F7,16);
    machine_fault_view_create(&popup.model,stage,0,0);
    popup.view_label=label(stage,"",16,19,290,&lv_font_instrument_sans_medium_12,0x6F818E);
    icon(stage,LV_SYMBOL_REFRESH,414,10,38,38,replay,NULL);icon(stage,LV_SYMBOL_PAUSE,456,10,38,38,pause_play,&popup.pause_label);
    popup.location=label(popup.card,"",28,302,506,&lv_font_instrument_sans_medium_14,0x945329);
    popup.safety=label(popup.card,ui_tr("Wait for all moving parts to stop before handling."),28,327,506,&lv_font_instrument_sans_medium_12,0x586B78);
    lv_obj_t *alert=box(popup.card,566,24,43,43,0xFBEFE5,13);
    lv_obj_t *mark=label(alert,LV_SYMBOL_WARNING,0,0,43,&lv_font_montserrat_24,0xAE5728);lv_obj_set_style_text_align(mark,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(mark);
    popup.title=label(popup.card,text(popup.guide.title),622,24,556,&lv_font_instrument_sans_semibold_28,0x1D2B34);
    char code[48];fault_guide_format_code(key,code,sizeof(code));
    popup.code=label(popup.card,code,622,87,285,&lv_font_instrument_sans_medium_12,0x748793);
    popup.queue_button=button(popup.card,1062,75,120,28,"",false,toggle_queue,NULL,&popup.queue_label);
    lv_obj_set_style_text_font(popup.queue_label,ui_message_font(&lv_font_instrument_sans_medium_12),0);
    popup.step_base=box(popup.card,566,111,616,48,0xE7EDF0,12);
    for(unsigned i=0;i<3;++i) {
        popup.steps[i]=button(popup.step_base,0,4,190,40,"",false,select_step,(void *)(uintptr_t)i,&popup.step_labels[i]);
        lv_obj_set_style_radius(popup.steps[i],9,0);lv_obj_set_style_text_font(popup.step_labels[i],ui_message_font(&lv_font_instrument_sans_medium_14),0);
    }
    popup.step_title=label(popup.card,"",566,177,612,&lv_font_instrument_sans_medium_24,0x1D2B34);
    popup.step_body=label(popup.card,"",566,218,605,&lv_font_instrument_sans_medium_18,0x586B78);
    lv_obj_set_style_text_line_space(popup.step_body,5,0);
    popup.previous=button(popup.card,566,307,42,42,LV_SYMBOL_LEFT,false,previous,NULL,NULL);
    popup.next=button(popup.card,676,307,42,42,LV_SYMBOL_RIGHT,false,next,NULL,NULL);
    lv_obj_set_style_text_font(lv_obj_get_child(popup.previous,0),&lv_font_montserrat_16,0);
    lv_obj_set_style_text_font(lv_obj_get_child(popup.next,0),&lv_font_montserrat_16,0);
    popup.step_count=label(popup.card,"",617,321,50,&lv_font_instrument_sans_medium_12,0x7A8D99);
    button(popup.card,1022,302,160,48,ui_tr("Confirm"),true,confirm,NULL,&popup.confirm_label);
    update_queue();render_step(true);lv_obj_move_foreground(popup.overlay);
}
void fault_popup_language_changed(void)
{
    if(!popup.overlay)return;
    bool queue_open=popup.queue!=NULL;
    lv_coord_t queue_scroll_y=queue_open?lv_obj_get_scroll_y(popup.queue):0;
    lv_label_set_text(popup.title,text(popup.guide.title));
    lv_label_set_text(popup.safety,ui_tr("Wait for all moving parts to stop before handling."));
    lv_label_set_text(popup.confirm_label,ui_tr("Confirm"));lv_obj_center(popup.confirm_label);
    update_queue();
    if(queue_open){
        toggle_queue(NULL);
        lv_obj_update_layout(popup.queue);
        lv_obj_scroll_to_y(popup.queue,queue_scroll_y,LV_ANIM_OFF);
    }
    render_step(false);
}
static void report(machine_fault_key_t key,bool allow_auto)
{
    bool fresh=machine_fault_report(key);
    if(!fresh){
        if(allow_auto && !popup.overlay && !auto_enabled)post_island_notice(key);
        return;
    }
    smart_island_faults_changed();
    smart_island_refresh_summary();
    if(allow_auto && auto_enabled)present(key);
    else if(popup.overlay && !machine_fault_find(popup.key,NULL))present(key);
    else {
        if(popup.overlay)update_queue();
        if(allow_auto && !auto_enabled)post_island_notice(key);
    }
}
void fault_popup_restore_island_notice(void)
{
    if (popup.overlay || smart_island_has_active_fault()) return;
    /* Prefer the latest remaining report. Read acknowledgement is not a
     * physical recovery, so acknowledged conditions also survive recreation. */
    machine_fault_record_t record;
    for (size_t i=machine_fault_count();i>0;--i) {
        if (!machine_fault_at(i-1,&record)) continue;
        if (auto_enabled && !record.acknowledged) continue;
        if (record.key.source==MACHINE_FAULT_RUNTIME && !machine_runtime_error_desc(record.key.code)) continue;
        post_island_notice(record.key);
        return;
    }
}
static void refresh_after_clear(bool restore_notice)
{
    smart_island_faults_changed();
    smart_island_refresh_summary();
    if(!popup.overlay){if(restore_notice)fault_popup_restore_island_notice();return;}
    if(machine_fault_find(popup.key,NULL)){update_queue();return;}
    machine_fault_record_t record;
    if(machine_fault_first_unread(&record))present(record.key);else {hide_fault_popup();fault_popup_restore_island_notice();}
}
void fault_popup_set_auto_enabled(bool enabled)
{
    auto_enabled=enabled;machine_fault_record_t record;
    if(enabled && !popup.overlay && machine_fault_first_unread(&record))present(record.key);
}
bool fault_popup_get_auto_enabled(void) {return auto_enabled;}
void fault_popup_report_start_fault(uint8_t type,uint8_t code) {report((machine_fault_key_t){MACHINE_FAULT_START,type,code},true);}
void fault_popup_report_start_no_note(void)
{
    report((machine_fault_key_t){MACHINE_FAULT_START,1,2},true);
}
void fault_popup_report_batch_full(void)
{
    report((machine_fault_key_t){MACHINE_FAULT_BATCH,0,4},true);
}
void fault_popup_report_runtime_fault(uint8_t code)
{
    if(code==0){machine_fault_clear_source(MACHINE_FAULT_RUNTIME);refresh_after_clear(true);return;}
    report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,code},true);
}
void fault_popup_record_runtime_notice(uint8_t code)
{
    if(code)report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,code},false);
}
static void boot_result(uint8_t step,uint8_t result,bool allow_auto)
{
    if(result==1){machine_fault_clear_code(MACHINE_FAULT_BOOT,step);refresh_after_clear(true);return;}
    machine_fault_key_t key={MACHINE_FAULT_BOOT,result,step};
    if(machine_fault_find(key,NULL))return;
    machine_fault_clear_code(MACHINE_FAULT_BOOT,step);report(key,allow_auto);
}
void fault_popup_report_boot_result(uint8_t step,uint8_t result) {boot_result(step,result,true);}
void fault_popup_record_boot_result(uint8_t step,uint8_t result) {boot_result(step,result,false);}
void fault_popup_report_sensor_mask(uint32_t mask)
{
    machine_fault_key_t fresh={MACHINE_FAULT_SENSOR,0,32};
    for(uint8_t bit=0;bit<32;++bit) {
        machine_fault_key_t key={MACHINE_FAULT_SENSOR,0,bit};
        if((mask&(UINT32_C(1)<<bit)) && !machine_fault_find(key,NULL)){fresh=key;break;}
    }
    machine_fault_sensor_snapshot(mask);refresh_after_clear(fresh.code>=32);
    /* A recovered bit must not restart another fault's current step or open
     * unrelated unread reports. Only a newly asserted bit raises a popup. */
    if(fresh.code<32) {
        if(auto_enabled && (!popup.overlay || !machine_fault_key_equal(popup.key,fresh)))present(fresh);
        else if(!auto_enabled)post_island_notice(fresh);
    }
}
bool fault_popup_show_key(machine_fault_key_t key)
{
    if(!machine_fault_find(key,NULL))return false;
    present(key);return true;
}
bool fault_popup_show_pending_now(void)
{
    machine_fault_record_t record;
    if(!machine_fault_first_unread(&record) && !machine_fault_at(0,&record))return false;
    present(record.key);return true;
}
bool fault_popup_get_pending_fault(fault_source_t *source,uint8_t *type,uint8_t *code)
{
    machine_fault_record_t record;
    if(!machine_fault_first_unread(&record) && !machine_fault_at(0,&record))return false;
    if(source)*source=record.key.source;
    if(type)*type=record.key.type;
    if(code)*code=record.key.code;
    return true;
}
void fault_popup_clear_runtime(void)
{
    machine_fault_clear_source(MACHINE_FAULT_START);machine_fault_clear_source(MACHINE_FAULT_RUNTIME);
    machine_fault_clear_source(MACHINE_FAULT_BATCH);refresh_after_clear(true);
}
void fault_popup_stacker_cleared(void)
{
    /* 0x51/01 confirms only the genuine-note pocket, not both pockets or a jam. */
    machine_fault_key_t key={MACHINE_FAULT_START,2,7};
    if(machine_fault_find(key,NULL))machine_fault_clear_code(MACHINE_FAULT_START,7);
    machine_fault_clear_source(MACHINE_FAULT_BATCH);
    machine_fault_clear_code(MACHINE_FAULT_RUNTIME,7);
    refresh_after_clear(true);
}
