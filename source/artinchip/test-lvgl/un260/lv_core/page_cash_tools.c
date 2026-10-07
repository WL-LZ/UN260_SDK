#include "page_cash_tools.h"
#include "lv_page_manager.h"
#include "page_19_history.h"
#include "cash_workspace_style.h"
#include "un260/app_service/cash_tools_service.h"
#include "un260/app_service/cash_workspace_access.h"
#include "un260/storage/cash_journal_store.h"
#include "un260/storage/cash_journal_export.h"
#include "un260/storage/workspace_store.h"
#include "un260/lv_components/lv_pin_keypad.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_system/machine_time.h"
#include "un260/lv_components/lv_recycled_list.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

enum { MODE_ORDINARY,MODE_FX };
enum { A_BACK=1000,A_ORDINARY,A_FX,A_CURRENCY,A_TO_CURRENCY,A_IMPORT,A_RECORD,A_OPERATORS,A_CLOSE,A_AMOUNT,A_RATE,A_SWAP,A_EXCHANGE,A_RECEIVED,A_NOTE,A_CONFIRM,A_CANCEL,A_RECEIPT,A_PAYMENT,A_OPENING,A_RETRY,A_NEXT,A_PREV,A_REVERSE,A_CORRECT,A_ALL_PEOPLE,A_PEOPLE,A_QUERY,A_ENTRIES,A_AUDIT,A_AUDIT_AMOUNT,A_AUDIT_SAVE,A_AUDIT_HISTORY,A_EXPORT,A_DATE,A_DATE_CONFIRM,A_SOURCE,A_LIST_MODE,
       A_CURRENCY_ITEM=2000,A_IMPORT_ITEM=2100,A_DETAIL=3000,A_PERSON=5200 };
