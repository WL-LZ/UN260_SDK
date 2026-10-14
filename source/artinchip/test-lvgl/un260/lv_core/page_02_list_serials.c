#include "page_02_list_serials.h"
#include "un260/counting/counting_data_store.h"
#include "un260/font/ui_message_font.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/lv_recycled_list.h"
#include "un260/lv_resources/ui_page_background.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/ui_text.h"
#include "lv_port_indev.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define SERIALS_BODY_HEIGHT 270
#define SERIALS_CONTENT_WIDTH 1212
#define SERIALS_GAP 12
#define SERIALS_INK 0x17212A
#define SERIALS_BODY 0x4C606E
#define SERIALS_MUTED 0x7A8D9B
#define SERIALS_LINE 0xE7ECEF

struct page_02_list_serials {
    lv_obj_t *root,*panel,*title,*badge_text,*headers[3][3],*header_lines[3][2],*body,*range,*empty;
    lv_obj_t *previous,*next,*back,*main_button;
    lv_recycled_list_t *list;
    const page_02_list_data_t *projection;
    page_02_list_serials_close_fn close,main;
    void *context;
    const lv_font_t *serial_fonts[4];
    unsigned count,columns,row_height,rows;
    language_t language;
    bool visible,failed,reset_pending,monetary_supported;
};

