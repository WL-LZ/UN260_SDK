#include "ui_multi_detail.h"
#include "lv_recycled_list.h"
#include "lv_alnum_keyboard.h"
#include "lv_damped_button.h"
#include "ui_detail_reveal.h"
#include "un260/counting/counting_multi_extra.h"
#include "un260/counting/counting_serial_query.h"
#include "un260/currency/currency_metadata.h"
#include "un260/lv_resources/lv_img_init.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/ui_update_batch.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/font/main_fonts.h"
#include "un260/font/scaled_font.h"
#include "lv_port_indev.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

struct ui_multi_detail {
    lv_obj_t *root,*content,*currency,*flag,*title,*subtitle,*pcs,*amount,*unit,*currency_target;
    lv_obj_t *headers[3],*tabs[2],*back,*refresh,*reject,*reject_value,*search,*foot,*empty;
    lv_obj_t *segments,*currency_name,*pcs_title,*amount_symbol,*search_text,*count_label;
    lv_obj_t *head_line,*foot_line,*currency_line;
    scaled_font_t symbol_font;
    uint8_t symbol_pixels[1024];
    lv_recycled_list_t *list;
    lv_alnum_keyboard_t *keyboard;
    ui_detail_reveal_t reveal;
    lv_timer_t *timer;
    ui_multi_open_fn open;void *context;
    int selected,tab,width,height;
    uint32_t generation,revision,extra;
    unsigned indices[MULTI_SERIAL_MAX],count;
    char query[33];
    uint32_t denomination_query[LV_ALNUM_KEYBOARD_MAX_CHOICES];
    uint8_t denomination_count;
    int search_currency;
    bool denomination_mode,keyboard_initialized;
    bool expanded,visible,pending,dirty;
};
static lv_obj_t *box(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *o=lv_obj_create(p);if(!o)return NULL;lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,255,0);lv_obj_set_style_radius(o,radius,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);return o;
}
static lv_obj_t *label(lv_obj_t *p,int x,int y,int w,const char *s,const lv_font_t *font,uint32_t color)
{
    if(!p)return NULL;
    lv_obj_t *o=lv_label_create(p);if(!o)return NULL;lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text(o,s);return o;
}
static void hide(lv_obj_t *o,bool yes){if(!o)return;if(yes)lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);}
static void text(lv_obj_t *o,const char *s){if(o&&strcmp(lv_label_get_text(o),s))lv_label_set_text(o,s);}
static void number(lv_obj_t *o,uint64_t n){char b[32];snprintf(b,sizeof(b),"%llu",(unsigned long long)n);text(o,b);}
static lv_obj_t *icon(lv_obj_t *parent,int x,int y,const char *source)
{
    lv_obj_t *o=lv_img_create(parent);
    if(o){lv_img_set_src(o,source);lv_obj_set_pos(o,x,y);lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE);}
    return o;
}
static const char *currency_name(const char *code)
{
    if(!strcmp(code,"CNY"))return ui_text_get(UI_TEXT_MULTI_CNY_NAME);
    if(!strcmp(code,"USD"))return ui_text_get(UI_TEXT_MULTI_USD_NAME);
    if(!strcmp(code,"EUR"))return ui_text_get(UI_TEXT_MULTI_EUR_NAME);
    return ui_text_get(UI_TEXT_MULTI_CURRENCY);
}
static void center_currency_glyphs(ui_multi_detail_t *v)
{
    const lv_font_t *font=lv_obj_get_style_text_font(v->currency,0);
    const char *code=lv_label_get_text(v->currency);
    int top=font->line_height,bottom=0;
    for(const char *p=code;*p;p++){
        lv_font_glyph_dsc_t glyph;
        if(!lv_font_get_glyph_dsc(font,&glyph,(uint8_t)*p,(uint8_t)p[1])||!glyph.box_h)continue;
        int y=font->line_height-font->base_line-glyph.box_h-glyph.ofs_y;
        if(y<top)top=y;
        if(y+glyph.box_h>bottom)bottom=y+glyph.box_h;
    }
    lv_obj_set_y(v->currency,(v->expanded?30:40)-(top+bottom)/2);
}
static void columns(ui_multi_detail_t *v,int *middle,int *last)
{
    if(v->selected==-1){*middle=v->expanded?432:380;*last=v->expanded?652:620;}
    else if(v->selected==-2){*middle=162;*last=v->width-216;}
    else{*middle=v->tab?(v->expanded?132:122):(v->expanded?472:460);*last=v->expanded?718:680;}
}
static lv_obj_t *button(ui_multi_detail_t *v,int x,int y,int w,int h,const char *s,uint32_t color,lv_event_cb_t cb)
{
    lv_obj_t *o=box(v->root,x,y,w,h,color,10);if(!o)return NULL;lv_obj_add_flag(o,LV_OBJ_FLAG_CLICKABLE);
    lv_damped_button_register(o,lv_color_hex(color),lv_color_hex(0xDFE8EE));
    lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,v);
    lv_obj_t *l=label(o,0,0,w,s,&lv_font_instrument_sans_medium_14,0x496475);if(l){lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(l);}return o;
}
static void flag(lv_obj_t *o,const char *code)
{
    const char *src=currency_metadata_symbol(code)?get_currency_img(code):get_currency_img("MULTI");
    lv_img_set_src(o,src);lv_img_set_zoom(o,63);lv_img_set_pivot(o,0,0);
}
static multi_detail_status_t current_status(ui_multi_detail_t *v)
{
    const counting_multi_t *m=counting_multi_current();
    if(v->selected==-2)return counting_multi_rejects()->status;
    if(v->selected<0||v->selected>=m->count)return MULTI_DETAIL_READY;
    return v->tab?counting_multi_serials(v->selected)->status:m->currencies[v->selected].status;
}
static bool available(multi_detail_status_t s){return s==MULTI_DETAIL_READY||s==MULTI_DETAIL_EMPTY;}
static void search_reset_state(ui_multi_detail_t *v)
{
    v->query[0]='\0';v->denomination_count=0;v->denomination_mode=false;
    v->keyboard_initialized=false;
}
static bool matched(ui_multi_detail_t *v,const multi_serial_t *row)
{
    if(v->denomination_mode) {
        unsigned count=v->denomination_count;
        if(!count)return true;
        for(unsigned i=0;i<count;++i)if(row->value==v->denomination_query[i])return true;
        return false;
    }
    char upper[21];snprintf(upper,sizeof(upper),"%s",row->text);
    for(char *p=upper;*p;p++)*p=(char)toupper((unsigned char)*p);
    return strstr(upper,v->query)!=NULL;
}
static void search_submit(const char *s,void *ctx)
{
    ui_multi_detail_t *v=ctx;
    const counting_multi_t *m=counting_multi_current();
    if(!v->visible || v->selected<0 || v->selected>=m->count || v->generation!=m->generation || m->counting)return;
    v->denomination_mode=lv_alnum_keyboard_is_choice_mode(v->keyboard);
    v->denomination_count=lv_alnum_keyboard_get_choices(v->keyboard,
        v->denomination_query,LV_ALNUM_KEYBOARD_MAX_CHOICES);
    char *query=v->query;
    snprintf(query,sizeof(v->query),"%s",s);
    for(char *p=query;*p;p++)*p=(char)toupper((unsigned char)*p);
    v->dirty=true;lv_recycled_list_refresh(v->list,0,true);ui_multi_detail_refresh(v);
}
/* Available choices come only from this currency's directory / serial cache.
 * Descending unique values; no denomination catalogue or currency inference. */
