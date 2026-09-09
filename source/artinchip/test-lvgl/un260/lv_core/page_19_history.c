#include "page_19_history.h"
#include "page_19_history_search.h"
#include "lv_page_manager.h"
#include "lv_port_indev.h"
#include "un260/history/history_query.h"
#include "un260/lv_components/lv_card_surface.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/lv_recycled_list.h"
#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/lv_system/ui_history_export_data.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/ui_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HISTORY_INK 0x17212A
#define HISTORY_BODY 0x4C606E
#define HISTORY_MUTED 0x7A8D9B
#define HISTORY_LINE 0xE7ECEF
#define HISTORY_BG 0xD8E2E8
#define HISTORY_ROWS 5
#define HISTORY_ROW_HEIGHT 48
#define DETAIL_ROWS 6
#define DETAIL_ROW_HEIGHT 32
_Static_assert(UI_TEXT_PAGE_MAX <= UI_TEXT_WIDGET_BASE, "Page text IDs overlap widget IDs");

typedef struct {
    unsigned id;
    lv_obj_t *panel, *empty, *footer;
    lv_recycled_list_t *list;
} history_section_t;

typedef struct {
    lv_obj_t *root, *title, *subtitle, *summary, *notice, *lifetime, *lifetime_title;
    lv_obj_t *reset_total, *actions[4], *list_panel, *empty, *range, *sort, *unknown;
    lv_obj_t *dialog;
    lv_recycled_list_t *list;
    history_section_t sections[3];
    page_19_history_search_t *search;
    history_query_input_t input;
    history_query_t query;
    history_query_result_t result;
    history_query_record_t records[UI_HISTORY_MAX_RECORDS];
    history_record_detail_t *details[UI_HISTORY_MAX_RECORDS];
    uint32_t result_ids[UI_HISTORY_MAX_RECORDS], unknown_ids[UI_HISTORY_MAX_RECORDS];
    uint32_t selected_ids[UI_HISTORY_MAX_RECORDS], confirmed_ids[UI_HISTORY_MAX_RECORDS];
    size_t record_count, unknown_count, selected_count, confirmed_count;
    uint32_t current_id, revision, pressed_revision, pressed_id;
    lv_point_t press;
    bool detail_mode, selecting, reviewing_unknown, tapping, model_dirty, confirm_clear;
    language_t language;
} history_ctx_t;

static history_ctx_t *history;
static void refresh_records(bool reset);
static void refresh_header(void);
static void show_record(uint32_t id);
static void show_list(void);
static void show_confirmation(bool clear_total);
static void close_dialog(void);
static void action_event(lv_event_t *event);

