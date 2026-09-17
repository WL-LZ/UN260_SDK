#include "page_01_multi.h"
#include "un260/lv_system/ui_text.h"
#include "lv_page_event.h"
#include "lv_page_manager.h"
#include "un260/counting/counting_multi.h"
#include "un260/font/manrope_fonts.h"
#include "un260/font/main_fonts.h"
#include "un260/font/scaled_font.h"
#include "un260/currency/currency_metadata.h"
#include "un260/lv_resources/lv_img_init.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/gesture/gesture_service.h"
#include "un260/lv_system/app_clock.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static struct {
    lv_obj_t *root, *body, *title, *subtitle, *currency, *flag, *amount_symbol;
    lv_obj_t *pcs, *pcs_title, *amount, *amount_title, *back, *refresh, *foot, *count_label, *message, *currency_title;
    lv_obj_t *head[3], *rows[COUNTING_MULTI_MAX], *values[COUNTING_MULTI_MAX][3];
    scaled_font_t symbol_font;
    uint8_t symbol_pixels[1024];
    int selected, built_selected, built_count;
    uint32_t generation, revision;
    bool visible;
} view;

static lv_obj_t *box(lv_obj_t *p, int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *o=lv_obj_create(p); lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0); lv_obj_set_style_radius(o,radius,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    return o;
}
static lv_obj_t *label(lv_obj_t *p,int x,int y,int w,const char *text,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *o=lv_label_create(p); lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w); lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(o,font,0); lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_label_set_text(o,text); lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE); return o;
}
static void text(lv_obj_t *o,const char *s) { if(strcmp(lv_label_get_text(o),s)) lv_label_set_text(o,s); }
static void number(lv_obj_t *o,uint64_t n)
{
    char raw[32],out[48]; snprintf(raw,sizeof(raw),"%llu",(unsigned long long)n);
    size_t len=strlen(raw),j=0;
    for(size_t i=0;i<len;++i){out[j++]=raw[i];if(i+1<len&&(len-i-1)%3==0)out[j++]=',';}
    out[j]=0; text(o,out);
    if(o==view.amount||o==view.pcs) {
        const lv_font_t *font=&lv_font_manrope_bold_28;
        lv_point_t size;lv_txt_get_size(&size,out,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        if(size.x>lv_obj_get_width(o))font=&lv_font_instrument_sans_semibold_20;
        lv_obj_set_style_text_font(o,font,0);
    }
}
static void hidden(lv_obj_t *o,bool yes){if(yes)lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);}
static lv_obj_t *icon(lv_obj_t *p,int x,int y,const char *src)
{
    lv_obj_t *o=lv_img_create(p);lv_obj_set_pos(o,x,y);lv_img_set_src(o,src);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE);return o;
}
/* Zoom changes the drawn pixels, not the LVGL object's layout bounds. Align
 * using decoded dimensions so flags and labels share the same visual centre. */
static void flag_fit(lv_obj_t *o,const char *src,int width,int row_height,int x)
{
    lv_img_header_t h;
    if(lv_img_decoder_get_info(src,&h)!=LV_RES_OK||!h.w||!h.h)return;
    unsigned zoom=(unsigned)width*256U/h.w;
    if(!zoom)zoom=1;
    lv_img_set_src(o,src);lv_img_set_zoom(o,zoom);lv_img_set_pivot(o,0,0);
    lv_obj_set_pos(o,x,(row_height-(int)(h.h*zoom/256U))/2);
}
static const char *flag_source(const char *code)
{
    /* The legacy flag lookup defaults to IQD for unknown codes. Never assign
     * that country to an unrecognised controller currency. */
    return code && currency_metadata_symbol(code) ? get_currency_img(code) :
        LVGL_DIR"main_icons/currencies_32.png";
}
/* Currency codes are ASCII. Align visible glyphs, not the font line box:
 * its ascent/descent padding otherwise places the code below the flag. */
