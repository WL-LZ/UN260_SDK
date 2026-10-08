#include "un260/font/ui_locale_font_bank.h"
#include "un260/lv_components/lv_alnum_keyboard.h"
#include "un260/lv_components/lv_pin_keypad.h"
#include "un260/lv_core/page_08_boot.h"
#include "un260/lv_core/page_00_boot_anim.h"
#include "un260/lv_components/lv_qr_popup.h"
#include "un260/boot/boot_service.h"
void boot_service_snapshot(boot_snapshot_t *s){memset(s,0,sizeof(*s));}
#define CASH_REAL_FONTS
#define main menu_suite_main
#include "test_menu_view.c"
#undef main
#include "un260/lv_core/page_cash_tools.h"
#include "un260/app_service/cash_tools_service.h"
#include "un260/storage/cash_journal_store.h"
#include "un260/app_service/cash_workspace_access.h"
static cash_journal_t journal_fixture;
static cash_entry_t pending_entry;static uint32_t pending_correction;static bool pending_save;static unsigned current_currencies=1;
static bool date_confirmed;static unsigned date_revision;
uint32_t machine_time_confirmation_revision(void){return date_revision;}
bool machine_time_is_confirmed(void){return date_confirmed;}
bool machine_time_is_valid(const machine_time_value_t *t){return t&&t->year>=2020;}
void machine_time_confirm(const machine_time_value_t *t){date_confirmed=machine_time_is_valid(t);++date_revision;}
static unsigned char background[1280*400*4];
static unsigned char boot_background[1280*400*4],ready_background[1280*400*4];
static unsigned char boot_emblem[96*96*4];
static char space_result[33];static void space_submit(const char *text,void *context){(void)context;snprintf(space_result,sizeof(space_result),"%s",text);}
static bool is_background(const void *src){return lv_img_src_get_type(src)==LV_IMG_SRC_FILE&&(strstr(src,"backgrounds/user.png")||strstr(src,"boot_theme_d/")||strstr(src,"backgrounds/boot.png"));}
static lv_res_t cash_asset_info(lv_img_decoder_t *d,const void *src,lv_img_header_t *h){if(!is_background(src))return asset_info(d,src,h);bool emblem=strstr(src,"emblem.png")!=NULL;memset(h,0,sizeof(*h));h->w=emblem?96:1280;h->h=emblem?96:400;h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;}
static lv_res_t cash_asset_open(lv_img_decoder_t *d,lv_img_decoder_dsc_t *s){if(!is_background(s->src))return asset_open(d,s);cash_asset_info(d,s->src,&s->header);s->img_data=strstr(s->src,"emblem.png")?boot_emblem:strstr(s->src,"boot_theme_d/")?boot_background:strstr(s->src,"backgrounds/boot.png")?ready_background:background;return LV_RES_OK;}
cash_store_status_t cash_journal_store_status(void){return pending_save?CASH_STORE_SAVING:CASH_STORE_READY;}
const cash_journal_t *cash_journal_store_get(void){return &journal_fixture;}
unsigned cash_journal_store_revision(void){return journal_fixture.count;}
bool cash_journal_store_retry(void){return false;}
cash_error_t cash_journal_store_submit(const cash_entry_t *e,uint32_t correction){assert(!pending_save);pending_entry=*e;pending_correction=correction;pending_save=true;return CASH_OK;}
unsigned cash_tools_currencies(char (*codes)[4],unsigned cap){assert(cap>=2);strcpy(codes[0],"USD");strcpy(codes[1],"CNY");return 2;}
unsigned cash_tools_history_count(void){return 1;}
bool cash_tools_history(unsigned index,cash_snapshot_t *s){if(index)return false;*s=(cash_snapshot_t){.code="USD",.record=128,.recorded_at=20261006100000ULL,.amount=1680*CASH_SCALE,.count=3,.denoms={{100*CASH_SCALE,12},{50*CASH_SCALE,8},{20*CASH_SCALE,4}}};return true;}
bool cash_tools_history_ready(void){return true;}
void cash_tools_entry_identity(cash_entry_t *e){e->time=date_confirmed?1791244800:0;e->operator_id=1;strcpy(e->operator_name,"Alex");}
const char *cash_tools_current_blocker(void){return NULL;}
const char *cash_tools_write_blocker(void){return NULL;}
unsigned cash_tools_current_count(void){return current_currencies;}
bool cash_tools_current(unsigned i,cash_snapshot_t *s){if(i>=current_currencies)return false;cash_tools_history(0,s);if(i){strcpy(s->code,"CNY");s->amount=500*CASH_SCALE;}return true;}
static bool require_pin;
static unsigned pin_attempts;
bool cash_access_required(cash_access_action_t a){(void)a;return require_pin;}
bool cash_access_verify(cash_access_action_t a,const char *p){(void)a;++pin_attempts;return p&&!strcmp(p,"2468");}
void cash_access_cancel(void){}
uint32_t cash_access_retry_delay_ms(void){return 0;}
static const char *delete_blocker;
const char *cash_workspace_delete_blocker(uint32_t id){(void)id;return delete_blocker;}
bool cashbook_store_check_operator(uint32_t id){return id!=0;}
cashbook_reference_t cashbook_store_operator_reference(uint32_t id){(void)id;return CASHBOOK_REF_CLEAR;}
bool cash_workspace_delete(uint32_t id){if(delete_blocker)return false;workspace_model_t next=model;if(!workspace_remove_user(&next,id))return false;return workspace_store_save(&next);}
bool cash_workspace_save(const workspace_model_t *m,bool auth){(void)auth;return workspace_store_save(m);}
cash_error_t cash_workspace_submit(cash_entry_t *e,uint32_t correction){cash_tools_entry_identity(e);return cash_journal_store_submit(e,correction);}
bool cash_journal_export_start(const uint32_t *ids,unsigned n){return ids&&n;}
static lv_obj_t *cash_button(lv_obj_t *p,const char *text){
 if(!lv_obj_is_visible(p))return NULL;
 if(lv_obj_check_type(p,&lv_label_class)&&!strcmp(lv_label_get_text(p),text)&&lv_obj_get_child_cnt(lv_obj_get_parent(p))==1)return lv_obj_get_parent(p);
 for(unsigned i=0;i<lv_obj_get_child_cnt(p);++i){lv_obj_t *o=cash_button(lv_obj_get_child(p,i),text);if(o)return o;}return NULL;
}
static void cash_click(const char *text){
 lv_obj_t *o=cash_button(lv_scr_act(),text);
 if(!o){
  /* Match the text LVGL actually stores after Arabic shaping. */
  lv_obj_t *probe=lv_label_create(lv_scr_act());lv_label_set_text(probe,text);
  char *shaped=strdup(lv_label_get_text(probe));assert(shaped);lv_obj_del(probe);
  o=cash_button(lv_scr_act(),shaped);free(shaped);
 }
 if(!o)fprintf(stderr,"Missing cash button: %s\n",text);assert(o);
 if(lv_obj_check_type(o,&lv_label_class))o=lv_obj_get_parent(o);
 lv_event_send(o,LV_EVENT_CLICKED,NULL);tick(100);
}
#include "test_locale_view_checks.h"