typedef struct {lv_obj_t *root,*body,*header,*overlay,*status_line;lv_timer_t *timer;lv_pin_keypad_t pin;bool ledger,queued,auxiliary_overlay;unsigned revision,date_revision,workspace_revision;cash_store_status_t status;char feedback[180];} cash_view_t;
static cash_view_t calculator,journal;
static lv_recycled_list_t *entry_list;
static uint16_t entry_indices[CASH_ENTRIES];
static unsigned entry_count;
static lv_obj_t *entry_range;
static uint32_t entry_pressed,entry_revision;
static lv_point_t entry_press;
static bool entry_tapping;
static struct {char text[24],code[4],to[4],rate[24],fx_amount[24],equation[96];int64_t left;char op;bool fresh;unsigned mode;cash_snapshot_t source;} calc={.text="0"};
static struct {char code[4],query[41];cash_entry_t entry,resume;bool active,saving,has_resume,audit,audit_history;uint32_t correction,detail,filter,owner,before_revision;unsigned offset;bool paged;int64_t actual;bool actual_set;cash_snapshot_t audit_source;} book;
static char currency_choices[64][4];static unsigned choice_count,choice_page;static bool currency_target;
static unsigned import_mode;static uint32_t last_actor=UINT32_MAX;static void (*authorized_action)(void);
static bool match(const cash_entry_t *);
static void draw(void *);static void action(lv_event_t *);static void start_entry(unsigned,const cash_snapshot_t *);
static const char *kind_text(unsigned k){switch(k){case CASH_OPENING:return UI_N_("Opening balance");case CASH_RECEIPT:return UI_N_("Receipt");case CASH_PAYMENT:return UI_N_("Payment");case CASH_EXCHANGE:return UI_N_("Exchange");case CASH_AUDIT:return UI_N_("Cash reconciliation");default:return UI_N_("Reversal");}}
static uint32_t actor_id(void){const workspace_user_t *u=workspace_active(workspace_store_get());return u?u->id:0;}
static const char *operator_text(uint32_t id,const char *name)
{const char *current=workspace_record_name(workspace_store_get(),id,name);return !id&&!strcmp(current,"Device workspace")?ui_tr("Device workspace"):current;}
static void queue(cash_view_t *v){if(v->root&&!v->queued){v->queued=true;lv_async_call(draw,v);}}
static lv_obj_t *button(cash_view_t *v,lv_obj_t *p,int x,int y,int w,int h,const char *s,unsigned id,unsigned style)
{lv_obj_t *o=cw_button(p,x,y,w,h,s,style==1?CW_BLUE:style==2?CW_SOFT:CW_PANEL,style==1?0xFFFFFF:style==2?CW_BLUE:CW_INK,action,(void *)(uintptr_t)id);lv_obj_set_user_data(o,v);if(id==A_CANCEL||id==A_CLOSE)lv_nav_button_mark_back(o);return o;}
static void hint(lv_obj_t *p,int x,int y,int w,const char *s){cw_label(p,x,y,w,s,&lv_font_instrument_sans_medium_12,CW_MUTED);}
static void money(int64_t n,const char *code,char *out,size_t size){char raw[40];int d=cash_currency_decimals(code);cash_format(n,d<0?6:(unsigned)d,raw,sizeof(raw));snprintf(out,size,"%s %s",raw,code);}
static void amount(lv_obj_t *p,int x,int y,int w,int64_t n,const char *code,const lv_font_t *font)
{
    char s[48];int d=cash_currency_decimals(code);cash_format(n,d<0?6:(unsigned)d,s,sizeof(s));
    lv_point_t extent;lv_txt_get_size(&extent,s,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    const lv_font_t *number_font=font;
    if(extent.x>w-48){number_font=&lv_font_instrument_sans_medium_24;lv_txt_get_size(&extent,s,number_font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);}
    cw_label(p,x,y,w-48,s,number_font,CW_INK);
    int unit_x=extent.x+8;if(unit_x>w-40)unit_x=w-40;
    cw_label(p,x+unit_x,y+number_font->line_height-18,40,code,&lv_font_instrument_sans_medium_15,CW_MUTED);
}
static void trim(char *s){if(strchr(s,'.')){char *p=s+strlen(s)-1;while(p>s&&*p=='0')*p--=0;if(*p=='.')*p=0;}}
static void default_code(char code[4]){if(!code[0]){char list[64][4];if(cash_tools_currencies(list,64))memcpy(code,list[0],4);}}
static void explain(cash_view_t *v,const char *s){snprintf(v->feedback,sizeof(v->feedback),"%s",s);ui_notice_post_text(UI_NOTICE_WARNING,"cash.tools",UI_N_("Cash tools"),s);queue(v);}
static const char *error_text(cash_error_t e){switch(e){case CASH_PRECISION:return UI_N_("Check currency decimal places.");case CASH_OVERFLOW:return UI_N_("The amount is too large.");case CASH_DIV_ZERO:return UI_N_("Cannot divide by zero.");case CASH_NEEDS_OPENING:return UI_N_("Set the opening balance for both currencies first.");case CASH_DUPLICATE:return UI_N_("This count is already recorded. Review its existing entry.");case CASH_BALANCE:return UI_N_("Insufficient book balance. Check the opening balance and receipts.");case CASH_FULL:return UI_N_("The journal is full. Existing entries are retained.");case CASH_REVERSED:return UI_N_("This entry has already been reversed.");default:return UI_N_("Check the amount, date and storage status, then try again.");}}
static void close_overlay(cash_view_t *v){lv_pin_keypad_destroy(&v->pin);if(v->overlay)lv_obj_del(v->overlay);v->overlay=NULL;v->auxiliary_overlay=false;authorized_action=NULL;cash_access_cancel();}
static lv_obj_t *overlay(cash_view_t *v,const char *title)
{
    close_overlay(v);v->overlay=cw_box(v->root,0,0,1280,400,0x1B2732,0);lv_obj_set_style_bg_opa(v->overlay,56,0);lv_obj_add_flag(v->overlay,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_user_data(v->overlay,v);lv_obj_add_event_cb(v->overlay,action,LV_EVENT_CLICKED,(void *)A_CLOSE);
    lv_obj_t *panel=cw_box(v->overlay,330,16,620,368,0xFFFFFF,20);lv_obj_add_flag(panel,LV_OBJ_FLAG_CLICKABLE);cw_label(panel,26,22,470,title,&lv_font_instrument_sans_semibold_20,CW_INK);button(v,panel,490,12,104,42,ui_tr("Cancel"),A_CLOSE,0);return panel;
}

static void pin_cancel(void *context){close_overlay(context);}
static void pin_confirm(const char *pin,void *context)
{cash_view_t *v=context;if(!cash_access_verify(CASH_ACCESS_ENTRY,pin)){lv_pin_keypad_clear(&v->pin);char message[96];uint32_t delay=cash_access_retry_delay_ms();if(delay)snprintf(message,sizeof(message),ui_tr("Try again in %u seconds."),(unsigned)((delay+999)/1000));else snprintf(message,sizeof(message),"%s",ui_tr("Incorrect password. Try again."));lv_pin_keypad_set_error(&v->pin,message);return;}void (*fn)(void)=authorized_action;authorized_action=NULL;lv_pin_keypad_hide(&v->pin);if(fn)fn();}
static void authorize(cash_view_t *v,void (*fn)(void))
{if(!cash_access_required(CASH_ACCESS_ENTRY)){fn();return;}close_overlay(v);authorized_action=fn;lv_pin_keypad_create(&v->pin,v->root,215,18);lv_pin_keypad_config_t c={.title=ui_tr("Management authorization"),.prompt=ui_tr("Authorize this action without changing operator."),.confirm_cb=pin_confirm,.cancel_cb=pin_cancel,.user_data=v,.compact=true,.auto_confirm=true};lv_pin_keypad_show(&v->pin,&c,"");}
static void currency_picker(cash_view_t *v,bool target)
{currency_target=target;choice_count=cash_tools_currencies(currency_choices,64);lv_obj_t *o=overlay(v,ui_tr("Select currency"));v->auxiliary_overlay=true;for(unsigned i=0;i<12&&choice_page*12+i<choice_count;++i){unsigned n=choice_page*12+i;button(v,o,26+(i%4)*142,86+(i/4)*65,132,52,currency_choices[n],A_CURRENCY_ITEM+n,0);}button(v,o,354,304,112,42,"<",A_PREV,0);button(v,o,480,304,112,42,">",A_NEXT,0);}
static void apply_import(cash_view_t *v,unsigned index)
{
    v->feedback[0]=0;cash_snapshot_t s;if(!cash_tools_current(index,&s)){explain(v,UI_N_("The current result is no longer available."));return;}close_overlay(v);
    if(v->ledger){if(book.audit){if(strcmp(book.code,s.code)){explain(v,UI_N_("Choose the same currency as the cashbox reconciliation."));return;}book.actual=s.amount;book.actual_set=true;book.audit_source=s;}else {unsigned kind=book.active&&book.entry.kind==CASH_PAYMENT?CASH_PAYMENT:CASH_RECEIPT;char note[sizeof(book.entry.note)]={0};if(book.active)snprintf(note,sizeof(note),"%s",book.entry.note);start_entry(kind,&s);snprintf(book.has_resume?book.resume.note:book.entry.note,sizeof(book.entry.note),"%s",note);}}
    else{if(import_mode==MODE_FX&&strcmp(calc.code,s.code))calc.rate[0]=0;memcpy(calc.code,s.code,4);calc.source=s;cash_format(s.amount,cash_currency_decimals(s.code),import_mode==MODE_FX?calc.fx_amount:calc.text,24);calc.op=0;calc.equation[0]=0;calc.fresh=true;if(!strcmp(calc.to,s.code)){calc.to[0]=0;calc.rate[0]=0;}}
    queue(v);
}
static void import_picker(cash_view_t *v)
{const char *reason=cash_tools_current_blocker();if(reason){explain(v,reason);return;}unsigned n=cash_tools_current_count();if(!n){explain(v,UI_N_("No complete saved monetary count is available."));return;}import_mode=calc.mode;if(n==1){apply_import(v,0);return;}lv_obj_t *o=overlay(v,ui_tr("Choose a currency from this count"));v->auxiliary_overlay=true;hint(o,26,64,560,ui_tr("MULTI currencies are imported separately."));for(unsigned i=0;i<4&&choice_page*4+i<n;++i){cash_snapshot_t s;if(!cash_tools_current(choice_page*4+i,&s))continue;char text[100],a[64];money(s.amount,s.code,a,sizeof(a));snprintf(text,sizeof(text),"#%u    %s",s.record,a);button(v,o,26,96+i*48,568,42,text,A_IMPORT_ITEM+i,0);}button(v,o,354,306,112,42,"<",A_PREV,0);button(v,o,480,306,112,42,">",A_NEXT,0);lv_obj_add_flag(v->overlay,LV_OBJ_FLAG_USER_1);}
static void keyboard_value(const char *text,void *context)
{
    unsigned id=(uintptr_t)context;cash_view_t *v=(id==A_NOTE||id==A_RECEIVED||id==A_QUERY||id==A_AUDIT_AMOUNT||(id==A_AMOUNT&&book.active))?&journal:&calculator;
    if(!v->root)return;v->feedback[0]=0;int64_t n;
    if(id==A_NOTE){snprintf(book.entry.note,sizeof(book.entry.note),"%s",text);queue(v);return;}
    if(id==A_QUERY){snprintf(book.query,sizeof(book.query),"%s",text);book.offset=0;queue(v);return;}
    if(!cash_parse(text,&n)||n<0){explain(v,error_text(CASH_INVALID));return;}
    if(id==A_RATE){if(!n||n>999999999999LL){explain(v,UI_N_("Enter a positive rate with up to six decimals."));return;}snprintf(calc.rate,sizeof(calc.rate),"%s",text);}
    else if(id==A_AMOUNT&&v==&calculator){snprintf(calc.fx_amount,sizeof(calc.fx_amount),"%s",text);calc.source.count=0;}
    else if(id==A_AMOUNT){if(book.entry.cash.record&&!book.correction)return;if(!cash_amount_precision(book.entry.cash.code,n)){explain(v,error_text(CASH_PRECISION));return;}book.entry.cash.amount=n;book.entry.cash.count=0;}
    else if(id==A_RECEIVED){if(!cash_amount_precision(book.entry.peer.code,n)){explain(v,error_text(CASH_PRECISION));return;}book.entry.peer.amount=n;}
    else if(id==A_AUDIT_AMOUNT){if(!cash_amount_precision(book.code,n)){explain(v,error_text(CASH_PRECISION));return;}book.actual=n;book.actual_set=true;memset(&book.audit_source,0,sizeof(book.audit_source));}
    queue(v);
}
static void keyboard(unsigned id,const char *title,const char *value,bool text)
{settings_detail_keyboard_show(ui_tr(title),value,text?40:22,text?SETTINGS_DETAIL_KEYBOARD_TEXT:SETTINGS_DETAIL_KEYBOARD_DECIMAL,keyboard_value,(void *)(uintptr_t)id);}
static void start_entry(unsigned kind,const cash_snapshot_t *s)
{
    if(book.saving)return;journal.feedback[0]=0;memset(&book.entry,0,sizeof(book.entry));book.entry.kind=kind;book.correction=0;book.detail=0;book.active=true;book.owner=actor_id();
    if(s){book.entry.cash=*s;memcpy(book.code,s->code,4);}else memcpy(book.entry.cash.code,book.code,4);
    const cash_journal_t *j=cash_journal_store_get();
    if(kind!=CASH_OPENING&&j&&!cash_journal_opened(j,book.code)){book.resume=book.entry;book.has_resume=true;memset(&book.entry,0,sizeof(book.entry));book.entry.kind=CASH_OPENING;memcpy(book.entry.cash.code,book.code,4);}
    cash_tools_entry_identity(&book.entry);queue(&journal);
}
static void confirm_post(void)
{if(!book.active||book.saving)return;if(book.owner!=actor_id()){explain(&journal,UI_N_("The operator changed. Start a new entry."));return;}cash_error_t e=cash_workspace_submit(&book.entry,book.correction);if(e){explain(&journal,error_text(e));return;}book.saving=true;book.before_revision=cash_journal_store_revision();close_overlay(&journal);queue(&journal);}
static void submit(void)
{const char *reason=cash_tools_write_blocker();if(reason){explain(&journal,reason);return;}cash_tools_entry_identity(&book.entry);if(!book.entry.time){explain(&journal,UI_N_("Set the device date before recording cash."));return;}if((book.correction||book.entry.kind==CASH_REVERSAL)&&!book.entry.note[0]){explain(&journal,UI_N_("Enter a reason for this correction or reversal."));return;}if(book.correction||book.entry.kind==CASH_OPENING||book.entry.kind==CASH_REVERSAL)authorize(&journal,confirm_post);else confirm_post();}
static void record_calculation(bool fx)
{cash_snapshot_t s=calc.source;memcpy(s.code,calc.code,4);if(!cash_parse(fx?calc.fx_amount:calc.text,&s.amount)||s.amount<=0||!cash_amount_precision(s.code,s.amount)){explain(&calculator,error_text(CASH_PRECISION));return;}if(s.amount!=s.original_amount)s.count=0;const cash_journal_t *j=cash_journal_store_get();if(cash_journal_source_used(j,&s)){explain(&calculator,error_text(CASH_DUPLICATE));return;}start_entry(CASH_RECEIPT,&s);ui_manager_push_page(UI_PAGE_CASH_JOURNAL);}
static void exchange(void)
{int64_t a,rate,result;if(!cash_parse(calc.fx_amount,&a)||!cash_amount_precision(calc.code,a)||!cash_parse(calc.rate,&rate)||!strcmp(calc.code,calc.to)||cash_convert(a,rate,calc.to,&result)){explain(&calculator,UI_N_("Check the two currencies, amount and manual rate."));return;}const cash_journal_t *j=cash_journal_store_get();if(!cash_journal_opened(j,calc.code)||!cash_journal_opened(j,calc.to)){explain(&calculator,error_text(CASH_NEEDS_OPENING));return;}cash_snapshot_t s=calc.source;memcpy(s.code,calc.code,4);s.amount=a;if(s.amount!=s.original_amount)s.count=0;start_entry(CASH_EXCHANGE,&s);memcpy(book.entry.peer.code,calc.to,4);book.entry.peer.amount=result;book.entry.rate=rate;ui_manager_push_page(UI_PAGE_CASH_JOURNAL);}

static void key(unsigned key)
{
    calculator.feedback[0]=0;if(key=='C')memset(&calc.source,0,sizeof(calc.source));else calc.source.count=0;
    if(key=='C'){snprintf(calc.text,sizeof(calc.text),"0");calc.op=0;calc.fresh=false;calc.left=0;calc.equation[0]=0;}
    else if(key=='B'){size_t n=strlen(calc.text);if(n>1)calc.text[n-1]=0;else strcpy(calc.text,"0");if(!strcmp(calc.text,"-"))strcpy(calc.text,"0");calc.fresh=false;}
    else if(key=='S'){int64_t n;if(cash_parse(calc.text,&n)){cash_format(-n,6,calc.text,sizeof(calc.text));trim(calc.text);}}
    else if(strchr("+-*/=",(char)key)){
        int64_t n;if(!cash_parse(calc.text,&n)){explain(&calculator,error_text(CASH_INVALID));return;}
        if(calc.op&&!calc.fresh){char lhs[40];cash_format(calc.left,6,lhs,sizeof(lhs));trim(lhs);snprintf(calc.equation,sizeof(calc.equation),"%s %c %s =",lhs,calc.op,calc.text);cash_error_t e=cash_calculate(calc.left,calc.op,n,&n);if(e){explain(&calculator,error_text(e));return;}cash_format(n,6,calc.text,sizeof(calc.text));trim(calc.text);}
        calc.left=n;calc.op=key=='='?0:(char)key;calc.fresh=true;
    }else{
        if(!calc.op)calc.equation[0]=0;char text[24];snprintf(text,sizeof(text),"%s",calc.fresh?"0":calc.text);if(key=='.'&&strchr(text,'.'))return;
        size_t len=strlen(text);if(len>=sizeof(text)-2)return;if(!strcmp(text,"0")&&key!='.')len=0;text[len]=(char)key;text[len+1]=0;
        int64_t n;if(!cash_parse(text,&n))return;strcpy(calc.text,text);calc.fresh=false;
    }queue(&calculator);
}
static void actor_picker(cash_view_t *v)
{lv_obj_t *o=overlay(v,ui_tr("Filter cash entries by operator"));v->auxiliary_overlay=true;button(v,o,26,70,568,42,ui_tr("All operators"),A_ALL_PEOPLE,2);const workspace_model_t *m=workspace_store_get();if(m)for(unsigned i=0;i<m->user_count;++i)button(v,o,26+(i%2)*290,124+(i/2)*49,278,42,m->users[i].name,A_PERSON+i,0);}
static void action(lv_event_t *event)
{
    if(lv_event_get_code(event)!=LV_EVENT_CLICKED)return;cash_view_t *v=lv_obj_get_user_data(lv_event_get_current_target(event));unsigned id=(uintptr_t)lv_event_get_user_data(event);if(!v||!v->root)return;
    if(id<128){key(id);return;}
    if(id==A_CLOSE){bool auxiliary=v->auxiliary_overlay;close_overlay(v);if(v->ledger&&!auxiliary){book.active=book.has_resume=false;book.detail=0;}queue(v);return;}
    if(id==A_BACK){settings_detail_keyboard_hide();close_overlay(v);if(!ui_manager_pop_page())ui_manager_switch(UI_PAGE_MAIN);return;}
    if(id==A_OPERATORS){if(book.saving){explain(v,UI_N_("Wait for the cash save to finish."));return;}close_overlay(v);ui_manager_push_page(UI_PAGE_OPERATORS);return;}
    if(id==A_ORDINARY||id==A_FX){if(id==A_FX&&calc.mode!=MODE_FX){snprintf(calc.fx_amount,sizeof(calc.fx_amount),"%s",calc.text);calc.rate[0]=0;}calc.mode=id==A_ORDINARY?MODE_ORDINARY:MODE_FX;queue(v);return;}
    if(id==A_CURRENCY||id==A_TO_CURRENCY){choice_page=0;currency_picker(v,id==A_TO_CURRENCY);return;}
    if(id>=A_CURRENCY_ITEM&&id<A_CURRENCY_ITEM+64){unsigned i=id-A_CURRENCY_ITEM;if(i>=choice_count)return;if(v->ledger){if(book.active)return;memcpy(book.code,currency_choices[i],4);book.actual_set=false;book.offset=0;}else{memcpy(currency_target?calc.to:calc.code,currency_choices[i],4);calc.rate[0]=0;if(!currency_target){strcpy(calc.text,"0");calc.fx_amount[0]=0;calc.op=0;memset(&calc.source,0,sizeof(calc.source));calc.equation[0]=0;}}close_overlay(v);queue(v);return;}
    if(id==A_IMPORT){choice_page=0;import_picker(v);return;}
    if(id>=A_IMPORT_ITEM&&id<A_IMPORT_ITEM+4){apply_import(v,choice_page*4+id-A_IMPORT_ITEM);return;}
    if((id==A_NEXT||id==A_PREV)&&v->overlay){bool imports=lv_obj_has_flag(v->overlay,LV_OBJ_FLAG_USER_1);unsigned n=imports?cash_tools_current_count():choice_count,per=imports?4:12;if(id==A_PREV&&choice_page)--choice_page;if(id==A_NEXT&&(choice_page+1)*per<n)++choice_page;if(imports)import_picker(v);else currency_picker(v,currency_target);return;}
    if(id==A_RECORD){record_calculation(calc.mode==MODE_FX);return;}
    if(id==A_RATE){keyboard(id,UI_N_("Manual exchange rate"),calc.rate,false);return;}
    if(id==A_SWAP){char c[4];memcpy(c,calc.code,4);memcpy(calc.code,calc.to,4);memcpy(calc.to,c,4);calc.fx_amount[0]=calc.rate[0]=0;memset(&calc.source,0,sizeof(calc.source));queue(v);return;}
    if(id==A_EXCHANGE){exchange();return;}
    if(id==A_AMOUNT){char s[32];if(v->ledger){if(book.entry.kind==CASH_REVERSAL||(book.entry.cash.record&&!book.correction)){explain(v,UI_N_("Imported amounts keep their source."));return;}cash_format(book.entry.cash.amount,cash_currency_decimals(book.entry.cash.code),s,sizeof(s));}else snprintf(s,sizeof(s),"%s",calc.fx_amount);keyboard(id,UI_N_("Amount"),s,false);return;}
    if(id==A_RETRY){cash_journal_store_retry();queue(v);return;}
    if(book.saving)return;
    if(id==A_ENTRIES||id==A_AUDIT){book.audit=id==A_AUDIT;book.audit_history=false;book.active=false;book.detail=0;queue(v);return;}
    if(id==A_DATE){ui_manager_push_page(UI_PAGE_TIMESET);return;}
    if(id==A_DATE_CONFIRM){machine_time_value_t t;machine_time_get(&t);if(!machine_time_is_valid(&t)||t.year<2020||t.year>2099){explain(v,UI_N_("Set a valid device date first."));return;}machine_time_confirm(&t);v->feedback[0]=0;queue(v);return;}
    if(id==A_QUERY){keyboard(id,UI_N_("Search cash entries"),book.query,true);return;}
    if(id==A_ALL_PEOPLE){book.filter=0;book.offset=0;close_overlay(v);queue(v);return;}
    if(id==A_PEOPLE){actor_picker(v);return;}
    if(id>=A_PERSON&&id<A_PERSON+WORKSPACE_USERS){const workspace_model_t *m=workspace_store_get();if(id-A_PERSON<m->user_count)book.filter=m->users[id-A_PERSON].id;close_overlay(v);queue(v);return;}
    if(id==A_NOTE){keyboard(id,UI_N_("Note / reason"),book.entry.note,true);return;}
    if(id==A_RECEIVED){if(book.entry.kind!=CASH_EXCHANGE)return;char s[32];cash_format(book.entry.peer.amount,cash_currency_decimals(book.entry.peer.code),s,sizeof(s));keyboard(id,UI_N_("Actual amount received"),s,false);return;}
    if(id==A_AUDIT_AMOUNT){char s[32];cash_format(book.actual,cash_currency_decimals(book.code),s,sizeof(s));keyboard(id,UI_N_("Count the entire cashbox"),s,false);return;}
    if(id==A_AUDIT_SAVE){if(!cash_journal_opened(cash_journal_store_get(),book.code)){explain(v,error_text(CASH_NEEDS_OPENING));return;}if(!book.actual_set){explain(v,UI_N_("Enter the actual cashbox amount first."));return;}cash_snapshot_t source=book.audit_source;memcpy(source.code,book.code,4);source.amount=book.actual;start_entry(CASH_AUDIT,&source);book.entry.cash.amount=book.actual;book.entry.cash.count=0;memcpy(book.entry.peer.code,book.code,4);book.entry.peer.amount=cash_journal_balance(cash_journal_store_get(),book.code);queue(v);return;}
    if(id==A_AUDIT_HISTORY){book.audit_history=true;book.audit=false;book.offset=0;queue(v);return;}
    if(id==A_RECEIPT||id==A_PAYMENT||id==A_OPENING){if(book.active&&id!=A_OPENING&&book.entry.kind!=CASH_EXCHANGE){if(book.correction)return;book.entry.kind=id==A_RECEIPT?CASH_RECEIPT:CASH_PAYMENT;queue(v);}else start_entry(id==A_OPENING?CASH_OPENING:id==A_PAYMENT?CASH_PAYMENT:CASH_RECEIPT,NULL);return;}
    if(id==A_CANCEL){book.active=false;book.detail=0;book.has_resume=false;close_overlay(v);queue(v);return;}
    if(id==A_CONFIRM){submit();return;}
    if(id==A_LIST_MODE){book.paged=!book.paged;if(entry_list)lv_recycled_list_set_paged(entry_list,book.paged);queue(v);return;}
    if(id==A_NEXT||id==A_PREV){if(entry_list)lv_recycled_list_page_step(entry_list,id==A_NEXT?1:-1);return;}
    const cash_journal_t *j=cash_journal_store_get();if(!j)return;
    if(id==A_SOURCE&&book.detail&&book.detail<=j->count){uint32_t source=j->entries[book.detail-1].cash.record;if(source)ui_page_19_history_open_record(source);return;}
    if(id==A_EXPORT){uint32_t ids[CASH_ENTRIES];unsigned count=0;for(unsigned i=0;i<j->count;++i)if(match(&j->entries[i]))ids[count++]=j->entries[i].id;if(!count){explain(v,UI_N_("No entries match this view."));return;}if(!cash_journal_export_start(ids,count))explain(v,UI_N_("Export could not start. Wait for storage and retry."));else ui_notice_post_text(UI_NOTICE_INFO,"cash.export",UI_N_("Cash journal"),UI_N_("Exporting this view to USB..."));return;}
    if(id>=A_DETAIL&&id<A_DETAIL+CASH_ENTRIES){book.detail=id-A_DETAIL+1;queue(v);return;}
    if((id==A_REVERSE||id==A_CORRECT)&&book.detail&&book.detail<=j->count){const cash_entry_t *e=&j->entries[book.detail-1];unsigned allowed=cash_journal_entry_actions(j,e->id);if(!(allowed&(id==A_REVERSE?CASH_ACTION_VOID:CASH_ACTION_CORRECT)))return;book.entry=*e;book.entry.approved=0;book.entry.id=0;book.owner=actor_id();book.correction=id==A_CORRECT?e->id:0;book.entry.note[0]=0;book.entry.cash.count=id==A_CORRECT?0:e->cash.count;book.entry.corrects=0;if(id==A_REVERSE){book.entry.kind=CASH_REVERSAL;book.entry.reverse_of=e->id;snprintf(book.entry.note,sizeof(book.entry.note),"%s",ui_tr("Entry cancelled"));}book.detail=0;book.active=true;cash_tools_entry_identity(&book.entry);queue(v);return;}
}
static void header(cash_view_t *v)
{
    lv_obj_clean(v->header);lv_obj_t *back=lv_nav_back_text_button_create(v->header,24,15,ui_message_font(&lv_font_instrument_sans_medium_16),action,(void *)A_BACK);lv_obj_set_user_data(back,v);cw_label(v->header,114,23,260,ui_tr(v->ledger?UI_N_("Cash journal"):UI_N_("Calculator")),&lv_font_instrument_sans_semibold_24,CW_INK);
    lv_obj_t *seg=cw_box(v->header,486,15,288,42,0xE9EEF3,13);
    const char *titles[2]={v->ledger?UI_N_("Cash entries"):UI_N_("Ordinary"),v->ledger?UI_N_("Reconcile"):UI_N_("Exchange rate")};
    for(unsigned i=0;i<2;++i){bool selected=v->ledger?book.audit==(bool)i:(calc.mode==MODE_FX)==(bool)i;lv_obj_t *b=button(v,seg,4+i*140,4,140,34,ui_tr(titles[i]),v->ledger?(i?A_AUDIT:A_ENTRIES):(i?A_FX:A_ORDINARY),0);lv_obj_set_style_bg_color(b,lv_color_hex(selected?0xFFFFFF:0xE9EEF3),0);lv_obj_set_style_text_color(lv_obj_get_child(b,0),lv_color_hex(selected?CW_BLUE:CW_MUTED),0);lv_obj_set_style_radius(b,9,0);}
    const workspace_user_t *u=workspace_active(workspace_store_get());lv_obj_t *person=button(v,v->header,1076,15,180,42,u&&u->id?u->name:ui_tr("Device workspace"),A_OPERATORS,0);lv_obj_set_style_bg_color(person,lv_color_hex(0xF0EDF9),0);lv_obj_set_style_text_color(lv_obj_get_child(person,0),lv_color_hex(0x7358AD),0);
}
static void draw_calculator(cash_view_t *v)
{
    default_code(calc.code);if(!calc.to[0]){char list[64][4];unsigned n=cash_tools_currencies(list,64);for(unsigned i=0;i<n;++i)if(strcmp(list[i],calc.code)){memcpy(calc.to,list[i],4);break;}}char s[120];lv_obj_t *b=v->body;
    if(calc.mode==MODE_ORDINARY){
        lv_obj_t *left=cw_box(b,0,0,492,304,0xFFFFFF,17);button(v,left,399,16,73,42,calc.code,A_CURRENCY,0);
        if(calc.op){char lhs[40];cash_format(calc.left,6,lhs,sizeof(lhs));trim(lhs);snprintf(s,sizeof(s),"%s %c %s",lhs,calc.op,calc.fresh?"...":calc.text);}else snprintf(s,sizeof(s),"%s",calc.equation);
        lv_obj_t *formula=cw_label(left,20,72,452,s,&lv_font_instrument_sans_medium_20,CW_MUTED);lv_obj_set_style_text_align(formula,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_height(formula,30);lv_label_set_long_mode(formula,LV_LABEL_LONG_SCROLL_CIRCULAR);
        const lv_font_t *nf=strlen(calc.text)>12?&lv_font_instrument_sans_semibold_37:&lv_font_instrument_sans_semibold_52;
        lv_obj_t *n=cw_label(left,20,117,452,calc.text,nf,CW_INK);lv_obj_set_style_text_align(n,LV_TEXT_ALIGN_CENTER,0);
        if(calc.source.record){snprintf(s,sizeof(s),"#%u · %s",calc.source.record,calc.source.code);hint(left,20,190,452,s);}
        button(v,left,20,242,144,42,ui_tr("Import count"),A_IMPORT,2);button(v,left,174,242,144,42,ui_tr("Exchange rate"),A_FX,2);button(v,left,328,242,144,42,ui_tr("Record cash"),A_RECORD,1);
        const char *keys[]={UI_N_("Clear"),"+/-","<","/","1","2","3","*","4","5","6","-","7","8","9","+","0",".","="};const unsigned ids[]={'C','S','B','/','1','2','3','*','4','5','6','-','7','8','9','+','0','.','='};
        for(unsigned i=0;i<19;++i){int x=510+(i%4)*182,y=(i/4)*62;lv_obj_t *key=button(v,b,x,y,i==18?358:174,54,ui_tr(keys[i]),ids[i],i==18?1:(i%4==3?2:0));if(i!=18&&i%4!=3)lv_obj_set_style_bg_color(key,lv_color_hex(0xF0F3F5),0);lv_obj_t *label=lv_obj_get_child(key,0);const lv_font_t *font=ui_message_font(i<3?&lv_font_instrument_sans_medium_17:&lv_font_instrument_sans_medium_26);lv_obj_set_style_text_font(label,font,0);lv_obj_set_height(label,font->line_height);lv_obj_center(label);}
    }else if(calc.mode==MODE_FX){lv_obj_t *left=cw_box(b,0,0,584,304,0xFFFFFF,17),*right=cw_box(b,602,0,630,304,CW_PANEL,17);cw_label(left,20,28,300,ui_tr("Original amount"),&lv_font_instrument_sans_semibold_18,CW_INK);button(v,left,424,20,140,42,ui_tr("Import count"),A_IMPORT,2);lv_obj_t *input=button(v,left,20,82,466,65,calc.fx_amount[0]?calc.fx_amount:"0.00",A_AMOUNT,0);lv_obj_set_style_text_font(lv_obj_get_child(input,0),&lv_font_instrument_sans_semibold_37,0);lv_obj_set_height(lv_obj_get_child(input,0),lv_font_instrument_sans_semibold_37.line_height);lv_obj_center(lv_obj_get_child(input,0));lv_obj_set_style_text_align(lv_obj_get_child(input,0),LV_TEXT_ALIGN_LEFT,0);button(v,left,498,82,66,65,calc.code,A_CURRENCY,0);
        if(calc.source.record)snprintf(s,sizeof(s),"#%u · %s",calc.source.record,operator_text(calc.source.operator_id,calc.source.operator_name));else snprintf(s,sizeof(s),"%s",ui_tr("Manual input. Machine currency stays unchanged."));hint(left,20,158,544,s);snprintf(s,sizeof(s),"1 %s = ? %s",calc.code,calc.to);hint(left,20,185,544,s);button(v,left,20,205,420,44,calc.rate[0]?calc.rate:ui_tr("Enter rate"),A_RATE,0);button(v,left,450,205,114,44,ui_tr("Swap"),A_SWAP,2);hint(left,20,263,544,ui_tr("Manual rate · not a live market quote"));
        cw_label(right,20,28,360,ui_tr("Converted amount"),&lv_font_instrument_sans_semibold_18,CW_INK);button(v,right,510,20,100,42,calc.to[0]?calc.to:ui_tr("Currency"),A_TO_CURRENCY,0);int64_t a,r,n;if(cash_parse(calc.fx_amount,&a)&&cash_parse(calc.rate,&r)&&strcmp(calc.code,calc.to)&&cash_convert(a,r,calc.to,&n)==CASH_OK)amount(right,20,99,590,n,calc.to,&lv_font_instrument_sans_semibold_48);else cw_label(right,20,111,590,ui_tr("Enter rate"),&lv_font_instrument_sans_medium_24,CW_MUTED);hint(right,20,174,590,ui_tr("Reference only. Confirm the actual cash exchanged."));hint(right,20,197,590,ui_tr("Rounded to the target currency precision."));button(v,right,20,242,200,42,ui_tr("Record original cash"),A_RECORD,2);button(v,right,400,242,210,42,ui_tr("Actual exchange"),A_EXCHANGE,1);
    }
}
static void draw_form(cash_view_t *v)
{
    cash_entry_t *e=&book.entry;lv_obj_t *o=overlay(v,ui_tr(book.correction?UI_N_("Correct cash entry"):kind_text(e->kind)));char s[192];
    snprintf(s,sizeof(s),"%s · %s",e->cash.code,operator_text(e->operator_id,e->operator_name));if(v->feedback[0]){lv_obj_t *error=cw_label(o,26,65,568,ui_tr(v->feedback),&lv_font_instrument_sans_medium_12,0xAA3845);lv_obj_set_height(error,30);lv_label_set_long_mode(error,LV_LABEL_LONG_WRAP);}else hint(o,26,67,568,e->kind==CASH_OPENING?ui_tr("Enter the cash already in this cashbox, including zero."):s);
    const cash_journal_t *ledger=cash_journal_store_get();bool reversing_exchange=e->kind==CASH_REVERSAL&&ledger&&e->reverse_of&&e->reverse_of<=ledger->count&&ledger->entries[e->reverse_of-1].kind==CASH_EXCHANGE;bool exchange=e->kind==CASH_EXCHANGE||reversing_exchange;
    if(e->kind==CASH_RECEIPT||e->kind==CASH_PAYMENT){button(v,o,26,99,158,44,ui_tr("Receipt"),A_RECEIPT,e->kind==CASH_RECEIPT?2:0);button(v,o,194,99,158,44,ui_tr("Payment"),A_PAYMENT,e->kind==CASH_PAYMENT?2:0);}
    else hint(o,26,111,320,ui_tr(reversing_exchange?UI_N_("Cash returned"):exchange?UI_N_("Cash paid out"):kind_text(e->kind)));
    money(e->cash.amount,e->cash.code,s,sizeof(s));lv_obj_t *cash_amount=button(v,o,364,99,230,44,s,A_AMOUNT,0);if(e->kind==CASH_REVERSAL||(e->cash.record&&!book.correction))lv_obj_add_state(cash_amount,LV_STATE_DISABLED);
    if(exchange){hint(o,26,165,210,ui_tr(reversing_exchange?UI_N_("Cash taken back"):UI_N_("Actual amount received")));money(e->peer.amount,e->peer.code,s,sizeof(s));lv_obj_t *peer=button(v,o,270,153,324,44,s,A_RECEIVED,0);if(reversing_exchange)lv_obj_add_state(peer,LV_STATE_DISABLED);}
    else{const cash_journal_t *j=cash_journal_store_get();int64_t before=cash_journal_balance(j,e->cash.code),base=before;if(book.correction)base-=cash_entry_delta(j,&j->entries[book.correction-1],e->cash.code);int64_t delta=cash_entry_delta(j,e,e->cash.code);char a[40],d[40],result[40];cash_format(base,cash_currency_decimals(e->cash.code),a,sizeof(a));cash_format(delta<0?-delta:delta,cash_currency_decimals(e->cash.code),d,sizeof(d));cash_format(base+delta,cash_currency_decimals(e->cash.code),result,sizeof(result));hint(o,26,154,568,ui_tr(book.correction?UI_N_("Balance excluding the original entry"):UI_N_("Current balance / Change / New balance")));snprintf(s,sizeof(s),"%s %c %s = %s %s",a,delta<0?'-':'+',d,result,e->cash.code);lv_obj_t *equation=cw_label(o,26,177,568,s,&lv_font_instrument_sans_medium_20,CW_INK);lv_obj_set_height(equation,28);lv_label_set_long_mode(equation,LV_LABEL_LONG_SCROLL_CIRCULAR);}

    button(v,o,26,210,568,44,e->note[0]?e->note:ui_tr((book.correction||e->kind==CASH_REVERSAL)?UI_N_("Reason required"):UI_N_("Note (optional)")),A_NOTE,0);
    machine_time_value_t date;machine_time_get(&date);snprintf(s,sizeof(s),"%04u-%02u-%02u  %02u:%02u",date.year,date.month,date.day,date.hour,date.minute);
    button(v,o,26,260,278,32,s,A_DATE,0);button(v,o,316,260,278,32,ui_tr(machine_time_is_confirmed()?UI_N_("Date confirmed"):UI_N_("Use this date")),A_DATE_CONFIRM,machine_time_is_confirmed()?2:0);
    if((e->kind==CASH_RECEIPT||e->kind==CASH_PAYMENT)&&!book.correction)button(v,o,26,304,176,42,ui_tr("Import count"),A_IMPORT,2);
    button(v,o,336,304,112,42,ui_tr("Cancel"),A_CANCEL,0);button(v,o,460,304,134,42,ui_tr(e->kind==CASH_REVERSAL?UI_N_("Void entry"):UI_N_("Confirm")),A_CONFIRM,1);

}
static void draw_detail(cash_view_t *v,const cash_journal_t *j)
{
    const cash_entry_t *e=&j->entries[book.detail-1];char s[180],a[70];snprintf(s,sizeof(s),"%s #%u",ui_tr(kind_text(e->kind)),e->id);lv_obj_t *o=overlay(v,s);time_t stamp=(time_t)e->time;struct tm date;if(localtime_r(&stamp,&date)){strftime(s,sizeof(s),"%Y-%m-%d  %H:%M:%S",&date);hint(o,26,60,568,s);}amount(o,26,82,568,e->cash.amount,e->cash.code,&lv_font_instrument_sans_semibold_37);
    if(e->kind==CASH_EXCHANGE||cash_entry_is_audit(j,e)){money(e->peer.amount,e->peer.code,a,sizeof(a));snprintf(s,sizeof(s),"%s: %s",ui_tr(cash_entry_is_audit(j,e)?UI_N_("Book balance"):UI_N_("Cash received")),a);hint(o,26,136,568,s);}
    snprintf(s,sizeof(s),"%s%s",operator_text(e->operator_id,e->operator_name),e->approved?ui_tr(" · Management authorization"):"");hint(o,26,154,568,s);hint(o,26,198,568,e->note);if(e->operator_name[0]&&strcmp(operator_text(e->operator_id,e->operator_name),e->operator_name)){snprintf(s,sizeof(s),ui_tr("Recorded as: %s"),e->operator_name);hint(o,26,176,568,s);}
    if(e->cash.record){snprintf(s,sizeof(s),"#%u · %s · %s",e->cash.record,e->cash.code,operator_text(e->cash.operator_id,e->cash.operator_name));button(v,o,26,222,568,34,s,A_SOURCE,2);}
    uint32_t linked=e->reverse_of?e->reverse_of:e->corrects;if(!linked)for(unsigned i=0;i<j->count;++i)if(j->entries[i].reverse_of==e->id){linked=j->entries[i].id;break;}if(linked){snprintf(s,sizeof(s),"#%u",linked);button(v,o,26,304,112,42,s,A_DETAIL+linked-1,0);}
    unsigned allowed=cash_journal_entry_actions(j,e->id);
    if(allowed&CASH_ACTION_CORRECT)button(v,o,290,304,140,42,ui_tr("Correct"),A_CORRECT,0);
    lv_obj_t *cancel=button(v,o,442,304,152,42,ui_tr("Void entry"),A_REVERSE,2);if(!(allowed&CASH_ACTION_VOID))lv_obj_add_state(cancel,LV_STATE_DISABLED);
    const char *reason=cash_journal_reversed(j,e->id)?UI_N_("Already voided. Open the linked entry to review."):e->kind==CASH_OPENING?UI_N_("Adjust the opening balance with Correct."):e->kind==CASH_REVERSAL?UI_N_("This is a reversal record; the original is linked below."):UI_N_("Void keeps a record and reverses its balance effect.");
    hint(o,26,267,568,ui_tr(reason));
}
static void draw_audit(cash_view_t *v,const cash_journal_t *j)
{
    lv_obj_t *left=cw_box(v->body,0,0,710,304,0xFFFFFF,17),*right=cw_box(v->body,728,0,504,304,CW_PANEL,17);char s[100];
    cw_label(left,20,24,500,ui_tr("Count the entire cashbox"),&lv_font_instrument_sans_semibold_18,CW_INK);button(v,left,586,16,104,42,book.code,A_CURRENCY,0);hint(left,20,69,670,ui_tr("Count all cash in the cashbox, not an arbitrary single batch."));hint(left,20,123,300,ui_tr("Book balance"));amount(left,20,155,330,cash_journal_balance(j,book.code),book.code,&lv_font_instrument_sans_semibold_37);hint(left,365,123,325,ui_tr("Actual cash"));money(book.actual,book.code,s,sizeof(s));button(v,left,365,147,325,64,book.actual_set?s:ui_tr("Enter amount"),A_AUDIT_AMOUNT,0);button(v,left,20,242,220,42,ui_tr("Import current count"),A_IMPORT,2);button(v,left,470,242,220,42,ui_tr("Reconciliation records"),A_AUDIT_HISTORY,0);
    cw_label(right,20,24,464,ui_tr("Difference"),&lv_font_instrument_sans_semibold_18,CW_INK);if(book.actual_set)amount(right,20,98,464,book.actual-cash_journal_balance(j,book.code),book.code,&lv_font_instrument_sans_semibold_37);else cw_label(right,20,111,464,ui_tr("Enter actual cash"),&lv_font_instrument_sans_medium_24,CW_MUTED);hint(right,20,177,464,ui_tr("Differences are saved for review, never posted automatically."));button(v,right,20,242,464,42,ui_tr("Save reconciliation"),A_AUDIT_SAVE,1);
}
static bool match(const cash_entry_t *e)
{
    if(book.audit_history!=cash_entry_is_audit(cash_journal_store_get(),e))return false;
    if(strcmp(e->cash.code,book.code)&&((e->kind!=CASH_EXCHANGE&&e->kind!=CASH_REVERSAL)||strcmp(e->peer.code,book.code)))return false;
    if(book.filter&&e->operator_id!=book.filter)return false;
    if(book.query[0]){char id[20];snprintf(id,sizeof(id),"%u",e->id);if(!strstr(id,book.query)&&!strstr(e->note,book.query)&&!strstr(e->operator_name,book.query)&&!strstr(operator_text(e->operator_id,e->operator_name),book.query))return false;}return true;
}
static lv_obj_t *entry_row_create(lv_obj_t *parent,lv_coord_t width,void *context)
{
    (void)context;lv_obj_t *row=cw_box(parent,0,0,width,60,0xFFFFFF,0);
    lv_obj_set_style_bg_color(row,lv_color_hex(CW_SOFT),LV_STATE_PRESSED);
    lv_obj_t *badge=cw_box(row,4,13,32,32,CW_SOFT,10);
    lv_obj_t *sign=cw_label(badge,0,0,32,"",&lv_font_instrument_sans_medium_20,CW_BLUE);lv_obj_set_style_text_align(sign,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(sign);
    cw_label(row,48,7,310,"",&lv_font_instrument_sans_medium_15,CW_INK);
    cw_label(row,48,32,470,"",&lv_font_instrument_sans_medium_12,CW_MUTED);
    lv_obj_t *value=cw_label(row,530,19,width-542,"",&lv_font_instrument_sans_medium_20,CW_INK);lv_obj_set_style_text_align(value,LV_TEXT_ALIGN_RIGHT,0);
    cw_box(row,0,59,width,1,0xEDF1F5,0);return row;
}
static void entry_row_bind(lv_obj_t *row,uint32_t index,void *context)
{
    (void)context;const cash_journal_t *j=cash_journal_store_get();if(index>=entry_count||!j)return;
    const cash_entry_t *e=&j->entries[entry_indices[index]];char text[180],value[70];int64_t delta=cash_entry_delta(j,e,book.code);bool negative=delta<0;
    lv_obj_t *badge=lv_obj_get_child(row,0);lv_obj_set_style_bg_color(badge,lv_color_hex(negative?0xF0EDF9:CW_SOFT),0);
    lv_obj_t *sign=lv_obj_get_child(badge,0);lv_label_set_text(sign,cash_entry_is_audit(j,e)?"=":negative?"-":"+");lv_obj_set_style_text_color(sign,lv_color_hex(negative?0x8059CA:CW_BLUE),0);lv_obj_center(sign);
    snprintf(text,sizeof(text),"%s%s",ui_tr(kind_text(e->kind)),cash_journal_reversed(j,e->id)?ui_tr(" · Voided"):"");lv_label_set_text(lv_obj_get_child(row,1),text);
    snprintf(text,sizeof(text),"#%u · %s · %.64s",e->id,operator_text(e->operator_id,e->operator_name),e->note);lv_label_set_text(lv_obj_get_child(row,2),text);
    money(cash_entry_is_audit(j,e)?e->cash.amount:negative?-delta:delta,book.code,value,sizeof(value));if(cash_entry_is_audit(j,e))snprintf(text,sizeof(text),"%s",value);else snprintf(text,sizeof(text),"%c%s",negative?'-':'+',value);lv_label_set_text(lv_obj_get_child(row,3),text);
}
static void entry_list_changed(const ui_list_window_t *window,void *context)
{
    (void)context;book.offset=window->first;if(entry_range){char text[80];snprintf(text,sizeof(text),"%u-%u / %u",window->count?window->first+1:0,ui_list_window_last(window),window->count);lv_label_set_text(entry_range,text);}
}
static void entry_pointer(lv_event_t *event)
{
    lv_event_code_t code=lv_event_get_code(event);if(code!=LV_EVENT_PRESSED&&code!=LV_EVENT_PRESSING&&code!=LV_EVENT_RELEASED&&code!=LV_EVENT_PRESS_LOST)return;
    lv_indev_t *input=lv_event_get_indev(event);if(!entry_list||!input||code==LV_EVENT_PRESS_LOST){entry_tapping=false;return;}
    lv_point_t point;lv_indev_get_point(input,&point);uint32_t index;
    if(code==LV_EVENT_PRESSED){entry_tapping=lv_recycled_list_index_at_point(entry_list,&point,&index)&&index<entry_count;entry_pressed=entry_tapping?entry_indices[index]+1:0;entry_revision=cash_journal_store_revision();entry_press=point;return;}
    if(LV_ABS(point.x-entry_press.x)>=6||LV_ABS(point.y-entry_press.y)>=6)entry_tapping=false;
    if(code==LV_EVENT_RELEASED){bool tap=entry_tapping&&lv_recycled_list_tap_allowed(entry_list)&&entry_revision==cash_journal_store_revision()&&lv_recycled_list_index_at_point(entry_list,&point,&index)&&index<entry_count&&(uint32_t)entry_indices[index]+1==entry_pressed;entry_tapping=false;if(tap){book.detail=entry_pressed;queue(&journal);}}
}
static void draw_book(cash_view_t *v)
{
    default_code(book.code);const cash_journal_t *j=cash_journal_store_get();cash_store_status_t status=cash_journal_store_status();
    if(status!=CASH_STORE_READY){cw_label(v->body,20,60,1170,ui_tr(status==CASH_STORE_LOADING?UI_N_("Loading cash journal..."):status==CASH_STORE_SAVING?UI_N_("Saving cash entry..."):status==CASH_STORE_FAILED?UI_N_("Cash entry not confirmed. Retry the same save."):UI_N_("Cash journal unavailable. Existing data was preserved.")),&lv_font_instrument_sans_semibold_20,CW_INK);if(status==CASH_STORE_FAILED)button(v,v->body,20,134,200,44,ui_tr("Retry"),A_RETRY,1);return;}if(!j)return;
    if(book.audit){draw_audit(v,j);if(book.active)draw_form(v);return;}
    lv_obj_t *left=cw_box(v->body,0,0,300,304,CW_PANEL,17),*list=cw_box(v->body,318,0,914,304,0xFFFFFF,17);cw_label(left,20,29,180,ui_tr("Book balance"),&lv_font_instrument_sans_semibold_18,CW_INK);button(v,left,210,20,70,42,book.code,A_CURRENCY,0);
    bool opened=cash_journal_opened(j,book.code);if(opened)amount(left,20,91,260,cash_journal_balance(j,book.code),book.code,&lv_font_instrument_sans_semibold_37);else cw_label(left,20,104,260,ui_tr("Not set"),&lv_font_instrument_sans_medium_24,CW_MUTED);
    hint(left,20,159,260,ui_tr("Shared cashbox · all operators"));button(v,left,20,193,260,38,ui_tr("Import count"),A_IMPORT,2);if(opened){button(v,left,20,242,124,42,ui_tr("Receipt"),A_RECEIPT,1);button(v,left,156,242,124,42,ui_tr("Payment"),A_PAYMENT,2);}else button(v,left,20,242,260,42,ui_tr("Set opening balance"),A_OPENING,1);
    cw_label(list,20,29,255,ui_tr(book.audit_history?UI_N_("Reconciliation records"):UI_N_("Cash entries")),&lv_font_instrument_sans_semibold_18,CW_INK);button(v,list,484,20,138,34,ui_tr(book.filter?UI_N_("Filtered operator"):UI_N_("All operators")),A_PEOPLE,0);button(v,list,634,20,124,34,book.query[0]?book.query:ui_tr("Search"),A_QUERY,0);button(v,list,770,20,124,34,ui_tr("Import count"),A_IMPORT,2);
    entry_count=0;for(unsigned i=j->count;i>0;--i)if(match(&j->entries[i-1]))entry_indices[entry_count++]=i-1;
    entry_range=cw_label(list,20,265,190,"",&lv_font_instrument_sans_medium_12,CW_MUTED);
    button(v,list,250,254,150,34,ui_tr(book.paged?UI_N_("Page view"):UI_N_("Scroll view")),A_LIST_MODE,0);
    button(v,list,520,254,200,34,ui_tr("Export this view"),A_EXPORT,2);button(v,list,772,254,52,34,"<",A_PREV,0);button(v,list,842,254,52,34,">",A_NEXT,0);
    uint32_t first=book.offset;lv_recycled_list_config_t config={.x=20,.y=70,.width=874,.rows=3,.row_height=60,.create_row=entry_row_create,.bind_row=entry_row_bind,.changed=entry_list_changed};
    entry_list=lv_recycled_list_create(list,&config);
    if(entry_list){lv_recycled_list_refresh(entry_list,entry_count,true);lv_recycled_list_set_paged(entry_list,book.paged);if(first<entry_count)lv_recycled_list_scroll_to_index(entry_list,first);lv_recycled_list_set_press_feedback(entry_list,true);lv_obj_add_event_cb(lv_recycled_list_object(entry_list),entry_pointer,LV_EVENT_ALL,NULL);}
    if(book.active)draw_form(v);else if(book.detail&&book.detail<=j->count)draw_detail(v,j);
}
static void draw(void *context)
{
    cash_view_t *v=context;v->queued=false;if(!v->root)return;close_overlay(v);if(v->ledger){entry_list=NULL;entry_range=NULL;entry_tapping=false;}lv_obj_clean(v->body);header(v);if(v->ledger)draw_book(v);else draw_calculator(v);lv_label_set_text(v->status_line,v->ledger&&book.active?"":ui_tr(v->feedback));
}
static void tick(lv_timer_t *timer)
{
    cash_view_t *v=timer->user_data;if(!v->root||!lv_obj_is_visible(v->root))return;unsigned workspace_revision=workspace_store_revision();if(v->workspace_revision!=workspace_revision){v->workspace_revision=workspace_revision;queue(v);}unsigned date_revision=machine_time_confirmation_revision();if(v->date_revision!=date_revision){v->date_revision=date_revision;if(v->ledger&&book.active)queue(v);}cash_store_status_t state=cash_journal_store_status();unsigned revision=cash_journal_store_revision();
    if(book.saving&&state==CASH_STORE_READY&&revision!=book.before_revision){book.saving=false;book.correction=0;book.detail=0;if(book.has_resume){book.entry=book.resume;book.has_resume=false;book.active=true;book.owner=actor_id();}else book.active=false;v->feedback[0]=0;}
    if(v->revision!=revision||v->status!=state){v->revision=revision;v->status=state;queue(v);}
}
static void deleted(lv_event_t *e)
{cash_view_t *v=lv_event_get_user_data(e);if(lv_event_get_target(e)!=v->root)return;if(v->ledger){entry_list=NULL;entry_range=NULL;entry_tapping=false;}lv_async_call_cancel(draw,v);if(v->timer)lv_timer_del(v->timer);v->timer=NULL;v->root=v->body=v->header=v->overlay=v->status_line=NULL;memset(&v->pin,0,sizeof(v->pin));v->queued=false;cash_access_cancel();}
static void create(cash_view_t *v,lv_obj_t *parent,bool ledger)
{
    if(v->root)return;uint32_t id=actor_id();if(id!=last_actor){memset(&calc,0,sizeof(calc));strcpy(calc.text,"0");if(!book.saving){book.active=book.has_resume=false;book.detail=0;}last_actor=id;}
    v->ledger=ledger;v->root=cw_root(parent);v->header=cw_box(v->root,0,0,1280,72,0xFFFFFF,0);lv_obj_set_style_bg_opa(v->header,LV_OPA_TRANSP,0);v->body=cw_box(v->root,24,76,1232,304,0xFFFFFF,0);lv_obj_set_style_bg_opa(v->body,LV_OPA_TRANSP,0);lv_obj_add_event_cb(v->root,deleted,LV_EVENT_DELETE,v);v->status_line=cw_label(v->root,24,382,1232,"",&lv_font_instrument_sans_medium_12,0xAA3845);v->status=cash_journal_store_status();v->revision=cash_journal_store_revision();v->date_revision=machine_time_confirmation_revision();v->workspace_revision=workspace_store_revision();v->timer=lv_timer_create(tick,200,v);draw(v);
}
static void destroy(cash_view_t *v)
{if(!v->root)return;settings_detail_keyboard_hide();settings_detail_dialog_hide();close_overlay(v);lv_async_call_cancel(draw,v);if(v->timer)lv_timer_del(v->timer);v->timer=NULL;lv_obj_del(v->root);}
void ui_calculator_create(lv_obj_t *p){create(&calculator,p,false);}void ui_calculator_destroy(void){destroy(&calculator);}
void ui_cash_journal_create(lv_obj_t *p){create(&journal,p,true);}void ui_cash_journal_destroy(void){destroy(&journal);}
void ui_cash_journal_filter_operator(uint32_t id){book.filter=id;book.offset=0;}
