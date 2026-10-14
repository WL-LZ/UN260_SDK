#include "page_receipts.h"
#include "lv_page_manager.h"
#include "page_19_history.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_components/lv_settings.h"
#include "un260/font/manrope_fonts.h"
#include "un260/font/ui_message_font.h"
#include "un260/lv_system/ui_i18n.h"
#define RECEIPTS_INK 0x20313B
#define RECEIPTS_MUTED 0x627583
#define RECEIPTS_BLUE 0x1462CC
#define RECEIPTS_PANEL 0xF2F5F8
LV_FONT_DECLARE(lv_font_instrument_sans_medium_15);
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/lv_system/ui_history_export_data.h"
#include "un260/counting/counting_history_service.h"
#include "un260/app_service/receipt_print_service.h"
#include <stdio.h>
#include <string.h>
#define RECEIPT_ROWS 4
static lv_obj_t *receipts_box(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius)
{lv_obj_t *o=lv_settings_box(p,x,y,w,h,color);lv_obj_set_style_radius(o,radius,0);return o;}
static lv_obj_t *receipts_label(lv_obj_t *p,int x,int y,int w,const char *s,const lv_font_t *font,uint32_t color)
{lv_obj_t *o=lv_settings_label(p,s,x,y,ui_message_font(font),color);lv_obj_set_width(o,w);lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);return o;}
static lv_obj_t *receipts_button(lv_obj_t *p,int x,int y,int w,int h,const char *s,bool primary,lv_event_cb_t cb,void *data)
{lv_obj_t *o=lv_settings_button(p,x,y,w,h,s,primary,cb,data);return o;}
static struct {
    lv_obj_t *root,*rows[RECEIPT_ROWS],*row_meta[RECEIPT_ROWS],*range,*empty;
    lv_obj_t *summary,*amount,*hint,*print,*export,*details,*previous,*next;
    lv_obj_t *output,*print_status,*path;
    lv_timer_t *timer;
    uint32_t ids[UI_HISTORY_MAX_RECORDS],row_ids[RECEIPT_ROWS],selected;
    unsigned count,page;
} receipts;
static uint32_t saved_selection;
static unsigned saved_page;
static void receipts_update(lv_obj_t *o,const char *s){if(strcmp(lv_label_get_text(o),s))lv_label_set_text(o,s);}
static void receipts_action(lv_event_t *e);
static void receipts_refresh(void)
{
    if(!receipts.root||lv_obj_has_flag(receipts.root,LV_OBJ_FLAG_HIDDEN))return;
    const ui_history_store_t *store=ui_history_data_get();
    unsigned count=0;
    if(store&&ui_history_data_is_available())for(unsigned i=0;i<store->record_count&&i<UI_HISTORY_MAX_RECORDS;i++){
        if(!store->records[i].valid)continue;
        uint32_t id=store->records[i].record_no;unsigned at=count++;
        while(at&&receipts.ids[at-1]<id){receipts.ids[at]=receipts.ids[at-1];--at;}receipts.ids[at]=id;
    }
    /* Preserve the first visible identity when newer records arrive. */
    if(receipts.row_ids[0]&&count!=receipts.count)
        for(unsigned i=0;i<count;i++)if(receipts.ids[i]==receipts.row_ids[0]){receipts.page=i/RECEIPT_ROWS;break;}
    receipts.count=count;if(receipts.page*RECEIPT_ROWS>=count)receipts.page=count?(count-1)/RECEIPT_ROWS:0;
    char text[256];
    for(unsigned i=0;i<RECEIPT_ROWS;i++){
        unsigned index=receipts.page*RECEIPT_ROWS+i;ui_history_record_t r;
        receipts.row_ids[i]=index<count?receipts.ids[index]:0;
        bool present=receipts.row_ids[i]&&ui_history_record_get_by_no(receipts.row_ids[i],&r);
        if(!present){lv_obj_add_flag(receipts.rows[i],LV_OBJ_FLAG_HIDDEN);continue;}
        lv_obj_clear_flag(receipts.rows[i],LV_OBJ_FLAG_HIDDEN);
        snprintf(text,sizeof(text),"#%u   %s   %04u-%02u-%02u",r.record_no,r.multi.enabled?"MULTI":r.currency,r.year,r.month,r.day);
        lv_damped_button_set_text(receipts.rows[i],text);
        snprintf(text,sizeof(text),"%02u:%02u:%02u / %u",r.hour,r.minute,r.second,r.pcs);receipts_update(receipts.row_meta[i],text);
        lv_settings_damped_choice_style(receipts.rows[i],r.record_no==receipts.selected);
    }
    snprintf(text,sizeof(text),"%u-%u / %u",count?receipts.page*RECEIPT_ROWS+1:0,LV_MIN(count,(receipts.page+1)*RECEIPT_ROWS),count);receipts_update(receipts.range,text);
    if(count)lv_obj_add_flag(receipts.empty,LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(receipts.empty,LV_OBJ_FLAG_HIDDEN);
    lv_damped_button_set_enabled(receipts.previous,receipts.page>0);
    lv_damped_button_set_enabled(receipts.next,(receipts.page+1)*RECEIPT_ROWS<count);
    ui_history_record_t record;
    bool selected=ui_history_data_is_available()&&receipts.selected&&ui_history_record_get_by_no(receipts.selected,&record);
    if(selected){
        snprintf(text,sizeof(text),"#%u / %04u-%02u-%02u %02u:%02u:%02u",record.record_no,record.year,record.month,record.day,record.hour,record.minute,record.second);receipts_update(receipts.summary,text);
        if(record.multi.enabled)snprintf(text,sizeof(text),"MULTI / %u",record.pcs);
        else snprintf(text,sizeof(text),"%s  %llu",record.currency,(unsigned long long)(record.detail_bytes?record.detail_amount:record.amount));
        receipts_update(receipts.amount,text);
    }else {receipts_update(receipts.summary,ui_tr("Select a saved record"));receipts_update(receipts.amount,"");}
    const char *blocker=selected?receipt_print_blocker(receipts.selected):UI_N_("Select a saved record");
    receipts_update(receipts.hint,ui_tr(blocker?blocker:UI_N_("This is the current controller result. Check the paper after printing.")));
    lv_damped_button_set_enabled(receipts.print,selected&&!blocker);
    const history_export_status_t *output=ui_history_export_data_status();
    lv_damped_button_set_enabled(receipts.export,selected&&output->state!=HISTORY_EXPORT_WRITING);
    lv_damped_button_set_enabled(receipts.details,selected);
    const char *state=output->state==HISTORY_EXPORT_WRITING?UI_N_("Writing to USB"):
        output->state==HISTORY_EXPORT_SAVED?UI_N_("Saved to USB"):
        output->state==HISTORY_EXPORT_FAILED?UI_N_("Export failed"):UI_N_("No output yet");
    if(output->state!=HISTORY_EXPORT_NONE)snprintf(text,sizeof(text),"%s / #%u / %zu",ui_tr(state),output->first_record,output->record_count);
    else snprintf(text,sizeof(text),"%s",ui_tr(state));receipts_update(receipts.output,text);
    const char *name=strrchr(output->first_html_path,'/');receipts_update(receipts.path,name?name+1:output->first_html_path);
    const receipt_print_status_t *printed=receipt_print_status();
    state=printed->state==RECEIPT_PRINT_SENT?UI_N_("Print sent; paper output unverified"):
        printed->state==RECEIPT_PRINT_FAILED?UI_N_("Print request not sent"):UI_N_("No print request yet");
    if(printed->record_no)snprintf(text,sizeof(text),"%s / #%u",ui_tr(state),printed->record_no);else snprintf(text,sizeof(text),"%s",ui_tr(state));
    receipts_update(receipts.print_status,text);
}
static void receipts_tick(lv_timer_t *timer)
{
    (void)timer;lv_indev_t *input=lv_indev_get_next(NULL);
    if(input&&input->proc.state==LV_INDEV_STATE_PRESSED)return;
    receipts_refresh();
}
static void receipts_action(lv_event_t *e)
{
    unsigned id=(uintptr_t)lv_event_get_user_data(e);
    if(id==0){ui_manager_pop_page();return;}
    if(id>=10&&id<10+RECEIPT_ROWS)receipts.selected=receipts.row_ids[id-10];
    if(id==1&&receipts.page)--receipts.page;
    if(id==2&&(receipts.page+1)*RECEIPT_ROWS<receipts.count)++receipts.page;
    if(id==3){
        const char *reason=receipt_print_blocker(receipts.selected);
        if(!receipts.selected||reason)ui_notice_post_text(UI_NOTICE_WARNING,"print.request",UI_N_("Receipts"),reason?reason:UI_N_("Select a saved record"));
        else {bool ok=receipt_print_request(receipts.selected);ui_notice_post_text(ok?UI_NOTICE_INFO:UI_NOTICE_ERROR,"print.request",UI_N_("Receipts"),ok?UI_N_("Print sent; paper output unverified"):UI_N_("Print request not sent"));}
    }
    if(id==4&&receipts.selected)(void)ui_history_export_data_request_records(&receipts.selected,1);
    if(id==5&&receipts.selected){ui_page_19_history_open_record(receipts.selected);return;}
    receipts_refresh();
}
static void receipts_deleted(lv_event_t *e)
{
    (void)e;if(receipts.timer)lv_timer_del(receipts.timer);
    saved_selection=receipts.selected;saved_page=receipts.page;memset(&receipts,0,sizeof(receipts));
}
void ui_receipts_create(lv_obj_t *parent)
{
    if(receipts.root)return;ui_history_data_init();
    lv_settings_header_t h={.title=ui_tr("Receipts"),.subtitle=ui_tr("Saved results, USB copies and print requests"),.back=receipts_action};
    lv_settings_frame_t f=lv_settings_frame_create(parent?parent:lv_scr_act(),&h);receipts.root=f.root;
    lv_obj_add_event_cb(receipts.root,receipts_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_set_style_bg_opa(f.body,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(f.body,0,0);
    lv_obj_t *left=receipts_box(f.body,0,0,466,300,RECEIPTS_PANEL,18),*right=receipts_box(f.body,482,0,750,176,RECEIPTS_PANEL,18);
    receipts_label(left,16,10,420,ui_tr("Saved records"),&lv_font_instrument_sans_semibold_18,RECEIPTS_INK);
    for(unsigned i=0;i<RECEIPT_ROWS;i++){
        receipts.rows[i]=receipts_button(left,12,41+i*49,442,45,"",false,receipts_action,(void*)(uintptr_t)(10+i));
        lv_obj_t *name=lv_damped_button_get_label(receipts.rows[i]);lv_obj_set_width(name,406);lv_obj_align(name,LV_ALIGN_TOP_LEFT,6,2);
        lv_obj_set_style_text_font(name,ui_message_font(&lv_font_instrument_sans_medium_15),0);
        receipts.row_meta[i]=receipts_label(receipts.rows[i],12,25,400,"",&lv_font_instrument_sans_medium_12,RECEIPTS_MUTED);
    }
    receipts.empty=receipts_label(left,24,110,418,ui_tr("No saved records"),&lv_font_instrument_sans_medium_18,RECEIPTS_MUTED);
    receipts.range=receipts_label(left,16,263,175,"",&lv_font_instrument_sans_medium_14,RECEIPTS_MUTED);
    receipts.previous=receipts_button(left,192,250,126,42,ui_tr("Previous"),false,receipts_action,(void*)1);
    receipts.next=receipts_button(left,330,250,124,42,ui_tr("Next"),false,receipts_action,(void*)2);
    receipts.summary=receipts_label(right,20,12,710,"",&lv_font_instrument_sans_medium_15,RECEIPTS_MUTED);
    receipts.amount=receipts_label(right,20,38,710,"",&lv_font_instrument_sans_semibold_24,RECEIPTS_INK);
    receipts.hint=receipts_label(right,20,90,710,"",&lv_font_instrument_sans_medium_14,RECEIPTS_MUTED);
    lv_label_set_long_mode(receipts.hint,LV_LABEL_LONG_WRAP);
    receipts.print=receipts_button(right,20,126,220,42,ui_tr("Print"),false,receipts_action,(void*)3);
    receipts.export=receipts_button(right,252,126,226,42,ui_tr("Export"),true,receipts_action,(void*)4);
    receipts.details=receipts_button(right,490,126,240,42,ui_tr("Details"),false,receipts_action,(void*)5);
    lv_obj_t *status=receipts_box(f.body,482,188,750,112,0xFFFFFF,14);
    receipts_label(status,20,0,710,ui_tr("Latest output this session"),&lv_font_instrument_sans_semibold_16,RECEIPTS_INK);
    receipts.output=receipts_label(status,20,27,710,"",&lv_font_instrument_sans_medium_14,RECEIPTS_BLUE);
    receipts.path=receipts_label(status,20,48,710,"",&lv_font_instrument_sans_medium_12,RECEIPTS_MUTED);
    lv_label_set_long_mode(receipts.path,LV_LABEL_LONG_DOT);
    receipts.print_status=receipts_label(status,20,82,710,"",&lv_font_instrument_sans_medium_14,RECEIPTS_MUTED);
    receipts.selected=saved_selection?saved_selection:counting_history_current_record();receipts.page=saved_page;
    receipts.timer=lv_timer_create(receipts_tick,250,NULL);receipts_refresh();
}
void ui_receipts_suspend(void){if(receipts.root){if(receipts.timer)lv_timer_pause(receipts.timer);lv_obj_add_flag(receipts.root,LV_OBJ_FLAG_HIDDEN);}}
bool ui_receipts_resume(void){if(!receipts.root)return false;lv_obj_clear_flag(receipts.root,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(receipts.root);if(receipts.timer)lv_timer_resume(receipts.timer);receipts_refresh();return true;}
void ui_receipts_destroy(void){if(receipts.root)lv_obj_del(receipts.root);}