static void cached_pin_locales(void){
 lv_pin_keypad_t pin={0};assert(lv_pin_keypad_create(&pin,lv_scr_act(),215,18));
 const language_t sequence[]={LANGUAGE_CN,LANGUAGE_KR,LANGUAGE_JA,LANGUAGE_AR,LANGUAGE_EN};
 for(unsigned i=0;i<sizeof(sequence)/sizeof(sequence[0]);++i){
  assert(ui_lang_prepare_resources(sequence[i]));ui_lang_set(sequence[i]);
  lv_pin_keypad_config_t cfg={.title=ui_tr("Settings access"),.prompt=ui_tr("Enter your 4-digit password."),
   .footnote=ui_tr("For authorized configuration and service."),.idle_status=ui_tr("Opens automatically when the code is correct."),.compact=true,.auto_confirm=true};
  assert(lv_pin_keypad_show(&pin,&cfg,""));tick(100);check_single_line(pin.root);
  bool rtl=sequence[i]==LANGUAGE_AR;
  assert(lv_obj_get_x(pin.dots[0])==(rtl?289:34));
  assert(lv_obj_get_x(pin.digits[0])<lv_obj_get_x(pin.digits[3]));
  assert(lv_obj_get_x(pin.footnote_icon)==(rtl?398:34));
  assert(lv_obj_get_width(pin.status)==(rtl?376:340));
  lv_obj_t *labels[]={pin.title,pin.prompt,pin.status,pin.footnote};
  for(unsigned j=0;j<4;++j){
   lv_obj_t *o=labels[j];lv_point_t size;
   lv_txt_get_size(&size,lv_label_get_text(o),lv_obj_get_style_text_font(o,0),lv_obj_get_style_text_letter_space(o,0),lv_obj_get_style_text_line_space(o,0),lv_obj_get_width(o),LV_TEXT_FLAG_NONE);
   assert(lv_obj_get_y(o)+size.y<=lv_obj_get_height(pin.root)-12);
   if(j==0)assert(size.y<=lv_obj_get_style_text_font(o,0)->line_height);
  }
  char file[64];snprintf(file,sizeof(file),"cached-settings-pin-%u",sequence[i]);raster(file);
  lv_pin_keypad_hide(&pin);
  assert(lv_qr_popup_show("UN260 locale test"));tick(100);check_single_line(lv_layer_top());
  snprintf(file,sizeof(file),"cached-qr-%u",sequence[i]);raster(file);lv_qr_popup_hide();
 }
 lv_pin_keypad_destroy(&pin);
}