static void search_choices(ui_multi_detail_t *v,bool preserve_draft)
{
    uint32_t values[LV_ALNUM_KEYBOARD_MAX_CHOICES],selected[LV_ALNUM_KEYBOARD_MAX_CHOICES];
    const multi_currency_t *c=&counting_multi_current()->currencies[v->selected];
    const multi_serial_cache_t *serials=counting_multi_serials(v->selected);
    unsigned count=0;uint32_t previous=0;
    while(count<LV_ALNUM_KEYBOARD_MAX_CHOICES) {
        uint32_t next=0;
        for(unsigned i=0;i<c->denom_count && i<COUNTING_MULTI_DENOMS;++i) {
            uint32_t value=c->denom[i].value;
            if(value>next && (!count || value<previous))next=value;
        }
        if(serials)for(unsigned i=0;i<serials->count && i<MULTI_SERIAL_MAX;++i) {
            uint32_t value=serials->rows[i].value;
            if(value>next && (!count || value<previous))next=value;
        }
        if(!next)break;
        values[count++]=previous=next;
    }
    unsigned n=preserve_draft?lv_alnum_keyboard_get_choices(v->keyboard,selected,LV_ALNUM_KEYBOARD_MAX_CHOICES):v->denomination_count;
    lv_alnum_keyboard_set_choices(v->keyboard,values,count,preserve_draft?selected:v->denomination_query,n);
}
void ui_multi_detail_search(ui_multi_detail_t *v)
{
    if(!v||!v->visible)return;
    if(v->selected<0){text(v->foot,ui_text_get(UI_TEXT_MULTI_SELECT_SEARCH));return;}
    if(v->tab!=1)ui_multi_detail_select(v,v->selected,1);
    if(!v->keyboard){
        lv_alnum_keyboard_config_t cfg={0};cfg.title=ui_text_get(UI_TEXT_QUERY_SERIAL);cfg.placeholder=ui_text_get(UI_TEXT_MULTI_SEARCH_HINT);
        cfg.choice_title=ui_text_get(UI_TEXT_QUERY_DENOMINATION);cfg.choice_hint=ui_text_get(UI_TEXT_QUERY_DENOM_HINT);
        cfg.choice_empty=ui_text_get(UI_TEXT_MULTI_NO_RECORDS);
        cfg.apply_text=ui_text_get(UI_TEXT_MULTI_SEARCH_APPLY);cfg.clear_text=ui_text_get(UI_TEXT_MULTI_SEARCH_CLEAR);cfg.cancel_text=ui_text_get(UI_TEXT_MULTI_BACK);
        cfg.max_length=32;cfg.submit=search_submit;cfg.context=v;cfg.modern=true;
        v->keyboard=lv_alnum_keyboard_create(lv_obj_get_parent(v->root),&cfg);
    }
    if(v->keyboard){
        if(!v->keyboard_initialized) {
            lv_alnum_keyboard_set_text(v->keyboard,v->query);
            search_choices(v,false);
            lv_alnum_keyboard_set_choice_mode(v->keyboard,v->denomination_mode);
            v->keyboard_initialized=true;
        } else search_choices(v,true);
        lv_alnum_keyboard_show(v->keyboard);
    }
}
static void search_cb(lv_event_t *e){ui_multi_detail_search(lv_event_get_user_data(e));}
static void tab_cb(lv_event_t *e){ui_multi_detail_t *v=lv_event_get_user_data(e);ui_multi_detail_select(v,v->selected,lv_event_get_target(e)==v->tabs[1]);}
static void back_cb(lv_event_t *e){ui_multi_detail_back(lv_event_get_user_data(e));}
static void reject_cb(lv_event_t *e){ui_multi_detail_select(lv_event_get_user_data(e),-2,0);}
static void refresh_cb(lv_event_t *e)
{
    ui_multi_detail_t *v=lv_event_get_user_data(e);if(v->selected==-1)return;
    if(v->selected>=0)search_reset_state(v);
    /* Reuse an in-flight reply; queue once if a different query owns the wire. */
    v->pending=current_status(v)!=MULTI_DETAIL_LOADING;
    v->dirty=true;ui_detail_reveal_begin(&v->reveal,true);ui_multi_detail_refresh(v);
}
static void tap(lv_event_t *e)
{
    ui_multi_detail_t *v=lv_event_get_user_data(e);
    if(lv_event_get_code(e)!=LV_EVENT_RELEASED||!lv_recycled_list_tap_allowed(v->list))return;
    lv_indev_t *indev=lv_event_get_indev(e);if(!indev)return;
    if(v->selected>=0&&!v->expanded){if(v->open)v->open(v->selected,v->tab,v->context);return;}
    if(v->selected!=-1)return;lv_point_t point;uint32_t index;lv_indev_get_point(indev,&point);
    if(lv_recycled_list_index_at_point(v->list,&point,&index)&&index<counting_multi_current()->count)
        ui_multi_detail_select(v,index,0);
}
static lv_obj_t *create_row(lv_obj_t *parent,lv_coord_t width,void *ctx)
{
    ui_multi_detail_t *v=ctx;lv_obj_t *row=box(parent,0,0,width,42,0xFFFFFF,6);if(!row)return NULL;
    for(int i=0;i<3;i++)if(!label(row,12,8,200,"",&lv_font_instrument_sans_semibold_20,0x4C606E)){lv_obj_del(row);return NULL;}
    lv_obj_t *f=lv_img_create(row);if(!f){lv_obj_del(row);return NULL;}lv_obj_set_pos(f,12,9);
    if(!label(row,12,12,36,"",&v->symbol_font.font,0x7A8D9B)||
       !label(row,width-106,14,72,ui_text_get(UI_TEXT_MULTI_DETAILS_ACTION),&lv_font_instrument_sans_medium_12,0x657F90)||
       !icon(row,width-30,12,LVGL_DIR"ui_icons/expand_18.png")){lv_obj_del(row);return NULL;}
    return row;
}
static void bind_row(lv_obj_t *row,uint32_t index,void *ctx)
{
    ui_multi_detail_t *v=ctx;const counting_multi_t *m=counting_multi_current();
    bool overview=v->selected==-1,rejects_view=v->selected==-2;
    uint32_t bg=index%2?0xF4F6F7:0xFFFFFF;
    lv_obj_set_style_bg_color(row,lv_color_hex(bg),0);
    lv_obj_set_style_bg_color(row,lv_damped_button_pressed_color(lv_color_hex(bg)),LV_STATE_PRESSED);
    lv_obj_t *a=lv_obj_get_child(row,0),*b=lv_obj_get_child(row,1),*c=lv_obj_get_child(row,2),*f=lv_obj_get_child(row,3);
    int x1=overview?74:12,x2,x3;columns(v,&x2,&x3);
    lv_obj_t *symbol=lv_obj_get_child(row,4);
    hide(symbol,rejects_view||(!overview&&v->tab));
    hide(lv_obj_get_child(row,5),!overview);hide(lv_obj_get_child(row,6),!overview);
    if(overview||(!rejects_view&&!v->tab)){
        const char *code=overview?m->currencies[index].code:m->currencies[v->selected].code;
        const char *s=currency_metadata_symbol(code);text(symbol,s?s:"");
        int sx=overview?x3:12;lv_obj_set_x(symbol,sx);
        lv_point_t size;lv_txt_get_size(&size,s?s:"",&v->symbol_font.font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        if(overview)x3+=size.x+10;else x1+=size.x+10;
    }
    lv_obj_set_x(a,x1);lv_obj_set_width(a,x2-x1-16);lv_obj_set_x(b,x2);lv_obj_set_width(b,x3-x2-20);lv_obj_set_x(c,x3);lv_obj_set_width(c,overview?140:v->width-68-x3);
    lv_obj_set_style_text_font(a,v->tab&&!overview&&!rejects_view?&lv_font_instrument_sans_medium_14:&lv_font_instrument_sans_semibold_20,0);
    hide(f,!overview);
    if(overview&&index<m->count){const multi_currency_t *cur=&m->currencies[index];flag(f,cur->code);text(a,cur->code);number(b,cur->pcs);number(c,cur->amount);}
    else if(rejects_view){const multi_reject_cache_t *r=counting_multi_rejects();if(index>=r->count)return;char code[16];snprintf(code,sizeof(code),"0x%02X",r->rows[index].code);text(a,code);text(b,ui_text_counting_reject_reason(r->rows[index].code));number(c,r->rows[index].pcs);}
    else if(v->tab){const multi_serial_cache_t *s=counting_multi_serials(v->selected);if(!s||index>=v->count)return;const multi_serial_t *r=&s->rows[v->indices[index]];number(a,r->number);text(b,r->text);if(r->value)number(c,r->value);else text(c,"--");}
    else{const multi_currency_t *cur=&m->currencies[v->selected];if(index>=cur->denom_count)return;
        number(a,cur->denom[index].value);
        number(b,cur->denom[index].pcs);number(c,(uint64_t)cur->denom[index].value*cur->denom[index].pcs);}
}
static void poll(lv_timer_t *timer){ui_multi_detail_refresh(timer->user_data);}
static void deleted(lv_event_t *e)
{
    ui_multi_detail_t *v=lv_event_get_user_data(e);if(lv_event_get_target(e)!=v->root)return;
    if(v->timer)lv_timer_del(v->timer);
    if(v->keyboard)lv_alnum_keyboard_destroy(v->keyboard);
    /* Child deletion removes the loading indicator. */
    lv_mem_free(v);
}
ui_multi_detail_t *ui_multi_detail_create(lv_obj_t *parent,bool expanded,ui_multi_open_fn open,void *ctx)
{
    ui_multi_detail_t *v=lv_mem_alloc(sizeof(*v));if(!v)return NULL;memset(v,0,sizeof(*v));
    v->expanded=expanded;v->width=expanded?1140:1048;v->height=expanded?376:320;v->selected=-1;v->open=open;v->context=ctx;
    v->generation=counting_multi_current()->generation;v->dirty=true;v->search_currency=-1;
    if(!scaled_font_init(&v->symbol_font,&lv_font_main_currency_32,56,v->symbol_pixels,sizeof(v->symbol_pixels))){lv_mem_free(v);return NULL;}
    v->root=box(parent,expanded?16:108,12,v->width,v->height,0xFFFFFF,16);if(!v->root){lv_mem_free(v);return NULL;}
    lv_obj_set_style_border_width(v->root,1,0);
    lv_obj_set_style_border_color(v->root,lv_color_hex(0xECF0F3),0);
    if(!lv_obj_add_event_cb(v->root,deleted,LV_EVENT_DELETE,v)){lv_obj_del(v->root);lv_mem_free(v);return NULL;}
    int head_y=expanded?0:10;
    v->flag=lv_img_create(v->root);if(!v->flag)goto failed;lv_obj_set_pos(v->flag,22,head_y+18);
    v->currency_name=label(v->root,82,head_y+6,108,"",&lv_font_instrument_sans_medium_10,0x7A8D9B);
    v->currency=label(v->root,82,head_y+23,100,"MULTI",&lv_font_instrument_sans_semibold_24,0x17212A);
    v->currency_target=box(v->root,12,12,165,48,0,0);
    if(!v->currency_target)goto failed;
    lv_obj_set_style_bg_opa(v->currency_target,0,0);
    lv_obj_set_style_radius(v->currency_target,10,0);
    lv_obj_set_style_bg_color(v->currency_target,lv_color_black(),LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(v->currency_target,20,LV_STATE_PRESSED);
    lv_obj_add_flag(v->currency_target,LV_OBJ_FLAG_CLICKABLE);
    v->title=label(v->root,expanded?208:194,head_y+12,168,"",&lv_font_instrument_sans_semibold_18,0x4C606E);
    v->subtitle=label(v->root,expanded?208:194,head_y+39,168,"",&lv_font_instrument_sans_medium_10,0x7A8D9B);
    v->segments=box(v->root,expanded?406:366,head_y+10,282,40,0xE7EDF0,10);
    if(!v->segments)goto failed;
    for(int i=0;i<2;i++){
        v->tabs[i]=button(v,0,0,138,34,ui_text_get(i?UI_TEXT_LIST_SERIAL_NUMBERS:UI_TEXT_LIST_DENOMINATIONS),0xE7EDF0,tab_cb);
        if(!v->tabs[i])goto failed;
        lv_obj_set_parent(v->tabs[i],v->segments);lv_obj_set_pos(v->tabs[i],3+i*138,3);
        lv_obj_t *l=lv_obj_get_child(v->tabs[i],0);lv_obj_set_align(l,LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(l,31,9);lv_obj_set_width(l,103);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_LEFT,0);
        lv_obj_set_style_text_font(l,&lv_font_instrument_sans_semibold_12,0);
        lv_obj_t *tab_icon=icon(v->tabs[i],8,8,i?LVGL_DIR"list_icons/barcode_24.png":LVGL_DIR"list_icons/receipt_24.png");
        if(!tab_icon)goto failed;
        lv_img_set_pivot(tab_icon,0,0);lv_img_set_zoom(tab_icon,192);
    }
    int metrics=v->width-342;
    v->pcs_title=label(v->root,metrics,head_y+10,64,ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_PCS),&lv_font_instrument_sans_medium_10,0x7A8D9B);
    v->pcs=label(v->root,metrics,head_y+25,66,"",&lv_font_instrument_sans_semibold_28,0x17212A);
    v->unit=label(v->root,metrics+70,head_y+10,140,"",&lv_font_instrument_sans_medium_10,0x7A8D9B);
    v->amount=label(v->root,metrics+70,head_y+25,140,"",&lv_font_instrument_sans_semibold_28,0x17212A);
    v->amount_symbol=label(v->root,metrics+140,head_y+32,50,"",&v->symbol_font.font,0x7A8D9B);
    v->back=button(v,v->width-114,head_y+10,92,44,ui_text_get(UI_TEXT_MULTI_BACK),0xE9EDF0,back_cb);
    if(!v->back)goto failed;
    lv_obj_t *back_text=lv_obj_get_child(v->back,0);lv_obj_set_align(back_text,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(back_text,34,13);lv_obj_set_width(back_text,52);
    if(!icon(v->back,12,13,LVGL_DIR"ui_icons/back_18.png"))goto failed;
    v->reject=button(v,v->width-148,head_y+10,126,44,"",0xFFFFFF,reject_cb);
    if(!v->reject)goto failed;
    lv_obj_t *reject_text=lv_obj_get_child(v->reject,0);lv_obj_set_align(reject_text,LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(reject_text,0,14);lv_obj_set_width(reject_text,64);
    text(reject_text,ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_REJECT));
    lv_obj_set_style_text_font(reject_text,&lv_font_instrument_sans_medium_10,0);
    v->reject_value=label(v->reject,70,8,52,"0",&lv_font_instrument_sans_semibold_28,0xA67834);
    v->currency_line=box(v->root,expanded?194:180,head_y+10,1,42,0xEFF3F5,0);
    v->head_line=box(v->root,22,expanded?60:80,v->width-44,1,0xEFF3F5,0);
    v->foot_line=box(v->root,22,v->height-38,v->width-44,1,0xEFF3F5,0);
    int body_y=expanded?84:108,rows=expanded?6:4;
    for(int i=0;i<3;i++)v->headers[i]=label(v->root,34,body_y-22,280,"",&lv_font_instrument_sans_medium_12,0x7A8D9B);
    v->content=box(v->root,22,body_y,v->width-44,rows*42,0xFFFFFF,0);if(!v->content)goto failed;
    lv_recycled_list_config_t cfg={0};cfg.width=v->width-44;cfg.content_width=v->width-68;cfg.rows=rows;cfg.row_height=42;cfg.create_row=create_row;cfg.bind_row=bind_row;cfg.context=v;cfg.scrollbar_right_inset=5;
    v->list=lv_recycled_list_create(v->content,&cfg);if(!v->list)goto failed;
    lv_port_indev_set_drag_obj(lv_recycled_list_object(v->list),true);
    lv_obj_add_event_cb(lv_recycled_list_object(v->list),tap,LV_EVENT_RELEASED,v);
    v->empty=label(v->content,30,rows*21-15,v->width-104,"",&lv_font_instrument_sans_medium_16,0x718795);if(!v->empty)goto failed;
    lv_obj_set_style_text_align(v->empty,LV_TEXT_ALIGN_CENTER,0);
    if(!ui_detail_reveal_init(&v->reveal,v->content))goto failed;
    v->foot=label(v->root,22,v->height-29,v->width-450,"",&lv_font_instrument_sans_medium_12,0x718795);
    v->count_label=label(v->root,v->width-200,v->height-29,178,"",&lv_font_instrument_sans_medium_12,0x718795);
    if(v->count_label)lv_obj_set_style_text_align(v->count_label,LV_TEXT_ALIGN_RIGHT,0);
    v->refresh=button(v,v->width-54,v->height-36,32,32,"",0xFFFFFF,refresh_cb);
    if(!v->refresh||!icon(v->refresh,7,7,LVGL_DIR"ui_icons/clear_18.png"))goto failed;
    v->search=box(v->root,v->width-356,v->height-36,292,32,0xF3F6F8,9);
    if(!v->search)goto failed;
    lv_obj_add_flag(v->search,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(v->search,search_cb,LV_EVENT_CLICKED,v);
    lv_damped_button_register(v->search,lv_color_hex(0xF3F6F8),lv_color_hex(0xDFE8EE));
    lv_obj_set_style_border_width(v->search,1,0);lv_obj_set_style_border_color(v->search,lv_color_hex(0xE7EDF0),0);
    if(!icon(v->search,10,7,LVGL_DIR"ui_icons/search_18.png"))goto failed;
    v->search_text=label(v->search,36,8,218,"",&lv_font_instrument_sans_medium_12,0x657F90);
    if(!v->search_text)goto failed;
    lv_label_set_long_mode(v->search_text,LV_LABEL_LONG_DOT);
    if(!v->currency_name||!v->pcs_title||!v->amount_symbol||!v->count_label||!v->currency_line||!v->head_line||!v->foot_line)goto failed;
    if(!v->currency||!v->title||!v->subtitle||!v->pcs||!v->unit||!v->amount||!v->back||!v->reject||!v->reject_value||!v->headers[0]||!v->headers[1]||!v->headers[2]||!v->foot||!v->refresh||!v->search||!v->tabs[0]||!v->tabs[1])goto failed;
    v->timer=lv_timer_create(poll,30,v);if(!v->timer)goto failed;lv_timer_pause(v->timer);hide(v->root,true);return v;
failed:lv_obj_del(v->root);return NULL;
}
lv_obj_t *ui_multi_detail_object(ui_multi_detail_t *v){return v?v->root:NULL;}
lv_obj_t *ui_multi_detail_scroll(ui_multi_detail_t *v){return v?lv_recycled_list_object(v->list):NULL;}
lv_obj_t *ui_multi_detail_currency_target(ui_multi_detail_t *v){return v?v->currency_target:NULL;}
void ui_multi_detail_visible(ui_multi_detail_t *v,bool visible)
{
    if(!v||v->visible==visible)return;v->visible=visible;hide(v->root,!visible);
    if(visible){lv_timer_resume(v->timer);v->dirty=true;ui_multi_detail_refresh(v);}
    else{lv_timer_pause(v->timer);lv_recycled_list_stop(v->list);ui_detail_reveal_cancel(&v->reveal);if(v->keyboard)lv_alnum_keyboard_hide(v->keyboard);}
}
void ui_multi_detail_select(ui_multi_detail_t *v,int index,int tab)
{
    if(!v||index < -2||index>=counting_multi_current()->count)return;
    if(app_command_runtime_count_start_busy())return;
    if(v->keyboard)lv_alnum_keyboard_hide(v->keyboard);
    if(index>=0 && index!=v->search_currency){search_reset_state(v);v->search_currency=index;}
    v->selected=index;v->tab=tab==1;v->dirty=true;
    multi_detail_status_t status=current_status(v);
    v->pending=index!=-1&&!available(status)&&status!=MULTI_DETAIL_LOADING;
    lv_recycled_list_refresh(v->list,0,true);ui_detail_reveal_begin(&v->reveal,false);ui_multi_detail_refresh(v);
}
bool ui_multi_detail_back(ui_multi_detail_t *v)
{
    if(!v||!v->visible)return false;
    if(v->keyboard&&lv_alnum_keyboard_is_visible(v->keyboard)){lv_alnum_keyboard_hide(v->keyboard);return true;}
    if(v->selected==-1)return false;ui_multi_detail_select(v,-1,0);return true;
}
void ui_multi_detail_destroy(ui_multi_detail_t *v){if(v)lv_obj_del(v->root);}
void ui_multi_detail_refresh(ui_multi_detail_t *v)
{
    if(!v||!v->visible)return;const counting_multi_t *m=counting_multi_current();
    uint32_t extra=counting_multi_extra_revision();
    if(v->generation!=m->generation||m->counting||v->selected>=m->count){
        if(v->selected!=-1||v->generation!=m->generation){v->selected=-1;v->pending=false;v->dirty=true;search_reset_state(v);v->search_currency=-1;ui_detail_reveal_cancel(&v->reveal);if(v->keyboard)lv_alnum_keyboard_hide(v->keyboard);}
        v->generation=m->generation;
    }
    if(v->pending&&!app_command_runtime_count_start_busy()&&!counting_multi_query_busy()&&!counting_multi_extra_busy()){
        if(v->selected==-2)counting_multi_reject_request(app_clock_uptime_ms());
        else if(v->selected>=0){if(v->tab)counting_multi_serial_request(v->selected,app_clock_uptime_ms());else counting_multi_request(v->selected,app_clock_uptime_ms());}
        v->pending=false;v->dirty=true;
    }
    multi_detail_status_t status=current_status(v);
    bool waiting=v->pending||status==MULTI_DETAIL_LOADING;
    if(!v->dirty&&v->revision==m->revision&&v->extra==extra){ui_detail_reveal_update(&v->reveal,!waiting);return;}
    if(v->selected>=0 && lv_alnum_keyboard_is_visible(v->keyboard))search_choices(v,true);
    /* Layout can be resolved inside recycled-list projection. Invalidate the
     * whole changed panel, not only its rows, after header geometry settles. */
    ui_update_batch_t batch;
    ui_update_batch_begin(&batch,v->root,UI_UPDATE_BATCH_INVALIDATE_ROOT);
    v->dirty=false;v->revision=m->revision;v->extra=extra;
    bool overview=v->selected==-1,rejects_view=v->selected==-2;
    const multi_currency_t *c=v->selected>=0?&m->currencies[v->selected]:NULL;
    flag(v->flag,c?c->code:"MULTI");text(v->currency,c?c->code:"MULTI");
    center_currency_glyphs(v);
    text(v->currency_name,c?currency_name(c->code):ui_text_get(UI_TEXT_MULTI_CURRENCY_MODE));
    text(v->title,ui_text_get(rejects_view?UI_TEXT_LIST_REJECT_ANALYSIS:overview?UI_TEXT_MULTI_RESULTS:UI_TEXT_MULTI_DETAILS));
    char scope[32];snprintf(scope,sizeof(scope),"MULTI / %s",c?c->code:"");
    text(v->subtitle,c?scope:ui_text_get(rejects_view?UI_TEXT_MULTI_GLOBAL_REJECT:UI_TEXT_MULTI_SEPARATE));
    /* Position getters still contain the previous layout inside this batch. */
    const int title_x=rejects_view?22:v->expanded?208:194;
    lv_obj_set_x(v->title,title_x);
    lv_obj_set_x(v->subtitle,title_x);
    lv_obj_set_width(v->title,c?168:v->width-560);
    lv_obj_set_width(v->subtitle,c?168:v->width-560);
    lv_obj_set_style_text_font(v->title,c?&lv_font_instrument_sans_semibold_18:&lv_font_instrument_sans_semibold_22,0);
    hide(v->flag,rejects_view);hide(v->currency,rejects_view);hide(v->currency_name,rejects_view);hide(v->currency_line,rejects_view);
    number(v->pcs,c?c->pcs:m->total_pcs);number(v->amount,c?c->amount:m->reject);number(v->reject_value,m->reject);
    hide(v->pcs,rejects_view);hide(v->pcs_title,rejects_view);
    hide(v->amount,overview);hide(v->unit,overview);hide(v->amount_symbol,!c);
    if(c){
        const char *symbol=currency_metadata_symbol(c->code);text(v->amount_symbol,symbol?symbol:"");
        lv_point_t size;const lv_font_t *font=&lv_font_instrument_sans_semibold_28;
        lv_txt_get_size(&size,lv_label_get_text(v->amount),font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        if(size.x>104){font=&lv_font_instrument_sans_semibold_20;lv_txt_get_size(&size,lv_label_get_text(v->amount),font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);}
        lv_obj_set_style_text_font(v->amount,font,0);
        lv_obj_set_x(v->amount_symbol,lv_obj_get_x(v->amount)+size.x+8);
    }
    char unit[64];snprintf(unit,sizeof(unit),"%s - %s",ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_AMOUNT),c?c->code:"");text(v->unit,c?unit:ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_REJECT));
    hide(v->back,overview);hide(v->refresh,overview);hide(v->reject,!overview);hide(v->segments,!c);hide(v->search,!c||!v->tab||v->expanded);
    lv_recycled_list_set_press_feedback(v->list,overview);
    hide(v->count_label,!overview);
    char count_text[48];snprintf(count_text,sizeof(count_text),ui_text_get(UI_TEXT_MULTI_CURRENCIES_FMT),(unsigned)m->count);text(v->count_label,count_text);
    hide(v->currency_target,!overview||v->expanded);
    for(int i=0;i<2;i++)lv_damped_button_set_palette(v->tabs[i],lv_color_hex(v->tab==i?0xFFFFFF:0xE7EDF0),lv_color_hex(0xDFE8EE));
    text(v->headers[0],overview?ui_text_get(UI_TEXT_MULTI_CURRENCY):rejects_view?ui_text_get(UI_TEXT_MULTI_ERROR_CODE):v->tab?ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_NO):ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_DENOM));
    text(v->headers[1],rejects_view?ui_text_get(UI_TEXT_LIST_COL_REASON):v->tab&&c?ui_text_get(UI_TEXT_LIST_COL_SERIAL_NUMBER):ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_PCS));
    text(v->headers[2],rejects_view?ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_PCS):v->tab&&c?ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_DENOM):ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_AMOUNT));
    if(overview)text(v->headers[2],ui_text_get(UI_TEXT_MULTI_AMOUNT_BY_CURRENCY));
    if(c){
        char heading[64];snprintf(heading,sizeof(heading),"%s - %s",ui_text_get(v->tab?UI_TEXT_PAGE01_DETAIL_COL_DENOM:UI_TEXT_PAGE01_DETAIL_COL_AMOUNT),c->code);text(v->headers[2],heading);
        if(!v->tab){snprintf(heading,sizeof(heading),"%s - %s",ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_DENOM),c->code);text(v->headers[0],heading);}
    }
    int middle,last;columns(v,&middle,&last);
    lv_obj_set_x(v->headers[0],34);lv_obj_set_x(v->headers[1],22+middle);lv_obj_set_x(v->headers[2],22+last);
    lv_obj_set_width(v->headers[0],middle-24);
    lv_obj_set_width(v->headers[1],last-middle-12);
    lv_obj_set_width(v->headers[2],v->width-44-last);
    v->count=overview?m->count:0;
    if(status==MULTI_DETAIL_READY&&!waiting){
        if(rejects_view)v->count=counting_multi_rejects()->count;
        else if(c&&v->tab){const multi_serial_cache_t *s=counting_multi_serials(v->selected);for(unsigned i=0;i<s->count;i++)if(matched(v,&s->rows[i]))v->indices[v->count++]=i;}
        else if(c)v->count=c->denom_count;
    }
    lv_recycled_list_refresh(v->list,v->count,false);hide(v->empty,v->count!=0);
    text(v->empty,ui_text_get(overview?UI_TEXT_MULTI_READY:waiting?UI_TEXT_MULTI_RECEIVING:available(status)?UI_TEXT_MULTI_NO_RECORDS:status==MULTI_DETAIL_TIMEOUT?UI_TEXT_MULTI_WAIT_END:UI_TEXT_MULTI_INVALID));
    text(v->foot,ui_text_get(rejects_view?UI_TEXT_MULTI_GLOBAL_REJECT:overview?UI_TEXT_MULTI_SEPARATE:v->tab?UI_TEXT_MULTI_UNKNOWN_DENOM:UI_TEXT_MULTI_CACHED));
    if(c&&v->tab){
        text(v->foot,"");
        char caption[384];
        bool filtered=v->denomination_mode?v->denomination_count>0:v->query[0]!=0;
        lv_damped_button_set_palette(v->search,lv_color_hex(filtered?0xE4F0FF:0xF3F6F8),lv_color_hex(0xD4E5F5));
        lv_obj_set_style_border_color(v->search,lv_color_hex(filtered?0x78ADEB:0xE7EDF0),0);
        lv_obj_set_style_text_color(v->search_text,lv_color_hex(filtered?0x176ACA:0x657F90),0);
        if(!filtered)snprintf(caption,sizeof(caption),"%s",ui_text_get(UI_TEXT_QUERY));
        else if(!v->denomination_mode)snprintf(caption,sizeof(caption),"%s",v->query);
        else {
            size_t used=0;caption[0]='\0';
            /* Choices arrive in descending order; entry caption is ascending. */
            for(unsigned i=v->denomination_count;i>0;--i) {
                int n=snprintf(caption+used,sizeof(caption)-used,"%s%lu",used?",":"",(unsigned long)v->denomination_query[i-1]);
                if(n<0 || (size_t)n>=sizeof(caption)-used)break;
                used+=(size_t)n;
            }
        }
        text(v->search_text,caption);
        if(filtered&&available(status)){
            snprintf(caption,sizeof(caption),ui_text_get(UI_TEXT_SERIAL_MATCH_COUNT),v->count,(unsigned)counting_multi_serials(v->selected)->count);
            text(v->foot,caption);
            if(!v->count)text(v->empty,ui_text_get(UI_TEXT_SERIAL_NO_MATCH));
        }
    }
    lv_obj_update_layout(v->content);
    ui_detail_reveal_update(&v->reveal,!waiting);
    ui_update_batch_end(&batch);
}