static const char *tr(ui_text_id_t id) { return ui_text_get(id); }
static void text_set(lv_obj_t *label, const char *text)
{
    if (label && strcmp(lv_label_get_text(label), text ? text : ""))
        lv_label_set_text(label, text ? text : "");
}
static void show(lv_obj_t *obj, bool visible)
{
    if (!obj) return;
    if (visible) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
static bool visible(void)
{
    return history && history->root && !lv_obj_has_flag(history->root, LV_OBJ_FLAG_HIDDEN);
}
static lv_obj_t *surface(lv_obj_t *parent,int x,int y,int w,int h,int radius,uint32_t color)
{
    lv_obj_t *obj=lv_obj_create(parent);
    if (!obj) return NULL;
    lv_obj_remove_style_all(obj);lv_obj_set_pos(obj,x,y);
    const lv_card_surface_style_t style={w,h,radius,color,HISTORY_LINE,0};
    lv_card_surface_apply(obj,&style);
    lv_obj_clear_flag(obj,LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
static lv_obj_t *label(lv_obj_t *parent,int x,int y,int w,int h,
                       const lv_font_t *font,uint32_t color,lv_text_align_t align)
{
    lv_obj_t *obj=lv_label_create(parent);
    if (!obj) return NULL;
    lv_obj_remove_style_all(obj);lv_obj_set_pos(obj,x,y);lv_obj_set_size(obj,w,h);
    lv_obj_set_style_text_font(obj,font,0);lv_obj_set_style_text_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_text_align(obj,align,0);
    lv_label_set_long_mode(obj,LV_LABEL_LONG_CLIP);lv_label_set_text(obj,"");
    lv_obj_clear_flag(obj,LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
static lv_obj_t *button(lv_obj_t *parent,int x,int y,int w,int h,const char *text,
                        lv_event_cb_t callback,void *context)
{
    const lv_damped_button_style_t style={0xFFFFFF,0xEBEBEB,0xF6F7F8,
        HISTORY_BODY,0xAAB5BE,12};
    lv_obj_t *obj=lv_damped_button_create(parent,&style,text,&lv_font_instrument_sans_semibold_14);
    if (!obj) return NULL;
    if (!lv_damped_button_get_label(obj)) {lv_obj_del(obj);return NULL;}
    lv_obj_set_pos(obj,x,y);lv_obj_set_size(obj,w,h);
    lv_obj_set_style_border_width(obj,1,0);lv_obj_set_style_border_color(obj,lv_color_hex(HISTORY_LINE),0);
    if (!lv_obj_add_event_cb(obj,callback,LV_EVENT_CLICKED,context)) {lv_obj_del(obj);return NULL;}
    return obj;
}
static void toast(const char *text)
{
    lv_print_toast_config_t cfg=lv_print_toast_get_default_config();
    cfg.text=text;cfg.w=480;cfg.h=110;cfg.show_loader=false;cfg.align_center=true;
    cfg.auto_hide_ms=2200;lv_print_toast_show_with_config(&cfg);
}
static const ui_history_record_t *record_find(uint32_t id)
{
    const ui_history_store_t *store=ui_history_data_get();
    if (!store || !id) return NULL;
    for (unsigned i=0;i<store->record_count && i<UI_HISTORY_MAX_RECORDS;++i)
        if (store->records[i].valid && store->records[i].record_no==id) return &store->records[i];
    return NULL;
}
static int record_index(uint32_t id)
{
    for (size_t i=0;i<history->record_count;++i)
        if (history->records[i].record_no==id) return (int)i;
    return -1;
}
static void date_text(const ui_history_record_t *rec,char *out,size_t size)
{
    if (!rec || !rec->year || !rec->month || rec->month>12 || !rec->day ||
        rec->day>31 || rec->hour>23 || rec->minute>59 || rec->second>59) {
        snprintf(out,size,"%s",tr(UI_TEXT_HISTORY_TIME_UNKNOWN));return;
    }
    snprintf(out,size,"%04u-%02u-%02u %02u:%02u:%02u",(unsigned)rec->year,
        (unsigned)rec->month,(unsigned)rec->day,(unsigned)rec->hour,
        (unsigned)rec->minute,(unsigned)rec->second);
}
static void release_details(void)
{
    for (unsigned i=0;i<UI_HISTORY_MAX_RECORDS;++i) {
        if (history->details[i]) {
            history_record_detail_release(history->details[i]);
            free(history->details[i]);history->details[i]=NULL;
        }
        history->records[i].detail=NULL;
    }
}
static bool prepare_detail(unsigned index)
{
    if (index>=history->record_count) return false;
    if (history->details[index]) return true;
    const ui_history_record_t *rec=record_find(history->records[index].record_no);
    if (!rec) return false;
    history_record_detail_t *detail=calloc(1,sizeof(*detail));
    if (!detail) return false;
    const history_detail_input_t input={
        .denom_text=rec->denom_text,.sn_detail_text=rec->sn_detail_text,
        .sn_text=rec->sn_text,.session_log=rec->session_log,.error_frame_text=rec->error_frame_text,
        .total_pcs=rec->pcs,
        .denoms_truncated=strlen(rec->denom_text)>=sizeof(rec->denom_text)-1,
        .serials_truncated=strlen(rec->sn_detail_text)>=sizeof(rec->sn_detail_text)-1,
        .legacy_serials_truncated=strlen(rec->sn_text)>=sizeof(rec->sn_text)-1,
        .log_truncated=strlen(rec->session_log)>=sizeof(rec->session_log)-1,
        .reject_log_complete=false
    };
    if (!history_record_detail_build(&input,detail)) {free(detail);return false;}
    history->details[index]=detail;history->records[index].detail=detail;
    return true;
}
static bool id_selected(uint32_t id)
{
    for (size_t i=0;i<history->selected_count;++i) if (history->selected_ids[i]==id) return true;
    return false;
}
static uint32_t displayed_id(uint32_t index)
{
    size_t count=history->reviewing_unknown ? history->unknown_count : history->result.written_count;
    if (index>=count) return 0;
    return history->reviewing_unknown ? history->unknown_ids[index] : history->result_ids[index];
}
static size_t displayed_count(void)
{
    return history->reviewing_unknown ? history->unknown_count : history->result.written_count;
}
static void list_changed(const ui_list_window_t *window,void *context)
{
    (void)context;
    if (!history || !history->range) return;
    char text[64];
    snprintf(text,sizeof(text),"%lu-%lu / %lu",(unsigned long)window->first+1,
        (unsigned long)ui_list_window_last(window),(unsigned long)window->count);
    if (!window->count) snprintf(text,sizeof(text),"0 / 0");
    text_set(history->range,text);
}
static lv_obj_t *record_row_create(lv_obj_t *parent,lv_coord_t width,void *context)
{
    (void)context;
    lv_obj_t *row=surface(parent,0,0,width,HISTORY_ROW_HEIGHT,8,0xFFFFFF);
    if (!row) return NULL;
    const int x[]={16,164,432,516,670,982},w[]={140,250,76,116,194,118};
    for (unsigned i=0;i<6;++i)
        if (!label(row,x[i],14,w[i],25,i==0 ? &lv_font_instrument_sans_medium_14 :
            &lv_font_instrument_sans_medium_18,i==5 ? HISTORY_MUTED : HISTORY_BODY,
            i==3 || i==4 || i==5 ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT)) {
            lv_obj_del(row);return NULL;
        }
    lv_obj_t *box=surface(row,16,14,20,20,4,0xFFFFFF);
    if (!box) {lv_obj_del(row);return NULL;}
    lv_obj_set_style_border_width(box,1,0);
    lv_obj_set_style_border_color(box,lv_color_hex(HISTORY_MUTED),0);
    return row;
}
static void record_row_bind(lv_obj_t *row,uint32_t index,void *context)
{
    (void)context;
    uint32_t id=displayed_id(index);
    const ui_history_record_t *rec=record_find(id);
    bool selected=id_selected(id);
    lv_obj_set_style_bg_color(row,lv_color_hex(selected ? 0xDCE6EC : index%2 ? 0xF4F6F7 : 0xFFFFFF),0);
    lv_obj_t *box=lv_obj_get_child(row,6);
    show(box,history->selecting);
    lv_obj_set_style_bg_color(box,lv_color_hex(selected ? 0x657F90 : 0xFFFFFF),0);
    lv_obj_set_x(lv_obj_get_child(row,0),history->selecting ? 46 : 16);
    lv_obj_set_width(lv_obj_get_child(row,0),history->selecting ? 110 : 140);
    char text[96];
    snprintf(text,sizeof(text),"#%lu",(unsigned long)id);
    text_set(lv_obj_get_child(row,0),rec ? text : "");
    date_text(rec,text,sizeof(text));text_set(lv_obj_get_child(row,1),rec ? text : "");
    text_set(lv_obj_get_child(row,2),rec ? rec->currency : "");
    snprintf(text,sizeof(text),"%lu",(unsigned long)(rec ? rec->pcs : 0));
    text_set(lv_obj_get_child(row,3),rec ? text : "");
    snprintf(text,sizeof(text),"%lu",(unsigned long)(rec ? rec->amount : 0));
    text_set(lv_obj_get_child(row,4),rec ? text : "");
    text_set(lv_obj_get_child(row,5),history->selecting ? "" : tr(UI_TEXT_HISTORY_RESULT_OPEN));
}
static void record_pointer(lv_event_t *event)
{
    lv_event_code_t code=lv_event_get_code(event);
    if (code!=LV_EVENT_PRESSED && code!=LV_EVENT_PRESSING &&
        code!=LV_EVENT_RELEASED && code!=LV_EVENT_PRESS_LOST) return;
    if (!visible() || history->detail_mode || history->search || history->dialog) return;
    lv_indev_t *input=lv_event_get_indev(event);
    if (!input || code==LV_EVENT_PRESS_LOST) {history->tapping=false;return;}
    lv_point_t point;lv_indev_get_point(input,&point);
    if (code==LV_EVENT_PRESSED) {
        uint32_t index;
        history->tapping=lv_recycled_list_index_at_point(history->list,&point,&index);
        history->pressed_id=history->tapping ? displayed_id(index) : 0;
        history->pressed_revision=history->revision;history->press=point;
    } else {
        if (LV_ABS(point.x-history->press.x)>=6 || LV_ABS(point.y-history->press.y)>=6)
            history->tapping=false;
        if (code==LV_EVENT_RELEASED) {
            uint32_t index=UINT32_MAX;
            bool tap=history->tapping && history->pressed_id &&
                history->pressed_revision==history->revision &&
                lv_recycled_list_index_at_point(history->list,&point,&index) &&
                displayed_id(index)==history->pressed_id;
            history->tapping=false;
            if (!tap) return;
            if (!history->selecting) {show_record(history->pressed_id);return;}
            bool selected=id_selected(history->pressed_id);
            if (selected) {
                for (size_t i=0;i<history->selected_count;++i)
                    if (history->selected_ids[i]==history->pressed_id) {
                        memmove(&history->selected_ids[i],&history->selected_ids[i+1],
                            (history->selected_count-i-1)*sizeof(uint32_t));
                        --history->selected_count;break;
                    }
            } else if (history->selected_count<UI_HISTORY_MAX_RECORDS)
                history->selected_ids[history->selected_count++]=history->pressed_id;
            lv_recycled_list_refresh(history->list,(uint32_t)displayed_count(),false);
            refresh_header();
        }
    }
}
static history_record_detail_t *current_detail(void)
{
    int index=record_index(history->current_id);
    return index>=0 ? history->details[index] : NULL;
}
static size_t section_count(unsigned id)
{
    history_record_detail_t *detail=current_detail();
    if (!detail) return 0;
    return id==0 ? detail->denom_count : id==1 ? detail->serial_count : detail->reject_count;
}
static lv_obj_t *detail_row_create(lv_obj_t *parent,lv_coord_t width,void *context)
{
    history_section_t *section=context;
    lv_obj_t *row=surface(parent,0,0,width,DETAIL_ROW_HEIGHT,5,0xFFFFFF);
    if (!row) return NULL;
    const int xs[][3]={{10,110,196},{10,60,308},{10,62,112}};
    const int ws[][3]={{90,64,124},{44,238,80},{44,42,174}};
    for (unsigned i=0;i<3;++i) {
        lv_text_align_t align=section->id==0 || (section->id==1 && i==2) || (section->id==2 && i==1)
            ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT;
        if (!label(row,xs[section->id][i],5,ws[section->id][i],24,
            i==0 && section->id ? &lv_font_instrument_sans_medium_14 : &lv_font_instrument_sans_medium_16,
            i==0 && section->id ? HISTORY_MUTED : section->id==2 && i==1 ? 0xA67834 : HISTORY_BODY,align)) {
            lv_obj_del(row);return NULL;
        }
    }
    return row;
}
static void detail_row_bind(lv_obj_t *row,uint32_t index,void *context)
{
    history_section_t *section=context;history_record_detail_t *detail=current_detail();
    lv_obj_set_style_bg_color(row,lv_color_hex(index%2 ? 0xF4F6F7 : 0xFFFFFF),0);
    char cells[3][80]={{0}};
    if (detail && index<section_count(section->id)) {
        if (section->id==0) {
            history_detail_denom_t *d=&detail->denoms[index];
            snprintf(cells[0],sizeof(cells[0]),"%lu",(unsigned long)d->value);
            snprintf(cells[1],sizeof(cells[1]),"%lu",(unsigned long)d->pcs);
            snprintf(cells[2],sizeof(cells[2]),"%llu",(unsigned long long)d->amount);
        } else if (section->id==1) {
            history_export_sn_entry_t *s=&detail->serials[index];
            snprintf(cells[0],sizeof(cells[0]),"%u",s->no);
            snprintf(cells[1],sizeof(cells[1]),"%s",s->sn);
            if (s->denom) snprintf(cells[2],sizeof(cells[2]),"%u",s->denom);
            else snprintf(cells[2],sizeof(cells[2]),"--");
        } else {
            history_detail_reject_t *r=&detail->rejects[index];
            snprintf(cells[0],sizeof(cells[0]),"%lu",(unsigned long)r->no);
            snprintf(cells[1],sizeof(cells[1]),"%lu",(unsigned long)r->pcs);
            snprintf(cells[2],sizeof(cells[2]),"%s",ui_text_counting_reject_reason(r->code));
        }
    }
    for (unsigned i=0;i<3;++i) text_set(lv_obj_get_child(row,i),cells[i]);
}
static void detail_changed(const ui_list_window_t *window,void *context)
{
    history_section_t *section=context;
    if (!section->footer) return;
    char text[96];snprintf(text,sizeof(text),tr(UI_TEXT_HISTORY_DETAIL_COUNT_FMT),(unsigned)window->count);
    text_set(section->footer,text);
}
static bool detail_create(void)
{
    if (history->sections[0].panel) return true;
    const int xs[]={16,388,830},ws[]={360,430,326};
    const uint32_t colors[]={0x2BD900,0x0074F8,0xF85820};
    const ui_text_id_t titles[]={UI_TEXT_LIST_DENOMINATIONS,UI_TEXT_LIST_SERIAL_NUMBERS,UI_TEXT_LIST_REJECT_ANALYSIS};
    const ui_text_id_t columns[][3]={{UI_TEXT_PAGE01_DETAIL_COL_DENOM,UI_TEXT_PAGE01_DETAIL_COL_PCS,UI_TEXT_PAGE01_DETAIL_COL_AMOUNT},
        {UI_TEXT_PAGE01_DETAIL_COL_NO,UI_TEXT_LIST_COL_SERIAL_NUMBER,UI_TEXT_PAGE01_DETAIL_COL_DENOM},
        {UI_TEXT_PAGE01_DETAIL_COL_NO,UI_TEXT_PAGE01_DETAIL_COL_PCS,UI_TEXT_LIST_COL_REASON}};
    const int cx[][3]={{22,122,208},{22,72,320},{22,74,124}};
    const int cw[][3]={{90,64,124},{44,238,80},{44,42,174}};
    for (unsigned i=0;i<3;++i) {
        history_section_t *s=&history->sections[i];s->id=i;
        s->panel=surface(history->root,xs[i],90,ws[i],298,15,0xFFFFFF);
        if (!s->panel) goto failed;
        lv_obj_t *badge=surface(s->panel,18,12,24,24,7,colors[i]);
        if (!badge) goto failed;
        lv_obj_t *letter=label(badge,0,3,24,22,&lv_font_instrument_sans_bold_14,0xFFFFFF,LV_TEXT_ALIGN_CENTER);
        lv_obj_t *title=label(s->panel,52,12,ws[i]-70,26,&lv_font_instrument_sans_semibold_20,HISTORY_INK,LV_TEXT_ALIGN_LEFT);
        if (!letter || !title) goto failed;
        text_set(letter,tr((ui_text_id_t)(UI_TEXT_PAGE01_DETAIL_BTN_A+i)));text_set(title,tr(titles[i]));
        if (!surface(s->panel,12,44,ws[i]-36,1,0,HISTORY_LINE) ||
            !surface(s->panel,12,71,ws[i]-36,1,0,HISTORY_LINE) ||
            !surface(s->panel,0,264,ws[i],1,0,HISTORY_LINE)) goto failed;
        for (unsigned j=0;j<3;++j) {
            lv_obj_t *head=label(s->panel,cx[i][j],51,cw[i][j],18,
                &lv_font_instrument_sans_medium_12,HISTORY_MUTED,
                i==0 || (i==1 && j==2) || (i==2 && j==1) ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT);
            if (!head) goto failed;
            text_set(head,tr(columns[i][j]));
        }
        s->footer=label(s->panel,20,274,ws[i]-40,22,&lv_font_instrument_sans_medium_14,HISTORY_MUTED,LV_TEXT_ALIGN_LEFT);
        if (!s->footer) goto failed;
        const lv_recycled_list_config_t config={12,72,ws[i]-12,DETAIL_ROWS,DETAIL_ROW_HEIGHT,
            detail_row_create,detail_row_bind,detail_changed,s};
        s->list=lv_recycled_list_create(s->panel,&config);
        if (!s->list) goto failed;
        lv_port_indev_set_drag_obj(lv_recycled_list_object(s->list),true);
        s->empty=label(s->panel,22,149,ws[i]-44,56,&lv_font_instrument_sans_medium_16,HISTORY_MUTED,LV_TEXT_ALIGN_CENTER);
        if (!s->empty) goto failed;
        lv_label_set_long_mode(s->empty,LV_LABEL_LONG_WRAP);
        text_set(s->empty,tr(i==1 ? UI_TEXT_HISTORY_NO_SERIAL : i==2 ? UI_TEXT_HISTORY_REJECT_EMPTY : UI_TEXT_HISTORY_PARTIAL));
    }
    return true;
failed:
    for (unsigned i=0;i<3;++i) {
        if (history->sections[i].panel) lv_obj_del(history->sections[i].panel);
        memset(&history->sections[i],0,sizeof(history->sections[i]));
    }
    return false;
}
static void show_record(uint32_t id)
{
    int index=record_index(id);
    if (index<0 || !record_find(id)) {toast(tr(UI_TEXT_HISTORY_MISSING));return;}
    if (!prepare_detail((unsigned)index) || !detail_create()) {toast(tr(UI_TEXT_SERIAL_UNAVAILABLE));return;}
    history->tapping=false;lv_recycled_list_stop(history->list);
    history->current_id=id;history->detail_mode=true;
    show(history->list_panel,false);
    for (unsigned i=0;i<3;++i) {
        show(history->sections[i].panel,true);
        lv_recycled_list_refresh(history->sections[i].list,(uint32_t)section_count(i),true);
        show(history->sections[i].empty,section_count(i)==0);
    }
    refresh_header();
}
static void show_list(void)
{
    history->detail_mode=false;history->current_id=0;
    for (unsigned i=0;i<3;++i) {
        lv_recycled_list_stop(history->sections[i].list);show(history->sections[i].panel,false);
    }
    show(history->list_panel,true);refresh_records(false);
}
static void refresh_header(void)
{
    if (!history) return;
    char text[200];
    bool available=ui_history_data_is_available();
    text_set(history->lifetime_title,tr(UI_TEXT_HISTORY_LIFETIME));
    snprintf(text,sizeof(text),"%lu",(unsigned long)ui_history_total_notes_counted_get());text_set(history->lifetime,text);
    lv_damped_button_set_text(history->reset_total,tr(UI_TEXT_HISTORY_CLEAR_TOTAL));
    bool review_available=!history->detail_mode && history->unknown_count && !history->selecting;
    show(history->reset_total,!history->detail_mode && !history->selecting && !review_available);
    show(history->lifetime,!history->detail_mode && !review_available);
    show(history->lifetime_title,!history->detail_mode && !review_available);
    show(history->unknown,review_available);
    if (history->reviewing_unknown) snprintf(text,sizeof(text),"%s",tr(UI_TEXT_HISTORY_BACK));
    else snprintf(text,sizeof(text),"%s: %s",tr(UI_TEXT_HISTORY_RESULT_OPEN),tr(UI_TEXT_HISTORY_PARTIAL));
    lv_damped_button_set_text(history->unknown,text);
    storage_job_status_t status=ui_history_data_status();
    if (history->detail_mode) {
        lv_obj_set_width(history->title,1096);
        lv_obj_set_width(history->subtitle,440);
        lv_obj_set_pos(history->summary,478,40);
        lv_obj_set_width(history->summary,626);
        lv_obj_set_style_text_font(history->summary,&lv_font_instrument_sans_medium_12,0);
        const ui_history_record_t *rec=record_find(history->current_id);
        char date[64];date_text(rec,date,sizeof(date));
        if (rec) {
            snprintf(text,sizeof(text),tr(UI_TEXT_HISTORY_RECORD_FMT),(unsigned long)rec->record_no,rec->currency,date);
            text_set(history->title,text);
            snprintf(text,sizeof(text),tr(UI_TEXT_HISTORY_TOTAL_FMT),(unsigned long)rec->pcs,rec->currency,(unsigned long)rec->amount);
            text_set(history->subtitle,text);
        } else {
            text_set(history->title,tr(UI_TEXT_HISTORY_MISSING));text_set(history->subtitle,"");
        }
        text_set(history->summary,tr(UI_TEXT_HISTORY_LIMITED));text_set(history->notice,"");
        const ui_text_id_t ids[]={UI_TEXT_HISTORY_PREVIOUS,UI_TEXT_HISTORY_NEXT,UI_TEXT_HISTORY_EXPORT,UI_TEXT_HISTORY_BACK};
        for (unsigned i=0;i<4;++i) lv_damped_button_set_text(history->actions[i],tr(ids[i]));
        int position=-1;
        for (size_t i=0;i<displayed_count();++i) if (displayed_id(i)==history->current_id) position=(int)i;
        lv_damped_button_set_enabled(history->actions[0],position>0);
        lv_damped_button_set_enabled(history->actions[1],position>=0 && (size_t)position+1<displayed_count());
        lv_damped_button_set_enabled(history->actions[2],rec!=NULL);
    } else {
        lv_obj_set_width(history->title,312);
        lv_obj_set_width(history->subtitle,312);
        lv_obj_set_pos(history->summary,344,12);
        lv_obj_set_width(history->summary,470);
        lv_obj_set_style_text_font(history->summary,&lv_font_instrument_sans_medium_18,0);
        text_set(history->title,tr(UI_TEXT_HISTORY_RECORDS));
        if (history->selecting) {
            snprintf(text,sizeof(text),tr(UI_TEXT_HISTORY_SELECTED_FMT),(unsigned)history->selected_count);
            text_set(history->subtitle,text);text_set(history->notice,tr(UI_TEXT_HISTORY_SELECTION_HINT));
        } else {
            snprintf(text,sizeof(text),tr(UI_TEXT_HISTORY_STORAGE_FMT),(unsigned)history->record_count,UI_HISTORY_MAX_RECORDS);
            text_set(history->subtitle,text);
            if (!available) text_set(history->notice,tr(UI_TEXT_HISTORY_STORAGE_FAILED));
            else if (status==STORAGE_JOB_FAILED) text_set(history->notice,tr(UI_TEXT_HISTORY_SAVE_FAILED));
            else if (status==STORAGE_JOB_PENDING)
                text_set(history->notice,tr(UI_TEXT_HISTORY_SAVING));
            else if (history->unknown_count) {
                snprintf(text,sizeof(text),tr(UI_TEXT_HISTORY_UNKNOWN_FMT),(unsigned)history->unknown_count);
                text_set(history->notice,text);
            } else text_set(history->notice,tr(UI_TEXT_HISTORY_LIMITED));
        }
        if (history->reviewing_unknown) text_set(history->summary,tr(UI_TEXT_HISTORY_PARTIAL));
        else {
            snprintf(text,sizeof(text),tr(UI_TEXT_HISTORY_SUMMARY_FMT),(unsigned)history->result.matched_count,
                (unsigned long long)history->result.matched_pcs);
            text_set(history->summary,text);
        }
        const ui_text_id_t ids[]={history->selecting ? UI_TEXT_HISTORY_ALL_VISIBLE : UI_TEXT_SERIAL_SEARCH,
            UI_TEXT_HISTORY_EXPORT,history->selecting ? UI_TEXT_HISTORY_DELETE : UI_TEXT_HISTORY_SELECT,
            history->selecting ? UI_TEXT_HISTORY_CANCEL : UI_TEXT_HISTORY_BACK};
        for (unsigned i=0;i<4;++i) lv_damped_button_set_text(history->actions[i],tr(ids[i]));
        lv_damped_button_set_enabled(history->actions[0],available && (!history->selecting || displayed_count()>0));
        lv_damped_button_set_enabled(history->actions[1],available &&
            (history->selecting ? history->selected_count>0 : displayed_count()>0));
        lv_damped_button_set_enabled(history->actions[2],available &&
            (history->selecting ? history->selected_count>0 : displayed_count()>0));
    }
    lv_damped_button_set_enabled(history->reset_total,available && ui_history_data_can_accept());
    lv_damped_button_set_text(history->sort,tr(history->input.oldest_first ? UI_TEXT_HISTORY_SORT_OLDEST : UI_TEXT_HISTORY_SORT_NEWEST));
}
static void refresh_records(bool reset)
{
    if (!history || history->search || history->dialog) return;
    ++history->revision;history->tapping=false;
    const ui_history_store_t *store=ui_history_data_get();
    if (history->model_dirty) {
        release_details();history->record_count=0;
        if (store && ui_history_data_is_available()) {
            for (unsigned i=0;i<store->record_count && i<UI_HISTORY_MAX_RECORDS;++i) {
                const ui_history_record_t *r=&store->records[i];if (!r->valid) continue;
                history_query_record_t *p=&history->records[history->record_count++];
                memset(p,0,sizeof(*p));p->valid=true;p->record_no=r->record_no;
                p->pcs=r->pcs;p->amount=r->amount;memcpy(p->currency,r->currency,sizeof(p->currency));
                p->year=r->year;p->month=r->month;p->day=r->day;p->hour=r->hour;p->minute=r->minute;p->second=r->second;
            }
        }
        history->model_dirty=false;
        for (size_t i=0;i<history->selected_count;) {
            if (record_find(history->selected_ids[i])) {++i;continue;}
            memmove(&history->selected_ids[i],&history->selected_ids[i+1],
                (--history->selected_count-i)*sizeof(uint32_t));
        }
    }
    if (history_query_compile(&history->input,&history->query)!=HISTORY_QUERY_OK) return;
    bool needs_detail=history->query.serial[0] || history->query.denomination_count ||
        history->query.rejects!=HISTORY_REJECT_ALL;
    if (needs_detail) for (size_t i=0;i<history->record_count;++i) (void)prepare_detail((unsigned)i);
    history->result=history_query_build(history->records,history->record_count,&history->query,
        history->result_ids,UI_HISTORY_MAX_RECORDS);
    history->unknown_count=0;
    for (size_t i=0;i<history->record_count;++i) {
        if (history_query_match_record(&history->query,&history->records[i])!=HISTORY_QUERY_UNKNOWN) continue;
        uint32_t id=history->records[i].record_no;
        size_t position=history->unknown_count++;
        while (position && (history->query.oldest_first ? history->unknown_ids[position-1]>id :
               history->unknown_ids[position-1]<id)) {
            history->unknown_ids[position]=history->unknown_ids[position-1];--position;
        }
        history->unknown_ids[position]=id;
    }
    if (!history->unknown_count) history->reviewing_unknown=false;
    for (size_t i=0;i<history->selected_count;) {
        bool present=false;
        for (size_t j=0;j<displayed_count();++j)
            if (displayed_id((uint32_t)j)==history->selected_ids[i]) {present=true;break;}
        if (present) {++i;continue;}
        memmove(&history->selected_ids[i],&history->selected_ids[i+1],
            (--history->selected_count-i)*sizeof(uint32_t));
    }
    lv_recycled_list_refresh(history->list,(uint32_t)displayed_count(),reset);
    show(history->empty,displayed_count()==0);
    text_set(history->empty,tr(!ui_history_data_is_available() ? UI_TEXT_HISTORY_STORAGE_FAILED :
        history->record_count ? UI_TEXT_HISTORY_NO_MATCH : UI_TEXT_HISTORY_EMPTY));
    if (history->detail_mode) {
        uint32_t id=history->current_id;int i=record_index(id);
        if (i>=0 && prepare_detail((unsigned)i)) {
            for (unsigned s=0;s<3;++s) {
                lv_recycled_list_refresh(history->sections[s].list,(uint32_t)section_count(s),false);
                show(history->sections[s].empty,section_count(s)==0);
            }
        } else for (unsigned s=0;s<3;++s) {
            lv_recycled_list_refresh(history->sections[s].list,0,true);
            show(history->sections[s].empty,true);
        }
    }
    refresh_header();
}
static void search_closed(const history_query_input_t *applied,void *context)
{
    (void)context;
    if (!history) return;
    bool apply=applied!=NULL;
    if (apply) history->input=*applied;
    page_19_history_search_t *search=history->search;history->search=NULL;
    page_19_history_search_destroy(search);
    if (apply) {
        history->selected_count=0;history->selecting=false;history->reviewing_unknown=false;
        history->detail_mode=false;history->current_id=0;
        for (unsigned i=0;i<3;++i) show(history->sections[i].panel,false);
        show(history->list_panel,true);
    }
    if (visible()) refresh_records(apply);
}
static void open_search(void)
{
    if (history->search || !ui_history_data_is_available()) return;
    lv_recycled_list_stop(history->list);history->tapping=false;
    for (size_t i=0;i<history->record_count;++i) (void)prepare_detail((unsigned)i);
    history->search=page_19_history_search_create(history->root,&history->input,history->records,
        history->record_count,search_closed,NULL);
    if (!history->search) toast(tr(UI_TEXT_SERIAL_UNAVAILABLE));
}
static void close_dialog(void)
{
    if (!history || !history->dialog) return;
    lv_obj_t *dialog=history->dialog;history->dialog=NULL;lv_obj_del(dialog);
}
static void confirmation_cancel(lv_event_t *event)
{
    (void)event;close_dialog();if (visible()) refresh_records(false);
}
static void confirmation_apply(lv_event_t *event)
{
    (void)event;
    if (!history || !history->dialog) return;
    bool accepted=false;
    if (ui_history_data_can_accept()) {
        if (history->confirm_clear) {
            storage_job_id_t previous=ui_history_last_commit_id();
            uint32_t total=ui_history_total_notes_counted_get();
            ui_history_total_notes_counted_clear();
            accepted=!total || (ui_history_last_commit_id()!=previous &&
                ui_history_data_status()!=STORAGE_JOB_FAILED);
        } else {
            accepted=ui_history_record_delete_records(history->confirmed_ids,history->confirmed_count);
        }
    }
    close_dialog();history->model_dirty=true;
    if (accepted) {history->selected_count=0;history->selecting=false;}
    else toast(tr(UI_TEXT_HISTORY_SAVE_FAILED));
    refresh_records(false);
}
static void show_confirmation(bool clear_total)
{
    if (!history || history->dialog) return;
    if (!clear_total && !history->selected_count) return;
    history->confirmed_count=history->selected_count;
    memcpy(history->confirmed_ids,history->selected_ids,history->confirmed_count*sizeof(uint32_t));
    history->confirm_clear=clear_total;
    lv_obj_t *overlay=surface(history->root,0,0,1280,400,0,0x17212A);
    if (!overlay) return;
    history->dialog=overlay;lv_obj_set_style_bg_opa(overlay,LV_OPA_40,0);
    lv_obj_add_flag(overlay,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *card=surface(overlay,365,90,550,220,18,0xFFFFFF);
    if (!card) goto failed;
    lv_obj_t *title=label(card,26,20,498,32,&lv_font_instrument_sans_semibold_22,HISTORY_INK,LV_TEXT_ALIGN_LEFT);
    lv_obj_t *body=label(card,26,64,498,74,&lv_font_instrument_sans_medium_16,HISTORY_BODY,LV_TEXT_ALIGN_LEFT);
    if (!title || !body) goto failed;
    text_set(title,tr(clear_total ? UI_TEXT_HISTORY_CLEAR_TOTAL : UI_TEXT_HISTORY_DELETE_TITLE));
    lv_label_set_long_mode(body,LV_LABEL_LONG_WRAP);
    char text[200];snprintf(text,sizeof(text),tr(clear_total ? UI_TEXT_HISTORY_CLEAR_WARNING :
        UI_TEXT_HISTORY_DELETE_WARNING),(unsigned)history->confirmed_count);text_set(body,text);
    lv_obj_t *cancel=lv_nav_button_create(card,26,156,234,44,confirmation_cancel,NULL);
    lv_obj_t *confirm=button(card,290,156,234,44,tr(UI_TEXT_HISTORY_APPLY),confirmation_apply,NULL);
    if (!cancel || !confirm) goto failed;
    lv_damped_button_set_text(cancel,tr(UI_TEXT_HISTORY_CANCEL));
    lv_damped_button_set_palette(confirm,lv_color_hex(0xFDECEC),lv_color_hex(0xEFCFD0));
    lv_damped_button_set_enabled(confirm,ui_history_data_can_accept());
    lv_recycled_list_stop(history->list);history->tapping=false;return;
failed:
    close_dialog();toast(tr(UI_TEXT_SERIAL_UNAVAILABLE));
}
static void sort_event(lv_event_t *event)
{
    (void)event;history->input.oldest_first=!history->input.oldest_first;refresh_records(true);
}
static void unknown_event(lv_event_t *event)
{
    (void)event;history->reviewing_unknown=!history->reviewing_unknown;
    history->selected_count=0;refresh_records(true);
}
static void reset_event(lv_event_t *event) {(void)event;show_confirmation(true);}
static void action_event(lv_event_t *event)
{
    if (!visible()) return;
    unsigned action=(unsigned)(uintptr_t)lv_event_get_user_data(event);
    if (action==3) {
        if (history->dialog) {close_dialog();refresh_records(false);}
        else if (history->detail_mode) show_list();
        else if (history->selecting) {history->selecting=false;history->selected_count=0;refresh_records(false);}
        else ui_manager_pop_page();
        return;
    }
    if (history->search || history->dialog) return;
    if (history->detail_mode) {
        if (action==2) {
            if (!ui_history_export_data_request_records(&history->current_id,1))
                toast(tr(UI_TEXT_SERIAL_UNAVAILABLE));
        } else {
            for (size_t i=0;i<displayed_count();++i) if (displayed_id(i)==history->current_id) {
                if (action==0 && i) show_record(displayed_id((uint32_t)i-1));
                if (action==1 && i+1<displayed_count()) show_record(displayed_id((uint32_t)i+1));
                break;
            }
        }
        return;
    }
    if (action==0) {
        if (!history->selecting) open_search();
        else {
            bool all=history->selected_count==displayed_count();
            history->selected_count=0;
            if (!all) for (size_t i=0;i<displayed_count();++i)
                history->selected_ids[history->selected_count++]=displayed_id((uint32_t)i);
            refresh_records(false);
        }
    } else if (action==1) {
        const uint32_t *ids=history->selecting ? history->selected_ids :
            history->reviewing_unknown ? history->unknown_ids : history->result_ids;
        size_t count=history->selecting ? history->selected_count : displayed_count();
        if (!count || !ui_history_export_data_request_records(ids,count))
            toast(tr(UI_TEXT_SERIAL_UNAVAILABLE));
    } else if (history->selecting) show_confirmation(false);
    else {history->selecting=true;history->selected_count=0;refresh_records(false);}
}
void ui_page_19_history_create(lv_obj_t *parent)
{
    if (history) return;
    ui_history_data_init();
    history=lv_mem_alloc(sizeof(*history));
    if (!history) return;
    memset(history,0,sizeof(*history));history->model_dirty=true;history->language=ui_lang_get();
    history->root=surface(parent ? parent : lv_scr_act(),0,0,1280,400,0,HISTORY_BG);
    if (!history->root) goto failed;
    lv_obj_t *header=surface(history->root,16,12,1140,66,15,0xFFFFFF);
    if (!header) goto failed;
    history->title=label(header,22,10,780,30,&lv_font_instrument_sans_semibold_22,HISTORY_INK,LV_TEXT_ALIGN_LEFT);
    history->subtitle=label(header,22,39,312,22,&lv_font_instrument_sans_medium_14,HISTORY_MUTED,LV_TEXT_ALIGN_LEFT);
    history->summary=label(header,344,12,470,25,&lv_font_instrument_sans_medium_18,HISTORY_BODY,LV_TEXT_ALIGN_LEFT);
    history->notice=label(header,344,39,470,22,&lv_font_instrument_sans_medium_12,HISTORY_MUTED,LV_TEXT_ALIGN_LEFT);
    history->lifetime_title=label(header,830,10,182,20,&lv_font_instrument_sans_medium_12,HISTORY_MUTED,LV_TEXT_ALIGN_RIGHT);
    history->lifetime=label(header,830,32,182,28,&lv_font_instrument_sans_semibold_22,HISTORY_BODY,LV_TEXT_ALIGN_RIGHT);
    history->reset_total=button(header,1030,14,96,38,tr(UI_TEXT_HISTORY_CLEAR_TOTAL),reset_event,NULL);
    if (!history->title || !history->subtitle || !history->summary || !history->notice ||
        !history->lifetime_title || !history->lifetime || !history->reset_total) goto failed;
    lv_obj_set_style_text_font(lv_damped_button_get_label(history->reset_total),&lv_font_instrument_sans_semibold_12,0);
    for (unsigned i=0;i<4;++i) {
        history->actions[i]= i==3 ? lv_nav_button_create(history->root,1168,12+96*i,96,88,action_event,(void *)(uintptr_t)i) :
            button(history->root,1168,12+96*i,96,88,"",action_event,(void *)(uintptr_t)i);
        if (!history->actions[i]) goto failed;
        lv_damped_button_set_palette(history->actions[i],lv_color_hex(0xFFFFFF),lv_color_hex(0xEBEBEB));
        lv_obj_set_style_radius(history->actions[i],12,0);
        lv_obj_set_style_text_font(lv_damped_button_get_label(history->actions[i]),&lv_font_instrument_sans_semibold_12,0);
    }
    history->list_panel=surface(history->root,16,90,1140,298,15,0xFFFFFF);
    if (!history->list_panel) goto failed;
    const ui_text_id_t heads[]={UI_TEXT_HISTORY_RECORDS,UI_TEXT_HISTORY_DATE,UI_TEXT_HISTORY_CURRENCY,
        UI_TEXT_HISTORY_PCS,UI_TEXT_HISTORY_AMOUNT};
    const int xs[]={28,176,444,528,682},ws[]={132,250,76,116,194};
    for (unsigned i=0;i<5;++i) {
        lv_obj_t *head=label(history->list_panel,xs[i],11,ws[i],20,
            &lv_font_instrument_sans_medium_12,HISTORY_MUTED,i>=3 ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT);
        if (!head) goto failed;
        text_set(head,tr(heads[i]));
    }
    history->sort=button(history->list_panel,968,3,156,30,"",sort_event,NULL);
    if (!history->sort || !surface(history->list_panel,12,35,1108,1,0,HISTORY_LINE)) goto failed;
    history->range=label(history->list_panel,22,278,300,20,&lv_font_instrument_sans_medium_12,HISTORY_MUTED,LV_TEXT_ALIGN_LEFT);
    if (!history->range) goto failed;
    const lv_recycled_list_config_t config={12,36,1128,HISTORY_ROWS,HISTORY_ROW_HEIGHT,
        record_row_create,record_row_bind,list_changed,NULL};
    history->list=lv_recycled_list_create(history->list_panel,&config);
    if (!history->list) goto failed;
    lv_obj_t *viewport=lv_recycled_list_object(history->list);
    lv_port_indev_set_drag_obj(viewport,true);
    if (!lv_obj_add_event_cb(viewport,record_pointer,LV_EVENT_ALL,NULL)) goto failed;
    history->empty=label(history->list_panel,60,133,1016,70,&lv_font_instrument_sans_medium_18,HISTORY_MUTED,LV_TEXT_ALIGN_CENTER);
    history->unknown=button(header,830,11,296,44,tr(UI_TEXT_HISTORY_PARTIAL),unknown_event,NULL);
    if (!history->empty || !history->unknown) goto failed;
    lv_damped_button_set_palette(history->unknown,lv_color_hex(0xF4F6F7),lv_color_hex(0xDCE6EC));
    lv_label_set_long_mode(history->empty,LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lv_damped_button_get_label(history->unknown),&lv_font_instrument_sans_medium_12,0);
    refresh_records(true);
    return;
failed:
    ui_page_19_history_destroy();
}
void ui_page_19_history_refresh(void)
{
    if (!history) return;
    history->model_dirty=true;++history->revision;history->tapping=false;
    if (visible()) refresh_records(false);
}
bool ui_page_19_history_resume(void)
{
    if (!history || !history->root) return false;
    if (history->language!=ui_lang_get()) {
        lv_obj_t *parent=lv_obj_get_parent(history->root);
        ui_page_19_history_destroy();
        ui_page_19_history_create(parent);
        if (!history || !history->root) return false;
    }
    /* A new visit starts at the history list; internal detail/back keeps filters. */
    history->detail_mode=false;history->current_id=0;history->selecting=false;
    history->selected_count=0;history->reviewing_unknown=false;
    memset(&history->input,0,sizeof(history->input));history->model_dirty=true;
    show(history->root,true);show(history->list_panel,true);
    for (unsigned i=0;i<3;++i) show(history->sections[i].panel,false);
    lv_obj_move_foreground(history->root);refresh_records(true);return true;
}
void ui_page_19_history_suspend(void)
{
    if (!history) return;
    if (history->search) {
        page_19_history_search_t *search=history->search;history->search=NULL;
        page_19_history_search_destroy(search);
    }
    close_dialog();lv_recycled_list_stop(history->list);history->tapping=false;
    for (unsigned i=0;i<3;++i) lv_recycled_list_stop(history->sections[i].list);
    show(history->root,false);
}
void ui_page_19_history_destroy(void)
{
    if (!history) return;
    ui_page_19_history_suspend();release_details();
    if (history->root) lv_obj_del(history->root);
    lv_mem_free(history);history=NULL;
}