static void early_boot_locales(void){
 ui_page_t previous=current;current=UI_PAGE_BOOT;
 for(size_t i=0;i<ui_lang_count();++i){
  const ui_locale_t *locale=ui_lang_at(i);ui_lang_set(LANGUAGE_EN);
  ui_page_08_curr_create(lv_scr_act());ui_page_00_boot_anim_create(lv_layer_top());
  assert(ui_lang_prepare_resources(locale->id));ui_lang_set(locale->id);
  ui_page_00_boot_anim_set_startup_ready(true);ui_page_00_boot_anim_adopt_elapsed(4300);tick(40);
  check_single_line(lv_layer_top());
  ui_page_00_boot_anim_set_startup_error(true);tick(100);check_single_line(lv_layer_top());
  char file[64];snprintf(file,sizeof(file),"startup-diagnostic-%s",locale->tag);raster(file);
  ui_page_00_boot_anim_destroy();ui_page_08_curr_destroy();
 }
 ui_lang_set(LANGUAGE_EN);ui_page_08_curr_create(lv_scr_act());ui_page_08_curr_set_covered(true);
 for(size_t i=1;i<ui_lang_count();++i){
  const ui_locale_t *locale=ui_lang_at(i);assert(ui_lang_prepare_resources(locale->id));ui_lang_set(locale->id);
  ui_page_08_curr_set_covered(false);tick(1500);check_single_line(lv_scr_act());
  char file[64];snprintf(file,sizeof(file),"early-selfcheck-%s",locale->tag);raster(file);
  ui_page_08_curr_set_covered(true);
 }
 ui_page_08_curr_destroy();ui_lang_set(LANGUAGE_EN);current=previous;
}