static void currency_align(void)
{
    const lv_font_t *font=lv_obj_get_style_text_font(view.currency,0);
    const char *code=lv_label_get_text(view.currency);
    int top=font->line_height, bottom=0;
    for (const char *p=code;*p;p++) {
        lv_font_glyph_dsc_t glyph;
        if (!lv_font_get_glyph_dsc(font,&glyph,(uint8_t)*p,(uint8_t)p[1]) || !glyph.box_h) continue;
        int y=font->line_height-font->base_line-glyph.box_h-glyph.ofs_y;
        if(y<top)top=y;
        if(y+glyph.box_h>bottom)bottom=y+glyph.box_h;
    }
    lv_obj_set_pos(view.currency,62,34-(top+bottom)/2);
}
static void button(lv_obj_t *o,lv_event_cb_t cb,void *ctx,uint32_t bg)
{
    lv_obj_add_flag(o,LV_OBJ_FLAG_CLICKABLE);
    lv_damped_button_register(o,lv_color_hex(bg),lv_color_hex(0xE7EDF1));
    lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,ctx);
}
bool page_01_multi_back(void)
{
    if(!view.visible||view.selected<0)return false;
    view.selected=-1;view.revision=UINT32_MAX;page_01_multi_refresh();return true;
}
static bool gesture(gesture_action_t a)
{
    return (a==GESTURE_ACTION_EXIT_PAGE||a==GESTURE_ACTION_HOME)&&page_01_multi_back();
}
static void back_cb(lv_event_t *e){(void)e;page_01_multi_back();}
static void query_cb(lv_event_t *e)
{
    const counting_multi_t *m=counting_multi_current();
    int index=(int)(intptr_t)lv_event_get_user_data(e)-1;
    const bool opening=index>=0;
    if(index<0)index=view.selected;
    if(index<0||index>=m->count||app_command_runtime_count_start_busy())return;
    if(!opening&&counting_multi_query_busy()) {
        text(view.foot,"Previous reply is still pending. Wait for its end marker.");return;
    }
    view.selected=index;
    if(!counting_multi_query_busy()&&
       (!opening||m->currencies[index].status==MULTI_DETAIL_NONE))
        counting_multi_request((unsigned)index,app_clock_uptime_ms());
    view.revision=UINT32_MAX;page_01_multi_refresh();
}
lv_obj_t *page_01_multi_create(lv_obj_t *parent)
{
    memset(&view,0,sizeof(view));view.selected=view.built_selected=-1;view.built_count=-1;view.revision=UINT32_MAX;
    scaled_font_init(&view.symbol_font,&lv_font_main_currency_32,56,view.symbol_pixels,sizeof(view.symbol_pixels));
    view.root=box(parent,108,12,1048,320,0xFFFFFF,16);
    lv_obj_set_style_border_width(view.root,1,0);lv_obj_set_style_border_color(view.root,lv_color_hex(0xECF0F3),0);
    lv_obj_t *target=box(view.root,22,8,160,68,0xFFFFFF,8);
    button(target,page_01_curr_btn_event_cb,NULL,0xFFFFFF);
    view.flag=icon(target,0,14,LVGL_DIR"main_icons/currencies_32.png");
    view.currency_title=label(target,58,5,100,"Currency mode",&lv_font_instrument_sans_medium_12,0x7A8D9B);
    view.currency=label(target,58,17,100,"MULTI",&lv_font_instrument_sans_semibold_24,0x17212A);
    box(view.root,184,22,1,40,0xEFF3F5,0);
    view.title=label(view.root,204,18,380,"Multi-currency count",&lv_font_instrument_sans_semibold_22,0x4C606E);
    view.subtitle=label(view.root,204,48,380,"Each currency, counted separately.",&lv_font_instrument_sans_medium_12,0x7A8D9B);
    view.pcs_title=label(view.root,665,20,65,"PCS",&lv_font_instrument_sans_medium_12,0x7A8D9B);
    view.pcs=label(view.root,642,37,90,"0",&lv_font_manrope_bold_28,0x17212A);
    lv_obj_set_style_text_align(view.pcs,LV_TEXT_ALIGN_LEFT,0);
    view.amount_title=label(view.root,758,20,146,"REJECT",&lv_font_instrument_sans_medium_12,0x7A8D9B);
    view.amount=label(view.root,748,37,154,"0",&lv_font_manrope_bold_28,0xA67834);
    lv_obj_set_style_text_align(view.amount,LV_TEXT_ALIGN_LEFT,0);
    view.amount_symbol=label(view.root,834,43,72,"",&view.symbol_font.font,0x7A8D9B);
    view.back=box(view.root,930,16,92,44,0xE9EDF0,10);button(view.back,back_cb,NULL,0xE9EDF0);
    icon(view.back,12,13,LVGL_DIR"ui_icons/back_18.png");label(view.back,36,12,52,"Back",&lv_font_instrument_sans_medium_16,0x4C606E);
    box(view.root,22,80,1004,1,0xEFF3F5,0);
    const int hx[]={32,470,705},hw[]={310,110,290};
    for(int i=0;i<3;i++){view.head[i]=label(view.root,hx[i],90,hw[i],"",&lv_font_instrument_sans_medium_12,0x7A8D9B);}
    view.body=box(view.root,22,110,1004,160,0xFFFFFF,0);
    lv_obj_add_flag(view.body,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);lv_obj_set_scroll_dir(view.body,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(view.body,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(view.body,lv_color_hex(0x9BAEBB),LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(view.body,LV_OPA_COVER,LV_PART_SCROLLBAR);
    lv_obj_set_style_width(view.body,4,LV_PART_SCROLLBAR);
    view.message=label(view.root,170,158,710,"",&lv_font_instrument_sans_medium_18,0x7A8D9B);
    lv_label_set_long_mode(view.message,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_align(view.message,LV_TEXT_ALIGN_CENTER,0);
    box(view.root,22,275,1004,1,0xEFF3F5,0);
    view.foot=label(view.root,22,290,850,"",&lv_font_instrument_sans_medium_12,0x7A8D9B);
    view.count_label=label(view.root,920,290,106,"",&lv_font_instrument_sans_medium_12,0x7A8D9B);
    lv_obj_set_style_text_align(view.count_label,LV_TEXT_ALIGN_RIGHT,0);
    view.refresh=box(view.root,930,276,96,43,0xFFFFFF,8);button(view.refresh,query_cb,NULL,0xFFFFFF);
    icon(view.refresh,4,13,LVGL_DIR"ui_icons/clear_18.png");label(view.refresh,28,12,68,"Refresh",&lv_font_instrument_sans_medium_14,0x617F91);
    hidden(view.root,true);return view.root;
}
void page_01_multi_visible(bool visible)
{
    if(!view.root)return;
    view.visible=visible;
    if(visible){gesture_service_set_page_policy(UI_PAGE_MAIN,NULL,gesture);page_01_multi_refresh();}
    else gesture_service_clear_page_policy(UI_PAGE_MAIN);
}
void page_01_multi_destroy(void){gesture_service_clear_page_policy(UI_PAGE_MAIN);memset(&view,0,sizeof(view));}
lv_obj_t *page_01_multi_scroll(void){return view.visible?view.body:NULL;}
void page_01_multi_refresh(void)
{
    if(!view.root||!view.visible)return;
    const counting_multi_t *m=counting_multi_current();
    if(view.generation!=m->generation||m->counting||view.selected>=m->count)view.selected=-1;
    if(view.revision==m->revision)return;
    view.revision=m->revision;view.generation=m->generation;
    const multi_currency_t *c=view.selected>=0?&m->currencies[view.selected]:NULL;
    text(view.currency,c?c->code:"MULTI");
    text(view.currency_title,c?"Currency":"Currency mode");
    lv_obj_set_x(view.pcs_title,c?595:750);text(view.pcs_title,c?"PCS":"TOTAL PCS");
    lv_obj_set_style_text_align(view.pcs_title,LV_TEXT_ALIGN_LEFT,0);
    lv_obj_set_x(view.pcs,c?595:750);
    lv_obj_set_x(view.amount_title,c?680:900);
    lv_obj_set_style_text_align(view.amount_title,LV_TEXT_ALIGN_LEFT,0);
    lv_obj_set_x(view.amount,c?680:900);
    lv_obj_set_width(view.amount,c?154:126);
    const char *symbol=c?currency_metadata_symbol(c->code):NULL;
    text(view.amount_symbol,symbol?symbol:"");hidden(view.amount_symbol,!c);
    const char *flag=c?flag_source(c->code):get_currency_img("MULTI");
    if(flag)flag_fit(view.flag,flag,44,68,0);
    hidden(view.currency_title,c!=NULL);
    currency_align();
    text(view.title,c?"Denomination details":"Multi-currency count");
    char buf[80];snprintf(buf,sizeof(buf),"MULTI / %s",c?c->code:"");
    text(view.subtitle,c?buf:"Each currency, counted separately.");
    number(view.pcs,c?c->pcs:m->total_pcs);number(view.amount,c?c->amount:m->reject);
    if(c) {
        lv_point_t size;
        lv_txt_get_size(&size,lv_label_get_text(view.amount),lv_obj_get_style_text_font(view.amount,0),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        lv_obj_set_x(view.amount_symbol,680+size.x+8);
    }
    snprintf(buf,sizeof(buf),"AMOUNT - %s",c?c->code:"");text(view.amount_title,c?buf:"REJECT");
    lv_obj_set_style_text_color(view.amount,lv_color_hex(c?0x17212A:0xA67834),0);
    hidden(view.back,!c);hidden(view.refresh,!c);
    hidden(view.count_label,c!=NULL);
    snprintf(buf,sizeof(buf),"%u currencies",m->count);text(view.count_label,buf);
    text(view.head[0],c?"DENOMINATION":"CURRENCY");text(view.head[1],"PCS");
    lv_obj_set_x(view.head[1],c?470:390);
    lv_obj_set_x(view.head[2],c?727:680);lv_obj_set_width(view.head[2],c?279:170);
    lv_obj_set_style_text_align(view.head[2],LV_TEXT_ALIGN_LEFT,0);
    snprintf(buf,sizeof(buf),"AMOUNT - %s",c?c->code:"");
    text(view.head[2],c?buf:"AMOUNT BY CURRENCY");
    const bool ready=c&&c->status==MULTI_DETAIL_READY;
    unsigned count=c?(ready?c->denom_count:0):m->count;
    if(view.built_selected!=view.selected||view.built_count!=(int)count){
        lv_coord_t scroll=view.built_selected==view.selected?lv_obj_get_scroll_y(view.body):0;
        lv_obj_clean(view.body);
        view.built_selected=view.selected;view.built_count=count;
        memset(view.rows,0,sizeof(view.rows));
        for(unsigned i=0;i<count;i++){
            uint32_t bg=i%2?0xF4F6F7:0xFFFFFF;
            lv_obj_t *row=box(view.body,0,i*(c?40:48),996,c?40:48,bg,6);view.rows[i]=row;
            if(!c){
                button(row,query_cb,(void*)(intptr_t)(i+1),bg);
                const char *src=flag_source(m->currencies[i].code);
                if(src){lv_obj_t *f=icon(row,10,12,src);flag_fit(f,src,44,48,10);}
                label(row,874,16,80,"DETAILS",&lv_font_instrument_sans_medium_12,0x7A8D9B);
                icon(row,970,15,LVGL_DIR"ui_icons/expand_18.png");
            }
            int symbol_end=c?12:658;
            {
                const char *row_symbol=currency_metadata_symbol(c?c->code:m->currencies[i].code);
                lv_obj_t *s=label(row,c?12:658,0,48,row_symbol?row_symbol:"",&view.symbol_font.font,0x7A8D9B);
                lv_obj_update_layout(s);lv_obj_set_y(s,((c?40:48)-lv_obj_get_height(s))/2);
                lv_point_t size;
                lv_txt_get_size(&size,row_symbol?row_symbol:"",&view.symbol_font.font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
                if(size.x)symbol_end+=size.x+10;
            }
            const int xs[]={c?symbol_end:72,c?448:368,c?705:symbol_end},ws[]={244,110,c?279:856-symbol_end};
            for(int j=0;j<3;j++){
                view.values[i][j]=label(row,xs[j],c?8:12,ws[j],"",&lv_font_instrument_sans_semibold_20,0x4C606E);
                lv_obj_set_style_text_align(view.values[i][j],LV_TEXT_ALIGN_LEFT,0);
            }
        }
        lv_obj_update_layout(view.body);lv_obj_scroll_to_y(view.body,scroll,LV_ANIM_OFF);
    }
    for(unsigned i=0;i<count;i++){
        if(c){number(view.values[i][0],c->denom[i].value);number(view.values[i][1],c->denom[i].pcs);number(view.values[i][2],(uint64_t)c->denom[i].value*c->denom[i].pcs);}
        else{const multi_currency_t *r=&m->currencies[i];text(view.values[i][0],r->code);number(view.values[i][1],r->pcs);number(view.values[i][2],r->amount);}
        for(int j=0;j<3;j++) {
            lv_obj_update_layout(view.values[i][j]);
            lv_obj_set_y(view.values[i][j],((c?40:48)-lv_obj_get_height(view.values[i][j]))/2);
        }
    }
    hidden(view.message,count>0);
    const char *message="Ready for multiple currencies";
    if(c)switch(c->status){
        case MULTI_DETAIL_NONE:message="Waiting for this currency's details...";break;
        case MULTI_DETAIL_LOADING:message="Receiving denomination details...";break;
        case MULTI_DETAIL_TIMEOUT:message=counting_multi_query_busy()?"Reply timed out. Waiting for its end marker; no overlapping queries.":"Reply timed out. Please refresh.";break;
        case MULTI_DETAIL_EMPTY:message="The controller returned no denomination details.";break;
        default:message="Incomplete or inconsistent reply. Currency totals are unchanged.";break;
    }
    text(view.message,message);
    text(view.foot,m->overflow?"Currency capacity exceeded; list is incomplete.":c?(ready?"Complete reply":"Totals from controller; unconfirmed details are not displayed."):ui_text_get(UI_TEXT_MULTI_HISTORY_HINT));
}