static lv_obj_t *serials_surface(lv_obj_t *parent,int x,int y,int w,int h,
    uint32_t color,int radius)
{
    lv_obj_t *o=lv_obj_create(parent);
    if(!o)return NULL;
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_radius(o,radius,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
static lv_obj_t *serials_label(lv_obj_t *parent,const char *text,int x,int y,
    int width,const lv_font_t *font,uint32_t color,lv_text_align_t align)
{
    lv_obj_t *o=lv_label_create(parent);
    if(!o)return NULL;
    lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);
    lv_obj_set_size(o,width,lv_font_get_line_height(ui_message_font(font)));
    lv_obj_set_style_text_font(o,ui_message_font(font),0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_set_style_base_dir(o,LV_BASE_DIR_AUTO,0);
    lv_obj_set_style_text_align(o,align,0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text(o,text);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
static void serials_text(lv_obj_t *label,const char *text)
{
    if(strcmp(lv_label_get_text(label),text))lv_label_set_text(label,text);
}
static unsigned serials_slot(const page_02_list_serials_t *v,unsigned rank)
{return rank<v->projection->serial_count?v->projection->serial[rank]:UINT_MAX;}
static const char *serials_at(const page_02_list_serials_t *v,unsigned rank)
{
    unsigned slot=serials_slot(v,rank);
    const counting_sim_t *data=counting_data_current();
    return slot<(unsigned)counting_data_serial_scan_limit(data)&&data->sn_str[slot]
        ?data->sn_str[slot]:"";
}
static unsigned serials_group_width(unsigned columns)
{return (SERIALS_CONTENT_WIDTH-(columns-1)*SERIALS_GAP)/columns;}
static unsigned serials_col_x(unsigned columns,unsigned col)
{
    if(col==0)return 10;
    if(col==1)return 58;
    return serials_group_width(columns)-88;
}
static unsigned serials_col_width(unsigned columns,unsigned col)
{
    if(col==0)return 42;
    if(col==2)return 76;
    return serials_col_x(columns,2)-serials_col_x(columns,1)-8;
}
static unsigned serials_rank(const page_02_list_serials_t *v,unsigned group,unsigned col)
{
    if(!v->rows||col>=v->columns)return UINT_MAX;
    unsigned first=(group/v->rows)*v->rows*v->columns;
    if(first>=v->count)return UINT_MAX;
    unsigned remaining=LV_MIN(v->count-first,v->rows*v->columns);
    unsigned page_rows=(remaining+v->columns-1)/v->columns;
    unsigned row=group%v->rows;
    if(row>=page_rows)return UINT_MAX;
    unsigned rank=first+col*page_rows+row;
    return rank<v->count?rank:UINT_MAX;
}
static unsigned serials_group_for_rank(const page_02_list_serials_t *v,unsigned rank)
{
    if(!v->count||!v->rows||!v->columns)return 0;
    rank=LV_MIN(rank,v->count-1);
    unsigned capacity=v->rows*v->columns,page=rank/capacity,first=page*capacity;
    unsigned remaining=LV_MIN(v->count-first,capacity);
    unsigned page_rows=(remaining+v->columns-1)/v->columns;
    return page*v->rows+(rank-first)%page_rows;
}
static bool serials_ascii(const char *text)
{
    for(const unsigned char *p=(const unsigned char *)text;*p;p++)
        if(*p<32||*p>126)return false;
    return true;
}
static const lv_font_t *serials_record_font(const page_02_list_serials_t *v,const char *text)
{
    if(strlen(text)>12||!serials_ascii(text))return v->serial_fonts[0];
    unsigned width=serials_col_width(v->columns,1);
    for(unsigned i=0;i<4;i++){
        lv_point_t size;
        lv_txt_get_size(&size,text,v->serial_fonts[i],0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        if((unsigned)size.x<=width)return v->serial_fonts[i];
    }
    return v->serial_fonts[3];
}
static void serials_plan(page_02_list_serials_t *v,unsigned *columns,
    unsigned *height,unsigned *rows)
{
    unsigned normal_width=serials_col_width(3,1),one_width=serials_col_width(1,1);
    *columns=3;*height=30;*rows=9;
    unsigned widest=0,tallest=0;
    /* Protocol-sized printable serials use per-cell font fitting in the
     * bounded visible pool. Only model text needs whole-result measurement. */
    for(unsigned i=0;i<v->count;i++){
        const char *text=serials_at(v,i);
        if(strlen(text)<=12&&serials_ascii(text))continue;
        lv_point_t size;
        lv_txt_get_size(&size,text,v->serial_fonts[0],0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        if((unsigned)size.x>widest)widest=size.x;
        if((unsigned)size.y>tallest)tallest=size.y;
    }
    if(widest<=normal_width&&tallest<=30)return;
    *columns=1;
    if(tallest+2>*height)*height=tallest+2;
    if(widest>one_width){
        for(unsigned i=0;i<v->count;i++){
            const char *text=serials_at(v,i);
            if(strlen(text)<=12&&serials_ascii(text))continue;
            lv_point_t size;
            lv_txt_get_size(&size,text,v->serial_fonts[0],0,0,one_width,LV_TEXT_FLAG_NONE);
            unsigned required=(unsigned)size.y+2;
            if(required>*height)*height=required;
        }
    }
    *rows=SERIALS_BODY_HEIGHT/ *height;
    if(!*rows)*rows=1;
}
static void serials_number(lv_obj_t *label,double value,const lv_font_t *preferred)
{
    char text[40];snprintf(text,sizeof(text),"%.0f",value);
    const lv_font_t *font=preferred;
    const lv_font_t *fallback[]={&lv_font_instrument_sans_medium_18,
        &lv_font_instrument_sans_medium_16,&lv_font_instrument_sans_medium_14};
    lv_coord_t width=lv_obj_get_style_width(label,0);
    lv_point_t size;lv_txt_get_size(&size,text,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    for(unsigned i=0;i<sizeof(fallback)/sizeof(fallback[0])&&size.x>width;i++){
        if(fallback[i]->line_height>=font->line_height)continue;
        font=fallback[i];lv_txt_get_size(&size,text,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    }
    lv_obj_set_style_text_font(label,font,0);
    lv_obj_set_height(label,lv_font_get_line_height(font));
    serials_text(label,text);
}
static lv_obj_t *serials_row_create(lv_obj_t *parent,lv_coord_t width,void *context)
{
    page_02_list_serials_t *v=context;
    lv_obj_t *row=serials_surface(parent,0,0,width,v->row_height,0xFFFFFF,0);
    if(!row)return NULL;
    unsigned group_width=serials_group_width(v->columns);
    for(unsigned group=0;group<3;group++){
        lv_obj_t *cells=serials_surface(row,group*(group_width+SERIALS_GAP),0,
            group_width,v->row_height,0xFFFFFF,5);
        if(!cells){lv_obj_del(row);return NULL;}
        for(unsigned col=0;col<3;col++){
            const lv_font_t *font=col==0?&lv_font_instrument_sans_medium_14:
                col==1?v->serial_fonts[0]:&lv_font_instrument_sans_medium_20;
            lv_obj_t *label=serials_label(cells,"",serials_col_x(v->columns,col),
                0,serials_col_width(v->columns,col),font,
                col==0?SERIALS_MUTED:SERIALS_BODY,col==2?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT);
            if(!label){lv_obj_del(row);return NULL;}
            lv_obj_set_style_base_dir(label,LV_BASE_DIR_LTR,0);
            if(col==1){
                lv_obj_set_style_text_font(label,v->serial_fonts[0],0);
                lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
            }
        }
    }
    return row;
}
static void serials_row_bind(lv_obj_t *row,uint32_t group_index,void *context)
{
    page_02_list_serials_t *v=context;
    const counting_sim_t *data=counting_data_current();
    for(unsigned group=0;group<3;group++){
        lv_obj_t *cells=lv_obj_get_child(row,group);
        unsigned rank=serials_rank(v,group_index,group);
        if(group>=v->columns||rank>=v->count){
            lv_obj_add_flag(cells,LV_OBJ_FLAG_HIDDEN);continue;
        }
        lv_obj_clear_flag(cells,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(cells,lv_color_hex(group_index%2?0xF4F6F7:0xFFFFFF),0);
        unsigned slot=serials_slot(v,rank);
        bool valid=slot<(unsigned)counting_data_serial_scan_limit(data)&&data->sn_str[slot];
        lv_obj_t *number=lv_obj_get_child(cells,0);
        lv_obj_t *serial=lv_obj_get_child(cells,1);
        lv_obj_t *denom=lv_obj_get_child(cells,2);
        serials_number(number,slot==UINT_MAX?0:slot+1,&lv_font_instrument_sans_medium_14);
        const char *text=valid?data->sn_str[slot]:"";
        lv_obj_set_style_text_font(serial,serials_record_font(v,text),0);
        serials_text(serial,text);
        lv_obj_set_height(serial,LV_SIZE_CONTENT);
        if(v->monetary_supported)
            serials_number(denom,valid?data->denom_mix[slot]:0,&lv_font_instrument_sans_medium_20);
        else{
            lv_obj_set_style_text_font(denom,&lv_font_instrument_sans_medium_20,0);
            lv_obj_set_height(denom,lv_font_get_line_height(&lv_font_instrument_sans_medium_20));
            serials_text(denom,"--");
        }
        lv_obj_update_layout(cells);
        for(unsigned col=0;col<3;col++){
            lv_obj_t *label=lv_obj_get_child(cells,col);
            lv_obj_set_y(label,((int)v->row_height-lv_obj_get_height(label))/2);
        }
    }
}
static void serials_headers_layout(page_02_list_serials_t *v)
{
    unsigned width=serials_group_width(v->columns);
    const lv_font_t *font=ui_message_font(&lv_font_instrument_sans_medium_12);
    unsigned height=lv_font_get_line_height(font);
    for(unsigned group=0;group<3;group++){
        for(unsigned line=0;line<2;line++){
            lv_obj_t *rule=v->header_lines[group][line];
            lv_obj_set_pos(rule,12+group*(width+SERIALS_GAP),line?71:48);
            lv_obj_set_width(rule,width);
            if(group>=v->columns)lv_obj_add_flag(rule,LV_OBJ_FLAG_HIDDEN);
            else lv_obj_clear_flag(rule,LV_OBJ_FLAG_HIDDEN);
        }
        for(unsigned col=0;col<3;col++){
            lv_obj_t *label=v->headers[group][col];
            lv_obj_set_pos(label,12+group*(width+SERIALS_GAP)+serials_col_x(v->columns,col),
                49+(22-(int)height)/2);
            lv_obj_set_size(label,serials_col_width(v->columns,col),height);
            if(group>=v->columns)lv_obj_add_flag(label,LV_OBJ_FLAG_HIDDEN);
            else lv_obj_clear_flag(label,LV_OBJ_FLAG_HIDDEN);
        }
    }
}
static void serials_range_changed(const ui_list_window_t *w,void *context)
{
    page_02_list_serials_t *v=context;
    char text[96];
    snprintf(text,sizeof(text),ui_text_get(UI_TEXT_LIST_PAGE_FMT),
        (unsigned)ui_list_window_page_number(w),(unsigned)ui_list_window_pages(w));
    serials_text(v->range,text);
    lv_obj_clear_flag(v->previous,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(v->next,LV_OBJ_FLAG_HIDDEN);
    lv_damped_button_set_enabled(v->previous,w->count&&w->first>0);
    lv_damped_button_set_enabled(v->next,
        ui_list_window_page_number(w)<ui_list_window_pages(w));
}
static bool serials_list_build(page_02_list_serials_t *v,unsigned columns,
    unsigned height,unsigned rows,unsigned rank)
{
    if(!rows||rows>9||height>SERIALS_BODY_HEIGHT)return false;
    unsigned old_columns=v->columns,old_height=v->row_height,old_rows=v->rows;
    v->columns=columns;v->row_height=height;v->rows=rows;
    lv_recycled_list_config_t cfg={12,0,1236,rows,height,
        serials_row_create,serials_row_bind,serials_range_changed,v,0,0};
    lv_recycled_list_t *next=lv_recycled_list_create(v->body,&cfg);
    if(!next){
        v->columns=old_columns;v->row_height=old_height;v->rows=old_rows;
        return false;
    }
    if(v->list)lv_obj_del(lv_recycled_list_object(v->list));
    v->list=next;lv_port_indev_set_drag_obj(lv_recycled_list_object(next),true);
    lv_recycled_list_set_paged(next,true);
    lv_recycled_list_refresh(next,(v->count+columns-1)/columns,true);
    if(v->count)lv_recycled_list_scroll_to_index(next,serials_group_for_rank(v,rank));
    return true;
}
static void serials_close(lv_event_t *event)
{
    page_02_list_serials_t *v=lv_event_get_user_data(event);
    v->close(v->context);
}
static void serials_main(lv_event_t *event)
{
    page_02_list_serials_t *v=lv_event_get_user_data(event);
    v->main(v->context);
}
static void serials_page(lv_event_t *event)
{
    page_02_list_serials_t *v=lv_event_get_user_data(event);
    if(v->failed||!v->list)return;
    lv_recycled_list_page_step(v->list,lv_event_get_target(event)==v->previous?-1:1);
}
static void serials_icon_draw(lv_event_t *event)
{
    lv_obj_t *button=lv_event_get_target(event);
    unsigned kind=(uintptr_t)lv_event_get_user_data(event);
    lv_area_t area;lv_obj_get_coords(button,&area);
    int x=area.x1+lv_obj_get_width(button)/2,y=area.y1+lv_obj_get_height(button)/2;
    lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);
    line.color=lv_color_hex(lv_obj_has_state(button,LV_STATE_DISABLED)?0xAAB5BE:0x657F90);
    line.width=2;line.round_start=line.round_end=1;
    lv_point_t a,b,c;
    a=(lv_point_t){x+(kind==0?3:-3),y-6};
    b=(lv_point_t){x+(kind==0?-3:3),y};c=(lv_point_t){a.x,y+6};
    lv_draw_line(lv_event_get_draw_ctx(event),&line,&a,&b);
    lv_draw_line(lv_event_get_draw_ctx(event),&line,&b,&c);
}
static lv_obj_t *serials_footer_button(page_02_list_serials_t *v,int x,unsigned kind)
{
    const lv_damped_button_style_t style={0xFFFFFF,0xEBEBEB,0xF6F7F8,
        SERIALS_BODY,0xAAB5BE,12};
    lv_obj_t *button=lv_damped_button_create(v->panel,&style,"",&lv_font_instrument_sans_semibold_12);
    if(!button)return NULL;
    lv_obj_set_pos(button,x,342+(384-342-38)/2);lv_obj_set_size(button,38,38);
    if(!lv_obj_add_event_cb(button,serials_page,LV_EVENT_CLICKED,v)||
       !lv_obj_add_event_cb(button,serials_icon_draw,LV_EVENT_DRAW_MAIN,
            (void *)(uintptr_t)kind)){lv_obj_del(button);return NULL;}
    return button;
}
static void serials_deleted(lv_event_t *event)
{
    if(lv_event_get_target(event)!=lv_event_get_current_target(event))return;
    lv_mem_free(lv_event_get_user_data(event));
}
static void serials_main_layout(page_02_list_serials_t *v)
{
    const lv_font_t *font=ui_message_font(&lv_font_instrument_sans_medium_16);
    lv_obj_t *label=lv_damped_button_get_label(v->main_button);
    lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(label,font,0);
    lv_damped_button_set_text(v->main_button,ui_text_get(UI_TEXT_LIST_MAIN));
    lv_point_t size;lv_txt_get_size(&size,lv_label_get_text(label),font,0,0,
        LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    int width=LV_MAX(76,size.x+16),x=1080-8-width;
    lv_obj_set_pos(v->main_button,x,12);lv_obj_set_size(v->main_button,width,42);
    lv_obj_set_size(label,width-16,lv_font_get_line_height(font));
    lv_obj_center(label);
    lv_obj_set_width(v->title,x-84);
}
static void serials_translate(page_02_list_serials_t *v)
{
    static const ui_text_id_t headings[3]={UI_TEXT_PAGE01_DETAIL_COL_NO,
        UI_TEXT_LIST_COL_SERIAL_NUMBER,UI_TEXT_PAGE01_DETAIL_COL_DENOM};
    v->language=ui_lang_get();
    const lv_font_t *title_font=ui_message_font(&lv_font_instrument_sans_semibold_20);
    lv_obj_set_style_text_font(v->title,title_font,0);
    lv_obj_set_height(v->title,LV_MAX(28,lv_font_get_line_height(title_font)));
    const lv_font_t *range_font=ui_message_font(&lv_font_instrument_sans_medium_14);
    lv_obj_set_style_text_font(v->range,range_font,0);
    lv_obj_set_height(v->range,LV_MAX(24,lv_font_get_line_height(range_font)));
    lv_obj_set_style_text_font(v->empty,ui_message_font(&lv_font_instrument_sans_medium_16),0);
    lv_obj_set_height(v->empty,lv_font_get_line_height(ui_message_font(&lv_font_instrument_sans_medium_16)));
    for(unsigned group=0;group<3;group++)for(unsigned col=0;col<3;col++){
        lv_obj_set_style_text_font(v->headers[group][col],ui_message_font(&lv_font_instrument_sans_medium_12),0);
        serials_text(v->headers[group][col],ui_text_get(headings[col]));
    }
    const lv_font_t *back_font=ui_message_font(&lv_font_instrument_sans_medium_16);
    lv_obj_t *back_label=lv_damped_button_get_label(v->back);
    lv_obj_set_style_text_font(back_label,back_font,0);
    lv_obj_set_height(back_label,lv_font_get_line_height(back_font));lv_obj_center(back_label);
    lv_damped_button_set_text(v->back,ui_tr("Back"));
    serials_main_layout(v);
    serials_text(v->title,ui_text_get(UI_TEXT_LIST_SERIAL_NUMBERS));
    serials_text(v->badge_text,ui_text_get(UI_TEXT_PAGE01_DETAIL_BTN_B));
    const lv_font_t *serial_bases[]={&lv_font_instrument_sans_medium_20,
        &lv_font_instrument_sans_medium_18,&lv_font_instrument_sans_medium_16,
        &lv_font_instrument_sans_medium_14};
    for(unsigned i=0;i<4;i++)v->serial_fonts[i]=ui_message_font(serial_bases[i]);
}
void page_02_list_serials_refresh(page_02_list_serials_t *v,bool reset,bool monetary_supported)
{
    if(!v)return;
    v->reset_pending|=reset;v->monetary_supported=monetary_supported;
    if(!v->visible)return;
    reset=v->reset_pending;
    bool language_changed=v->language!=ui_lang_get();
    if(language_changed)serials_translate(v);
    unsigned rank=0;
    const ui_list_window_t *window=lv_recycled_list_window(v->list);
    if(window){
        rank=serials_rank(v,window->first,0);
        if(rank==UINT_MAX)rank=0;
    }
    v->count=v->projection->serial_count;
    unsigned columns,height,rows;serials_plan(v,&columns,&height,&rows);
    bool rebuild=v->failed||language_changed||!v->list||columns!=v->columns||
        height!=v->row_height||rows!=v->rows;
    if(rebuild){
        lv_recycled_list_stop(v->list);
        if(!serials_list_build(v,columns,height,rows,reset?0:rank)){
            v->failed=true;
            if(v->list)lv_obj_add_flag(lv_recycled_list_object(v->list),LV_OBJ_FLAG_HIDDEN);
            serials_text(v->empty,ui_text_get(UI_TEXT_SERIAL_UNAVAILABLE));
            lv_obj_clear_flag(v->empty,LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(v->range,LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(v->previous,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(v->next,LV_OBJ_FLAG_HIDDEN);
            return;
        }
    }else lv_recycled_list_refresh(v->list,(v->count+v->columns-1)/v->columns,reset);
    v->failed=false;v->reset_pending=false;
    serials_headers_layout(v);
    lv_obj_clear_flag(v->range,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lv_recycled_list_object(v->list),LV_OBJ_FLAG_HIDDEN);
    serials_text(v->empty,ui_text_get(UI_TEXT_LIST_NO_SERIAL_NUMBERS));
    if(v->count)lv_obj_add_flag(v->empty,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(v->empty,LV_OBJ_FLAG_HIDDEN);
}
page_02_list_serials_t *page_02_list_serials_create(lv_obj_t *parent,
    const page_02_list_data_t *projection,unsigned initial_rank,
    bool monetary_supported,page_02_list_serials_close_fn close,
    page_02_list_serials_close_fn main,void *context)
{
    if(!parent||!projection||!close||!main)return NULL;
    page_02_list_serials_t *v=lv_mem_alloc(sizeof(*v));if(!v)return NULL;
    memset(v,0,sizeof(*v));v->projection=projection;v->close=close;v->main=main;v->context=context;
    v->root=serials_surface(parent,0,0,1280,400,0xF2F5F7,0);
    if(!v->root){lv_mem_free(v);return NULL;}
    if(!lv_obj_add_event_cb(v->root,serials_deleted,LV_EVENT_DELETE,v)){
        lv_obj_del(v->root);lv_mem_free(v);return NULL;
    }
    ui_page_background_apply(v->root,UI_BACKGROUND_USER);
    lv_obj_add_flag(v->root,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(v->root,LV_OBJ_FLAG_GESTURE_BUBBLE);
    v->panel=serials_surface(v->root,16,8,1248,384,0xFFFFFF,15);
    if(!v->panel)goto failed;
    lv_obj_t *badge=serials_surface(v->panel,17,13,26,26,0x0074F8,7);
    if(!badge)goto failed;
    v->badge_text=serials_label(badge,"",0,0,26,
        &lv_font_instrument_sans_bold_14,0xFFFFFF,LV_TEXT_ALIGN_CENTER);
    v->title=serials_label(v->panel,"",52,12,1088,
        &lv_font_instrument_sans_semibold_20,SERIALS_INK,LV_TEXT_ALIGN_LEFT);
    v->back=lv_nav_back_text_button_create(v->root,1080,12,
        &lv_font_instrument_sans_medium_16,serials_close,v);
    v->main_button=lv_nav_back_text_button_create(v->root,1096,12,
        &lv_font_instrument_sans_medium_16,NULL,NULL);
    if(!v->badge_text||!v->title||!v->back||!v->main_button||
       !lv_obj_add_event_cb(v->main_button,serials_main,LV_EVENT_CLICKED,v))goto failed;
    lv_damped_button_set_exact_palette(v->back,lv_color_hex(0xF5F6F7),lv_color_hex(0xE8ECEF));
    lv_damped_button_set_exact_palette(v->main_button,lv_color_hex(0xF5F6F7),lv_color_hex(0xE8ECEF));
    lv_obj_center(v->badge_text);
    for(unsigned group=0;group<3;group++)for(unsigned col=0;col<3;col++){
        v->headers[group][col]=serials_label(v->panel,"",0,0,1,
            &lv_font_instrument_sans_medium_12,SERIALS_MUTED,
            col==2?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT);
        if(!v->headers[group][col])goto failed;
    }
    for(unsigned group=0;group<3;group++)for(unsigned line=0;line<2;line++){
        v->header_lines[group][line]=serials_surface(v->panel,12+group*408,
            line?71:48,396,1,SERIALS_LINE,0);
        if(!v->header_lines[group][line])goto failed;
    }
    if(!serials_surface(v->panel,0,342,1248,1,SERIALS_LINE,0))goto failed;
    v->body=serials_surface(v->panel,0,72,1248,270,0xFFFFFF,0);
    v->range=serials_label(v->panel,"",60,354,1128,
        &lv_font_instrument_sans_medium_14,SERIALS_MUTED,LV_TEXT_ALIGN_CENTER);
    if(!v->body||!v->range)goto failed;
    lv_obj_set_height(v->range,24);
    v->empty=serials_label(v->body,"",24,124,1200,
        &lv_font_instrument_sans_medium_16,SERIALS_MUTED,LV_TEXT_ALIGN_CENTER);
    v->previous=serials_footer_button(v,12,0);
    v->next=serials_footer_button(v,1198,1);
    if(!v->empty||!v->previous||!v->next)goto failed;
    serials_translate(v);v->visible=true;
    page_02_list_serials_refresh(v,true,monetary_supported);
    if(!v->list)goto failed;
    if(v->count)lv_recycled_list_scroll_to_index(v->list,
        serials_group_for_rank(v,initial_rank));
    lv_obj_move_foreground(v->empty);lv_obj_move_foreground(v->root);
    return v;
failed:
    lv_obj_del(v->root);return NULL;
}
void page_02_list_serials_set_visible(page_02_list_serials_t *v,bool visible)
{
    if(!v)return;
    v->visible=visible;
    if(visible){lv_obj_clear_flag(v->root,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(v->root);}
    else{lv_recycled_list_stop(v->list);lv_obj_add_flag(v->root,LV_OBJ_FLAG_HIDDEN);}
}
void page_02_list_serials_destroy(page_02_list_serials_t *v)
{if(v)lv_obj_del(v->root);}