int main(void){
 const char *bg_path=getenv("UN260_CASH_BACKGROUND");assert(bg_path);
 FILE *f=fopen(bg_path,"rb");assert(f);assert(fread(background,1,sizeof(background),f)==sizeof(background));fclose(f);
 f=fopen(getenv("UN260_BOOT_BACKGROUND"),"rb");assert(f);assert(fread(boot_background,1,sizeof(boot_background),f)==sizeof(boot_background));fclose(f);
 f=fopen(getenv("UN260_READY_BACKGROUND"),"rb");assert(f);assert(fread(ready_background,1,sizeof(ready_background),f)==sizeof(ready_background));fclose(f);
 f=fopen(getenv("UN260_BOOT_EMBLEM"),"rb");assert(f);assert(fread(boot_emblem,1,sizeof(boot_emblem),f)==sizeof(boot_emblem));fclose(f);
 workspace_defaults(&model);cashbook_defaults(&ledger);
 lv_init();ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,draw_buffer,NULL,1280*64);lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=1280;d.ver_res=400;d.flush_cb=flush;d.draw_buf=&buf;lv_disp_t *display=lv_disp_drv_register(&d);assert(display);ui_scrollbar_init(display);
 lv_indev_drv_t in;lv_indev_drv_init(&in);in.type=LV_INDEV_TYPE_POINTER;in.read_cb=read_pointer;assert(lv_indev_drv_register(&in));
 lv_img_decoder_t *decoder=lv_img_decoder_create();lv_img_decoder_set_info_cb(decoder,cash_asset_info);lv_img_decoder_set_open_cb(decoder,cash_asset_open);lv_img_decoder_set_close_cb(decoder,asset_close);
 cached_pin_locales();
 early_boot_locales();
 current=UI_PAGE_CALCULATOR;ui_calculator_create(lv_scr_act());tick(250);cash_click("1");cash_click("2");cash_click("+");cash_click("3");cash_click("=");assert(find(lv_scr_act(),"15"));check_single_line(lv_scr_act());raster("cash-calculator");
 cash_click("Exchange rate");cash_click("Import count");assert(find(lv_scr_act(),"1680.00"));check_single_line(lv_scr_act());raster("cash-fx");ui_calculator_destroy();tick(300);
 cash_entry_t e={.kind=CASH_OPENING,.time=1791244800,.cash={.code="USD",.amount=2000*CASH_SCALE}};assert(cash_journal_append(&journal_fixture,&e)==CASH_OK);
 e.kind=CASH_RECEIPT;e.cash.amount=1680*CASH_SCALE;assert(cash_journal_append(&journal_fixture,&e)==CASH_OK);e.kind=CASH_PAYMENT;e.cash.amount=280*CASH_SCALE;assert(cash_journal_append(&journal_fixture,&e)==CASH_OK);
 current=UI_PAGE_CASH_JOURNAL;ui_cash_journal_create(lv_scr_act());tick(250);assert(!cash_button(lv_scr_act(),ui_tr("Quick")));check_single_line(lv_scr_act());raster("cash-journal");cash_click("Receipt");check_single_line(lv_scr_act());raster("cash-entry");cash_click("Cancel");
 cash_click("Payment");cash_click("0.00 USD");assert(find(lv_scr_act(),"."));assert(lv_obj_get_width(find(lv_scr_act(),"1"))==107);assert(lv_obj_get_style_border_width(find(lv_scr_act(),"1"),0)==0);assert(!cash_button(lv_obj_get_parent(find(lv_scr_act(),"1")),"-"));raster("cash-amount-keypad");cash_click("Clear");cash_click("2");cash_click("2");cash_click("Apply");
 assert(find(lv_scr_act(),"3400.00 - 22.00 = 3378.00 USD"));raster("cash-payment-22");
 ui_notice_set_enabled(false);cash_click("Confirm");assert(find(lv_scr_act(),"Set the device date before recording cash."));ui_notice_set_enabled(true);assert(!pending_save&&journal_fixture.count==3);assert(find(lv_scr_act(),"Use this date"));
 cash_click("Note (optional)");cash_click("A");cash_click("Space");cash_click("B");cash_click("Apply");assert(find(lv_scr_act(),"A B"));raster("cash-note-space");cash_click("Use this date");cash_click("Confirm");assert(pending_save);assert(pending_entry.kind==CASH_PAYMENT&&pending_entry.cash.amount==22*CASH_SCALE);
 assert(cash_journal_append(&journal_fixture,&pending_entry)==CASH_OK);pending_save=false;tick(300);assert(cash_journal_balance(&journal_fixture,"USD")==3378*CASH_SCALE);
 cash_click("Payment");current_currencies=2;cash_click("Import count");assert(find(lv_scr_act(),"MULTI currencies are imported separately."));cash_click("Cancel");assert(find(lv_scr_act(),"3378.00 + 0.00 = 3378.00 USD"));cash_click("Import count");cash_click("#128    1680.00 USD");assert(find(lv_scr_act(),"3378.00 - 1680.00 = 1698.00 USD"));current_currencies=1;cash_click("Cancel");assert(journal_fixture.count==4);
 /* Dragging the recycled rows must not open a transaction. */
 point=(lv_point_t){700,165};down=LV_INDEV_STATE_PRESSED;tick(60);lv_refr_now(NULL);assert(framebuffer[150*1280+380].full==lv_color_hex(0xEAF2FF).full);raster("cash-entry-pressed");point.y=195;tick(60);lv_refr_now(NULL);assert(framebuffer[150*1280+380].full!=lv_color_hex(0xEAF2FF).full);down=LV_INDEV_STATE_RELEASED;tick(900);assert(!cash_button(lv_scr_act(),"Correct"));
 point=(lv_point_t){700,265};down=LV_INDEV_STATE_PRESSED;tick(40);for(int y=250;y>=165;y-=15){point.y=y;tick(20);}down=LV_INDEV_STATE_RELEASED;tick(900);assert(!cash_button(lv_scr_act(),"Correct"));
 strcpy(model.users[0].name,"Alex Renamed");++revision;tick(300);assert(!strcmp(journal_fixture.entries[3].operator_name,"Alex"));assert(!strcmp(workspace_record_name(&model,1,"Alex"),"Alex Renamed"));cash_click("Scroll view");assert(find(lv_scr_act(),"Page view"));cash_click("<");tick(300);
 point=(lv_point_t){700,165};down=LV_INDEV_STATE_PRESSED;tick(60);down=LV_INDEV_STATE_RELEASED;tick(300);assert(cash_button(lv_scr_act(),"Void entry"));assert(find(lv_scr_act(),"Alex Renamed"));assert(find(lv_scr_act(),"Recorded as: Alex"));raster("cash-entry-detail");
 cash_click("Void entry");assert(find(lv_scr_act(),"3378.00 + 22.00 = 3400.00 USD"));cash_click("Void entry");assert(pending_save&&pending_entry.kind==CASH_REVERSAL&&pending_entry.reverse_of==4);assert(cash_journal_append(&journal_fixture,&pending_entry)==CASH_OK);pending_save=false;tick(300);assert(cash_journal_balance(&journal_fixture,"USD")==3400*CASH_SCALE);assert(cash_journal_reversed(&journal_fixture,4));
 cash_click("Reconcile");cash_click("Enter amount");cash_click("Clear");cash_click("3");cash_click("4");cash_click("0");cash_click("0");cash_click("Apply");cash_click("Save reconciliation");assert(cash_button(lv_scr_act(),"Confirm"));cash_click("Confirm");assert(pending_save&&pending_entry.kind==CASH_AUDIT&&!strcmp(pending_entry.cash.code,"USD"));assert(cash_journal_append(&journal_fixture,&pending_entry)==CASH_OK);pending_save=false;tick(300);assert(cash_journal_balance(&journal_fixture,"USD")==3400*CASH_SCALE);cash_click("Reconciliation records");point=(lv_point_t){700,165};down=LV_INDEV_STATE_PRESSED;tick(60);down=LV_INDEV_STATE_RELEASED;tick(200);assert(cash_button(lv_scr_act(),"Void entry"));raster("cash-audit-detail");cash_click("Void entry");cash_click("Void entry");assert(pending_save&&cash_entry_is_audit(&journal_fixture,&pending_entry));assert(cash_journal_append(&journal_fixture,&pending_entry)==CASH_OK);pending_save=false;tick(300);assert(cash_journal_valid(&journal_fixture)&&cash_journal_balance(&journal_fixture,"USD")==3400*CASH_SCALE);
 cash_click("Cash entries");cash_click("Search");cash_click("1");cash_click("Apply");point=(lv_point_t){700,165};down=LV_INDEV_STATE_PRESSED;tick(60);down=LV_INDEV_STATE_RELEASED;tick(200);assert(lv_obj_has_state(cash_button(lv_scr_act(),"Void entry"),LV_STATE_DISABLED));assert(lv_obj_get_style_text_opa(cash_button(lv_scr_act(),"Void entry"),LV_PART_MAIN)==LV_OPA_COVER);assert(find(lv_scr_act(),"Adjust the opening balance with Correct."));raster("cash-opening-detail");cash_click("Cancel");cash_click("1");cash_click("Clear");cash_click("Apply");
 ui_cash_journal_destroy();tick(300);
 for(unsigned i=0;i<20;++i){ui_calculator_create(lv_scr_act());tick(20);ui_calculator_destroy();ui_cash_journal_create(lv_scr_act());tick(20);ui_cash_journal_destroy();}tick(300);
 lv_obj_t *temporary=lv_obj_create(lv_scr_act());ui_calculator_create(temporary);lv_obj_del(temporary);tick(300);ui_calculator_destroy();
 ui_operators_create(lv_scr_act());tick(250);check_single_line(lv_scr_act());lv_obj_t *badge=find(lv_scr_act(),"A");assert(badge);lv_obj_t *number=lv_obj_get_child(badge,0);assert(lv_obj_get_style_text_align(number,0)==LV_TEXT_ALIGN_CENTER);assert(LV_ABS(lv_obj_get_x(number)*2+lv_obj_get_width(number)-lv_obj_get_width(badge))<=1);assert(LV_ABS(lv_obj_get_y(number)*2+lv_obj_get_height(number)-lv_obj_get_height(badge))<=1);assert(!cash_button(lv_scr_act(),"Quick"));assert(find(lv_scr_act(),"Applies to all operators"));raster("cash-operators");
 unsigned people_before=model.user_count;char original_name[25];strcpy(original_name,model.users[0].name);
 cash_click("Add operator");cash_click("Save");tick(300);cash_click("Enter name");cash_click("N");cash_click("Apply");cash_click("Save");tick(300);assert(model.user_count==people_before+1&&!strcmp(model.users[0].name,original_name));

 uint32_t disposable;assert(workspace_add_user(&model,"Temporary",&disposable));revision++;tick(300);
 lv_obj_t *person=find(lv_scr_act(),"Temporary");assert(person);lv_event_send(person,LV_EVENT_CLICKED,NULL);tick(100);
 click("Operator details");delete_blocker="Checking operator records...";cash_click("Delete operator");assert(lv_obj_has_state(cash_button(lv_scr_act(),"Delete"),LV_STATE_DISABLED));assert(lv_obj_get_style_text_color(lv_obj_get_child(cash_button(lv_scr_act(),"Delete"),0),0).full==lv_color_hex(0x627583).full);
 delete_blocker="Linked records prevent deletion.";tick(300);assert(find(lv_scr_act(),delete_blocker));raster("operator-delete-linked");cash_click("Cancel");assert(workspace_find(&model,disposable));
 click("Operator details");delete_blocker=NULL;cash_click("Delete operator");raster("operator-delete-confirm");require_pin=true;cash_click("Delete");
 assert(workspace_find(&model,disposable));assert(find(lv_scr_act(),"Management authorization"));
 assert(lv_obj_get_x(find(lv_scr_act(),"Clear"))==486);
 assert(lv_obj_get_style_border_width(find(lv_scr_act(),"1"),0)==0);
 cash_click("1");cash_click("1");cash_click("1");assert(pin_attempts==0);
 cash_click("1");assert(pin_attempts==1&&workspace_find(&model,disposable));
 assert(find(lv_scr_act(),"Incorrect password. Try again."));raster("operator-pin-invalid");
 cash_click("2");cash_click("4");cash_click("6");cash_click("8");tick(300);
 assert(pin_attempts==2&&!workspace_find(&model,disposable));require_pin=false;
 ui_operators_destroy();
 model.disabled[1]=1;revision++;ui_operators_create(lv_scr_act());tick(200);
 lv_obj_t *disabled_person=find(lv_scr_act(),"N");assert(disabled_person);lv_event_send(disabled_person,LV_EVENT_CLICKED,NULL);tick(100);
 lv_obj_t *management=find(lv_scr_act(),"Device management");lv_obj_t *enable=lv_obj_get_child(management,4);assert(lv_obj_get_width(enable)==54);lv_event_send(enable,LV_EVENT_CLICKED,NULL);tick(300);assert(model.operators_enabled&&model.active_id==1);
 assert(lv_obj_has_state(cash_button(lv_scr_act(),"Disabled"),LV_STATE_DISABLED));click("Operator details");delete_blocker="Linked records prevent deletion.";cash_click("Delete operator");assert(find(lv_scr_act(),"Already disabled. Existing records are retained."));raster("operator-disabled-linked");cash_click("Cancel");
 model.disabled[1]=0;revision++;tick(300);unsigned switch_notices=operation_starts;assert(model.active_id==1);cash_click("Switch operator");assert(model.active_id==1&&operation_starts==switch_notices);cash_click("Confirm switch");assert(model.active_id==model.users[1].id&&operation_started==APP_UI_NOTICE_OPERATOR_SWITCH);raster("operator-switch-confirmed");
 ui_operators_destroy();model.operators_enabled=0;model.active_id=1;revision++;
 for(unsigned i=model.user_count;i<8;++i){char name[25];snprintf(name,sizeof(name),"Operator %u",i+1);assert(workspace_add_user(&model,name,NULL));}revision++;
 ui_operators_create(lv_scr_act());tick(250);raster("operators-eight-scroll");
 lv_obj_t *first_row=find(lv_scr_act(),"Alex Renamed"); /* title can also match, locate by its badge */
 lv_obj_t *initial_badge=find(lv_scr_act(),"A");assert(initial_badge);first_row=lv_obj_get_parent(initial_badge);lv_obj_t *viewport=lv_obj_get_parent(first_row);
 assert(lv_obj_get_width(first_row)<=lv_obj_get_width(viewport)-UI_SCROLLBAR_GUTTER);
 lv_obj_scroll_to_y(viewport,200,LV_ANIM_OFF);tick(100);raster("operators-eight-scrolled");
 lv_coord_t old_scroll=lv_obj_get_scroll_y(viewport);lv_obj_t *row=lv_obj_get_child(viewport,4);lv_event_send(row,LV_EVENT_CLICKED,NULL);tick(100);
 viewport=lv_obj_get_child(lv_obj_get_parent(cash_button(lv_scr_act(),"Scroll view")),0);assert(lv_obj_get_scroll_y(viewport)==old_scroll);
 cash_click("Scroll view");cash_click(">");tick(100);raster("operators-eight-paged");
 ui_operators_destroy();workspace_defaults(&model);revision++;
 lv_alnum_keyboard_config_t space_config={.title="Text search",.placeholder="",.apply_text="Apply",.clear_text="Clear",.cancel_text="Cancel",.max_length=32,.submit=space_submit,.symbols=true};lv_alnum_keyboard_t *space_keyboard=lv_alnum_keyboard_create(lv_scr_act(),&space_config);assert(space_keyboard);lv_alnum_keyboard_show(space_keyboard);tick(80);cash_click("A");cash_click("Space");cash_click("B");raster("cash-text-space-keyboard");cash_click("Apply");assert(!strcmp(space_result,"A B"));lv_alnum_keyboard_destroy(space_keyboard);
 assert(ui_lang_prepare_resources(LANGUAGE_CN));ui_lang_set(LANGUAGE_CN);ui_calculator_create(lv_scr_act());tick(250);assert(!cash_button(lv_scr_act(),ui_tr("Quick")));check_single_line(lv_scr_act());raster("cash-calculator-cn");ui_calculator_destroy();ui_cash_journal_create(lv_scr_act());tick(250);assert(!cash_button(lv_scr_act(),ui_tr("Quick")));check_single_line(lv_scr_act());raster("cash-journal-cn");cash_click(ui_tr("Receipt"));cash_click(ui_tr("Note (optional)"));lv_obj_t *space_cn=cash_button(lv_scr_act(),"Space");assert(space_cn);assert(cash_button(lv_scr_act(),"Clear"));assert(cash_button(lv_scr_act(),"Apply"));assert(cash_button(lv_scr_act(),"Cancel"));assert(!cash_button(lv_scr_act(),ui_tr("Space")));raster("cash-note-keyboard-cn");settings_detail_keyboard_hide();cash_click(ui_tr("Cancel"));ui_cash_journal_destroy();ui_operators_create(lv_scr_act());tick(250);check_single_line(lv_scr_act());assert(find(lv_scr_act(),ui_tr("Device workspace")));raster("cash-operators-cn");unsigned ink=0;for(unsigned y=27;y<46;++y)for(unsigned x=1080;x<1250;++x)if(framebuffer[y*1280+x].full==lv_color_hex(0x7358AD).full)++ink;assert(ink>20);ui_operators_destroy();
 for(size_t language=1;language<ui_lang_count();++language){
  const ui_locale_t *locale=ui_lang_at(language);assert(ui_lang_prepare_resources(locale->id));ui_lang_set(locale->id);
  char file[80];
  ui_calculator_create(lv_scr_act());tick(250);check_single_line(lv_scr_act());
  snprintf(file,sizeof(file),"calculator-%s",locale->tag);raster(file);ui_calculator_destroy();
  ui_cash_journal_create(lv_scr_act());tick(250);check_single_line(lv_scr_act());
  snprintf(file,sizeof(file),"journal-%s",locale->tag);raster(file);
  cash_click(ui_tr("Receipt"));check_single_line(lv_scr_act());
  snprintf(file,sizeof(file),"receipt-%s",locale->tag);raster(file);
  cash_click(ui_tr("Note (optional)"));
  assert(cash_button(lv_scr_act(),"Space")&&cash_button(lv_scr_act(),"Clear")&&cash_button(lv_scr_act(),"Apply"));
  snprintf(file,sizeof(file),"keyboard-%s",locale->tag);raster(file);
  settings_detail_keyboard_hide();cash_click(ui_tr("Cancel"));ui_cash_journal_destroy();
  ui_operators_create(lv_scr_act());tick(250);check_single_line(lv_scr_act());
  snprintf(file,sizeof(file),"operators-%s",locale->tag);raster(file);ui_operators_destroy();
  current=UI_PAGE_MENU;ui_page_03_menu_create(lv_scr_act());tick(150);
  for(unsigned tab=0;tab<6;++tab)for(unsigned sub=0;sub<(tab?subcounts[tab]:1);++sub){
   menu.tab=tab;menu.sub=sub;menu.selected_group=menu.selected_close=0;
   menu.record_pending=menu.verify_pick=false;menu.dirty=true;tick(120);
   check_single_line(lv_scr_act());
   snprintf(file,sizeof(file),"menu-%s-%u-%u",locale->tag,tab,sub);raster(file);
  }
  ui_page_03_menu_destroy();
 }
 ui_lang_set(LANGUAGE_EN);
 current=UI_PAGE_MENU;ui_page_03_menu_create(lv_scr_act());tick(150);
 const int baseline_widths[]={126,103,121,109,147,82};
 for(unsigned i=0;i<6;++i)assert(lv_obj_get_width(menu.nav[i])==baseline_widths[i]);
 assert(lv_obj_get_width(lv_obj_get_parent(menu.nav[0]))==706);
 ui_page_03_menu_destroy();
 puts("PASS native cash tools: arithmetic/formula, payment balance, date confirmation, MULTI cancellation/direction, visible press and drag cancellation, scroll versus tap, void balance, manual reconciliation, numeric PIN geometry, CJK pixels and repeated lifecycle");return 0;
}
