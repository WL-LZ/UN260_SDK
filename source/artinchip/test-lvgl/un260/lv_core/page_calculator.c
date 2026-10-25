#include "un260/font/ui_message_font.h"
#include "page_calculator.h"
#include "lv_page_manager.h"
#include "workspace_style.h"
#include "un260/app_service/calculator_import.h"
#include "un260/storage/workspace_store.h"
#include "un260/lv_components/ui_notice.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MODE_ORDINARY,MODE_FX };
enum { A_BACK=1000,A_ORDINARY,A_FX,A_CURRENCY,A_TO_CURRENCY,A_IMPORT,A_OPERATORS,A_CLOSE,A_AMOUNT,A_RATE,A_SWAP,A_NEXT,A_PREV,A_CURRENCY_ITEM=2000,A_IMPORT_ITEM=2100 };
typedef struct {lv_obj_t *root,*body,*header,*overlay,*status_line;lv_timer_t *timer;bool queued,auxiliary_overlay;unsigned workspace_revision;char feedback[180];} calculator_view_t;
static calculator_view_t calculator;
static struct {char text[24],code[4],to[4],rate[24],fx_amount[24],equation[96];int64_t left;char op;bool fresh;unsigned mode;calculator_snapshot_t source;} calc={.text="0"};
static char currency_choices[64][4];static unsigned choice_count,choice_page;static bool currency_target;
static unsigned import_mode;static uint32_t last_actor=UINT32_MAX;
static void draw(void *);static void action(lv_event_t *);
static uint32_t actor_id(void){const workspace_user_t *u=workspace_active(workspace_store_get());return u?u->id:0;}
static const char *operator_text(uint32_t id,const char *name)
{const char *current=workspace_record_name(workspace_store_get(),id,name);return !id&&!strcmp(current,"Device workspace")?ui_tr("Device workspace"):current;}
static void queue(calculator_view_t *v){if(v->root&&!v->queued){v->queued=true;lv_async_call(draw,v);}}
static lv_obj_t *button(calculator_view_t *v,lv_obj_t *p,int x,int y,int w,int h,const char *s,unsigned id,unsigned style)
{lv_obj_t *o=cw_button(p,x,y,w,h,s,style==1?CW_BLUE:style==2?CW_SOFT:CW_PANEL,style==1?0xFFFFFF:style==2?CW_BLUE:CW_INK,action,(void *)(uintptr_t)id);lv_obj_set_user_data(o,v);if(id==A_CLOSE)lv_nav_button_mark_back(o);return o;}
static void hint(lv_obj_t *p,int x,int y,int w,const char *s){cw_label(p,x,y,w,s,&lv_font_instrument_sans_medium_12,CW_MUTED);}
static void money(int64_t n,const char *code,char *out,size_t size){char raw[40];int d=calculator_currency_decimals(code);calculator_format(n,d<0?6:(unsigned)d,raw,sizeof(raw));snprintf(out,size,"%s %s",raw,code);}
static void amount(lv_obj_t *p,int x,int y,int w,int64_t n,const char *code,const lv_font_t *font)
{
    char s[48];int d=calculator_currency_decimals(code);calculator_format(n,d<0?6:(unsigned)d,s,sizeof(s));
    lv_point_t extent;lv_txt_get_size(&extent,s,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    const lv_font_t *number_font=font;
    if(extent.x>w-48){number_font=&lv_font_instrument_sans_medium_24;lv_txt_get_size(&extent,s,number_font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);}
    lv_obj_t *value=cw_label(p,x,y,w-48,s,number_font,CW_INK);
    lv_obj_set_style_base_dir(value,LV_BASE_DIR_LTR,0);
    lv_obj_set_style_text_align(value,LV_TEXT_ALIGN_LEFT,0);
    int unit_x=extent.x+8;if(unit_x>w-40)unit_x=w-40;
    lv_obj_t *unit=cw_label(p,x+unit_x,y+number_font->line_height-18,40,code,&lv_font_instrument_sans_medium_15,CW_MUTED);
    lv_obj_set_style_base_dir(unit,LV_BASE_DIR_LTR,0);
}
static void trim(char *s){if(strchr(s,'.')){char *p=s+strlen(s)-1;while(p>s&&*p=='0')*p--=0;if(*p=='.')*p=0;}}
static void default_code(char code[4]){if(!code[0]){char list[64][4];if(calculator_import_currencies(list,64))memcpy(code,list[0],4);}}
static void explain(calculator_view_t *v,const char *s){snprintf(v->feedback,sizeof(v->feedback),"%s",s);ui_notice_post_text(UI_NOTICE_WARNING,"calculator",UI_N_("Calculator"),s);queue(v);}
static const char *error_text(calculator_error_t e){switch(e){case CALCULATOR_PRECISION:return UI_N_("Check currency decimal places.");case CALCULATOR_OVERFLOW:return UI_N_("The amount is too large.");case CALCULATOR_DIV_ZERO:return UI_N_("Cannot divide by zero.");default:return UI_N_("Check the amount, date and storage status, then try again.");}}
static void close_overlay(calculator_view_t *v){if(v->overlay)lv_obj_del(v->overlay);v->overlay=NULL;v->auxiliary_overlay=false;}
static lv_obj_t *overlay(calculator_view_t *v,const char *title)
{
    close_overlay(v);v->overlay=cw_box(v->root,0,0,1280,400,0x1B2732,0);lv_obj_set_style_bg_opa(v->overlay,56,0);lv_obj_add_flag(v->overlay,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_user_data(v->overlay,v);lv_obj_add_event_cb(v->overlay,action,LV_EVENT_CLICKED,(void *)A_CLOSE);
    lv_obj_t *panel=cw_box(v->overlay,330,16,620,368,0xFFFFFF,20);lv_obj_add_flag(panel,LV_OBJ_FLAG_CLICKABLE);cw_label(panel,26,22,470,title,&lv_font_instrument_sans_semibold_20,CW_INK);button(v,panel,490,12,104,42,ui_tr("Cancel"),A_CLOSE,0);return panel;
}

static void currency_picker(calculator_view_t *v,bool target)
{currency_target=target;choice_count=calculator_import_currencies(currency_choices,64);lv_obj_t *o=overlay(v,ui_tr("Select currency"));v->auxiliary_overlay=true;for(unsigned i=0;i<12&&choice_page*12+i<choice_count;++i){unsigned n=choice_page*12+i;button(v,o,26+(i%4)*142,86+(i/4)*65,132,52,currency_choices[n],A_CURRENCY_ITEM+n,0);}button(v,o,354,304,112,42,"<",A_PREV,0);button(v,o,480,304,112,42,">",A_NEXT,0);}
static void apply_import(calculator_view_t *v,unsigned index)
{
    v->feedback[0]=0;calculator_snapshot_t s;if(!calculator_import_current(index,&s)){explain(v,UI_N_("The current result is no longer available."));return;}close_overlay(v);
    {if(import_mode==MODE_FX&&strcmp(calc.code,s.code))calc.rate[0]=0;memcpy(calc.code,s.code,4);calc.source=s;calculator_format(s.amount,calculator_currency_decimals(s.code),import_mode==MODE_FX?calc.fx_amount:calc.text,24);calc.op=0;calc.equation[0]=0;calc.fresh=true;if(!strcmp(calc.to,s.code)){calc.to[0]=0;calc.rate[0]=0;}}
    queue(v);
}
static void import_picker(calculator_view_t *v)
{const char *reason=calculator_import_current_blocker();if(reason){explain(v,reason);return;}unsigned n=calculator_import_current_count();if(!n){explain(v,UI_N_("No complete saved monetary count is available."));return;}import_mode=calc.mode;if(n==1){apply_import(v,0);return;}lv_obj_t *o=overlay(v,ui_tr("Choose a currency from this count"));v->auxiliary_overlay=true;hint(o,26,64,560,ui_tr("MULTI currencies are imported separately."));for(unsigned i=0;i<4&&choice_page*4+i<n;++i){calculator_snapshot_t s;if(!calculator_import_current(choice_page*4+i,&s))continue;char text[100],a[64];money(s.amount,s.code,a,sizeof(a));snprintf(text,sizeof(text),"#%u    %s",s.record,a);button(v,o,26,96+i*48,568,42,text,A_IMPORT_ITEM+i,0);}button(v,o,354,306,112,42,"<",A_PREV,0);button(v,o,480,306,112,42,">",A_NEXT,0);lv_obj_add_flag(v->overlay,LV_OBJ_FLAG_USER_1);}
static void keyboard_value(const char *text,void *context)
{
    unsigned id=(uintptr_t)context;calculator_view_t *v=&calculator;
    if(!v->root)return;v->feedback[0]=0;int64_t n;
    if(!calculator_parse(text,&n)||n<0){explain(v,error_text(CALCULATOR_INVALID));return;}
    if(id==A_RATE){if(!n||n>999999999999LL){explain(v,UI_N_("Enter a positive rate with up to six decimals."));return;}snprintf(calc.rate,sizeof(calc.rate),"%s",text);}
    else if(id==A_AMOUNT){snprintf(calc.fx_amount,sizeof(calc.fx_amount),"%s",text);calc.source.count=0;}
    queue(v);
}
static void keyboard(unsigned id,const char *title,const char *value,bool text)
{settings_detail_keyboard_show(ui_tr(title),value,text?40:22,text?SETTINGS_DETAIL_KEYBOARD_TEXT:SETTINGS_DETAIL_KEYBOARD_DECIMAL,keyboard_value,(void *)(uintptr_t)id);}
static void key(unsigned key)
{
    calculator.feedback[0]=0;if(key=='C')memset(&calc.source,0,sizeof(calc.source));else calc.source.count=0;
    if(key=='C'){snprintf(calc.text,sizeof(calc.text),"0");calc.op=0;calc.fresh=false;calc.left=0;calc.equation[0]=0;}
    else if(key=='B'){size_t n=strlen(calc.text);if(n>1)calc.text[n-1]=0;else strcpy(calc.text,"0");if(!strcmp(calc.text,"-"))strcpy(calc.text,"0");calc.fresh=false;}
    else if(key=='S'){int64_t n;if(calculator_parse(calc.text,&n)){calculator_format(-n,6,calc.text,sizeof(calc.text));trim(calc.text);}}
    else if(strchr("+-*/=",(char)key)){
        int64_t n;if(!calculator_parse(calc.text,&n)){explain(&calculator,error_text(CALCULATOR_INVALID));return;}
        if(calc.op&&!calc.fresh){char lhs[40];calculator_format(calc.left,6,lhs,sizeof(lhs));trim(lhs);snprintf(calc.equation,sizeof(calc.equation),"%s %c %s =",lhs,calc.op,calc.text);calculator_error_t e=calculator_calculate(calc.left,calc.op,n,&n);if(e){explain(&calculator,error_text(e));return;}calculator_format(n,6,calc.text,sizeof(calc.text));trim(calc.text);}
        calc.left=n;calc.op=key=='='?0:(char)key;calc.fresh=true;
    }else{
        if(!calc.op)calc.equation[0]=0;char text[24];snprintf(text,sizeof(text),"%s",calc.fresh?"0":calc.text);if(key=='.'&&strchr(text,'.'))return;
        size_t len=strlen(text);if(len>=sizeof(text)-2)return;if(!strcmp(text,"0")&&key!='.')len=0;text[len]=(char)key;text[len+1]=0;
        int64_t n;if(!calculator_parse(text,&n))return;strcpy(calc.text,text);calc.fresh=false;
    }queue(&calculator);
}
static void action(lv_event_t *event)
{
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED)return;calculator_view_t *v=lv_obj_get_user_data(lv_event_get_current_target(event));unsigned id=(uintptr_t)lv_event_get_user_data(event);if(!v||!v->root)return;
    if(id<128){key(id);return;}
    if(id==A_CLOSE){close_overlay(v);queue(v);return;}
    if(id==A_BACK){settings_detail_keyboard_hide();close_overlay(v);if(!ui_manager_pop_page())ui_manager_switch(UI_PAGE_MAIN);return;}
    if(id==A_OPERATORS){close_overlay(v);ui_manager_push_page(UI_PAGE_OPERATORS);return;}
    if(id==A_ORDINARY||id==A_FX){if(id==A_FX&&calc.mode!=MODE_FX){snprintf(calc.fx_amount,sizeof(calc.fx_amount),"%s",calc.text);calc.rate[0]=0;}calc.mode=id==A_ORDINARY?MODE_ORDINARY:MODE_FX;queue(v);return;}
    if(id==A_CURRENCY||id==A_TO_CURRENCY){choice_page=0;currency_picker(v,id==A_TO_CURRENCY);return;}
    if(id>=A_CURRENCY_ITEM&&id<A_CURRENCY_ITEM+64){unsigned i=id-A_CURRENCY_ITEM;if(i>=choice_count)return;{memcpy(currency_target?calc.to:calc.code,currency_choices[i],4);calc.rate[0]=0;if(!currency_target){strcpy(calc.text,"0");calc.fx_amount[0]=0;calc.op=0;memset(&calc.source,0,sizeof(calc.source));calc.equation[0]=0;}}close_overlay(v);queue(v);return;}
    if(id==A_IMPORT){choice_page=0;import_picker(v);return;}
    if(id>=A_IMPORT_ITEM&&id<A_IMPORT_ITEM+4){apply_import(v,choice_page*4+id-A_IMPORT_ITEM);return;}
    if((id==A_NEXT||id==A_PREV)&&v->overlay){bool imports=lv_obj_has_flag(v->overlay,LV_OBJ_FLAG_USER_1);unsigned n=imports?calculator_import_current_count():choice_count,per=imports?4:12;if(id==A_PREV&&choice_page)--choice_page;if(id==A_NEXT&&(choice_page+1)*per<n)++choice_page;if(imports)import_picker(v);else currency_picker(v,currency_target);return;}
    if(id==A_RATE){keyboard(id,UI_N_("Manual exchange rate"),calc.rate,false);return;}
    if(id==A_SWAP){char c[4];memcpy(c,calc.code,4);memcpy(calc.code,calc.to,4);memcpy(calc.to,c,4);calc.fx_amount[0]=calc.rate[0]=0;memset(&calc.source,0,sizeof(calc.source));queue(v);return;}
    if(id==A_AMOUNT){char s[32];snprintf(s,sizeof(s),"%s",calc.fx_amount);keyboard(id,UI_N_("Amount"),s,false);return;}
}
static void header(calculator_view_t *v)
{
    lv_obj_clean(v->header);lv_obj_t *back=lv_nav_back_text_button_create(v->header,24,15,ui_message_font(&lv_font_instrument_sans_medium_16),action,(void *)A_BACK);lv_obj_set_user_data(back,v);cw_label(v->header,114,23,260,ui_tr(UI_N_("Calculator")),&lv_font_instrument_sans_semibold_24,CW_INK);
    lv_obj_t *seg=cw_box(v->header,486,15,288,42,0xE9EEF3,13);
    const char *titles[2]={UI_N_("Ordinary"),UI_N_("Exchange rate")};
    for(unsigned i=0;i<2;++i){bool selected=(calc.mode==MODE_FX)==(bool)i;lv_obj_t *b=button(v,seg,4+i*140,4,140,34,ui_tr(titles[i]),(i?A_FX:A_ORDINARY),0);lv_obj_set_style_bg_color(b,lv_color_hex(selected?0xFFFFFF:0xE9EEF3),0);lv_obj_set_style_text_color(lv_obj_get_child(b,0),lv_color_hex(selected?CW_BLUE:CW_MUTED),0);lv_obj_set_style_radius(b,9,0);}
    const workspace_user_t *u=workspace_active(workspace_store_get());lv_obj_t *person=button(v,v->header,1076,15,180,42,u&&u->id?u->name:ui_tr("Device workspace"),A_OPERATORS,0);lv_obj_set_style_bg_color(person,lv_color_hex(0xF0EDF9),0);lv_obj_set_style_text_color(lv_obj_get_child(person,0),lv_color_hex(0x7358AD),0);
}
static void draw_calculator(calculator_view_t *v)
{
    default_code(calc.code);if(!calc.to[0]){char list[64][4];unsigned n=calculator_import_currencies(list,64);for(unsigned i=0;i<n;++i)if(strcmp(list[i],calc.code)){memcpy(calc.to,list[i],4);break;}}char s[120];lv_obj_t *b=v->body;
    if(calc.mode==MODE_ORDINARY){
        lv_obj_t *left=cw_box(b,0,0,492,304,0xFFFFFF,17);button(v,left,399,16,73,42,calc.code,A_CURRENCY,0);
        if(calc.op){char lhs[40];calculator_format(calc.left,6,lhs,sizeof(lhs));trim(lhs);snprintf(s,sizeof(s),"%s %c %s",lhs,calc.op,calc.fresh?"...":calc.text);}else snprintf(s,sizeof(s),"%s",calc.equation);
        lv_obj_t *formula=cw_label(left,20,72,452,s,&lv_font_instrument_sans_medium_20,CW_MUTED);lv_obj_set_style_text_align(formula,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_height(formula,30);lv_label_set_long_mode(formula,LV_LABEL_LONG_SCROLL_CIRCULAR);
        const lv_font_t *nf=strlen(calc.text)>12?&lv_font_instrument_sans_semibold_37:&lv_font_instrument_sans_semibold_52;
        lv_obj_t *n=cw_label(left,20,117,452,calc.text,nf,CW_INK);lv_obj_set_style_text_align(n,LV_TEXT_ALIGN_CENTER,0);
        if(calc.source.record){snprintf(s,sizeof(s),"#%u · %s",calc.source.record,calc.source.code);hint(left,20,190,452,s);}
        button(v,left,20,242,144,42,ui_tr("Import count"),A_IMPORT,2);button(v,left,174,242,144,42,ui_tr("Exchange rate"),A_FX,2);
        const char *keys[]={UI_N_("Clear"),"+/-","<","/","1","2","3","*","4","5","6","-","7","8","9","+","0",".","="};const unsigned ids[]={'C','S','B','/','1','2','3','*','4','5','6','-','7','8','9','+','0','.','='};
        for(unsigned i=0;i<19;++i){int x=510+(i%4)*182,y=(i/4)*62;lv_obj_t *key=button(v,b,x,y,i==18?358:174,54,ui_tr(keys[i]),ids[i],i==18?1:(i%4==3?2:0));if(i!=18&&i%4!=3)lv_obj_set_style_bg_color(key,lv_color_hex(0xF0F3F5),0);lv_obj_t *label=lv_obj_get_child(key,0);const lv_font_t *font=ui_message_font(i<3?&lv_font_instrument_sans_medium_17:&lv_font_instrument_sans_medium_26);lv_obj_set_style_text_font(label,ui_message_font(font),0);lv_obj_set_height(label,font->line_height);lv_obj_center(label);}
    }else if(calc.mode==MODE_FX){lv_obj_t *left=cw_box(b,0,0,584,304,0xFFFFFF,17),*right=cw_box(b,602,0,630,304,CW_PANEL,17);cw_label(left,20,28,300,ui_tr("Original amount"),&lv_font_instrument_sans_semibold_18,CW_INK);button(v,left,424,20,140,42,ui_tr("Import count"),A_IMPORT,2);lv_obj_t *input=button(v,left,20,82,466,65,calc.fx_amount[0]?calc.fx_amount:"0.00",A_AMOUNT,0);lv_obj_set_style_text_font(lv_obj_get_child(input,0),&lv_font_instrument_sans_semibold_37,0);lv_obj_set_height(lv_obj_get_child(input,0),lv_font_instrument_sans_semibold_37.line_height);lv_obj_center(lv_obj_get_child(input,0));lv_obj_set_style_text_align(lv_obj_get_child(input,0),LV_TEXT_ALIGN_LEFT,0);button(v,left,498,82,66,65,calc.code,A_CURRENCY,0);
        if(calc.source.record)snprintf(s,sizeof(s),"#%u · %s",calc.source.record,operator_text(calc.source.operator_id,calc.source.operator_name));else snprintf(s,sizeof(s),"%s",ui_tr("Manual input. Machine currency stays unchanged."));hint(left,20,158,544,s);snprintf(s,sizeof(s),"1 %s = ? %s",calc.code,calc.to);hint(left,20,185,544,s);button(v,left,20,205,420,44,calc.rate[0]?calc.rate:ui_tr("Enter rate"),A_RATE,0);button(v,left,450,205,114,44,ui_tr("Swap"),A_SWAP,2);hint(left,20,263,544,ui_tr("Manual rate · not a live market quote"));
        cw_label(right,20,28,360,ui_tr("Converted amount"),&lv_font_instrument_sans_semibold_18,CW_INK);button(v,right,510,20,100,42,calc.to[0]?calc.to:ui_tr("Currency"),A_TO_CURRENCY,0);int64_t a,r,n;if(calculator_parse(calc.fx_amount,&a)&&calculator_parse(calc.rate,&r)&&strcmp(calc.code,calc.to)&&calculator_convert(a,r,calc.to,&n)==CALCULATOR_OK)amount(right,20,99,590,n,calc.to,&lv_font_instrument_sans_semibold_48);else cw_label(right,20,111,590,ui_tr("Enter rate"),&lv_font_instrument_sans_medium_24,CW_MUTED);hint(right,20,174,590,ui_tr("Reference only. Confirm the actual cash exchanged."));hint(right,20,197,590,ui_tr("Rounded to the target currency precision."));
    }
}
static void draw(void *context)
{
    calculator_view_t *v=context;v->queued=false;if(!v->root)return;close_overlay(v);lv_obj_clean(v->body);header(v);draw_calculator(v);lv_label_set_text(v->status_line,ui_tr(v->feedback));
}
static void tick(lv_timer_t *timer)
{
    calculator_view_t *v=timer->user_data;if(!v->root||!lv_obj_is_visible(v->root))return;
    unsigned revision=workspace_store_revision();if(v->workspace_revision!=revision){v->workspace_revision=revision;queue(v);}
}
static void deleted(lv_event_t *e)
{calculator_view_t *v=lv_event_get_user_data(e);if(lv_event_get_target(e)!=v->root)return;lv_async_call_cancel(draw,v);if(v->timer)lv_timer_del(v->timer);v->timer=NULL;v->root=v->body=v->header=v->overlay=v->status_line=NULL;v->queued=false;}
static void create(calculator_view_t *v,lv_obj_t *parent)
{
    if(v->root)return;uint32_t id=actor_id();if(id!=last_actor){memset(&calc,0,sizeof(calc));strcpy(calc.text,"0");last_actor=id;}
    v->root=cw_root(parent);v->header=cw_box(v->root,0,0,1280,72,0xFFFFFF,0);lv_obj_set_style_bg_opa(v->header,LV_OPA_TRANSP,0);v->body=cw_box(v->root,24,76,1232,304,0xFFFFFF,0);lv_obj_set_style_bg_opa(v->body,LV_OPA_TRANSP,0);lv_obj_add_event_cb(v->root,deleted,LV_EVENT_DELETE,v);v->status_line=cw_label(v->root,24,382,1232,"",&lv_font_instrument_sans_medium_12,0xAA3845);v->workspace_revision=workspace_store_revision();v->timer=lv_timer_create(tick,200,v);draw(v);
}
static void destroy(calculator_view_t *v)
{if(!v->root)return;settings_detail_keyboard_hide();settings_detail_dialog_hide();close_overlay(v);lv_async_call_cancel(draw,v);if(v->timer)lv_timer_del(v->timer);v->timer=NULL;lv_obj_del(v->root);}
void ui_calculator_create(lv_obj_t *p){create(&calculator,p);}void ui_calculator_destroy(void){destroy(&calculator);}
