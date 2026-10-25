#include "un260/font/ui_locale_font_bank.h"
#include "un260/lv_components/lv_alnum_keyboard.h"
#include "un260/lv_components/lv_pin_keypad.h"
#include "un260/lv_core/page_08_boot.h"
#include "un260/lv_core/page_00_boot_anim.h"
#include "un260/lv_components/lv_qr_popup.h"
#include "un260/boot/boot_service.h"
void boot_service_snapshot(boot_snapshot_t *s){memset(s,0,sizeof(*s));}
#define CALCULATOR_REAL_FONTS
#define main menu_suite_main
#include "test_menu_view.c"
#undef main
#include "un260/lv_core/page_calculator.h"
#include "un260/lv_core/page_operators.h"
#include "un260/app_service/calculator_import.h"
#include "un260/app_service/workspace_access.h"
static unsigned current_currencies=1;
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
static lv_res_t calculator_asset_info(lv_img_decoder_t *d,const void *src,lv_img_header_t *h){if(!is_background(src))return asset_info(d,src,h);bool emblem=strstr(src,"emblem.png")!=NULL;memset(h,0,sizeof(*h));h->w=emblem?96:1280;h->h=emblem?96:400;h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;}
static lv_res_t calculator_asset_open(lv_img_decoder_t *d,lv_img_decoder_dsc_t *s){if(!is_background(s->src))return asset_open(d,s);calculator_asset_info(d,s->src,&s->header);s->img_data=strstr(s->src,"emblem.png")?boot_emblem:strstr(s->src,"boot_theme_d/")?boot_background:strstr(s->src,"backgrounds/boot.png")?ready_background:background;return LV_RES_OK;}
unsigned calculator_import_currencies(char (*codes)[4],unsigned cap){assert(cap>=2);strcpy(codes[0],"USD");strcpy(codes[1],"CNY");return 2;}
unsigned calculator_import_history_count(void){return 1;}
bool calculator_import_history(unsigned index,calculator_snapshot_t *s){if(index)return false;*s=(calculator_snapshot_t){.code="USD",.record=128,.recorded_at=20261006100000ULL,.amount=1680*CALCULATOR_SCALE,.count=3,.denoms={{100*CALCULATOR_SCALE,12},{50*CALCULATOR_SCALE,8},{20*CALCULATOR_SCALE,4}}};return true;}
bool calculator_import_history_ready(void){return true;}
const char *calculator_import_current_blocker(void){return NULL;}
const char *calculator_import_write_blocker(void){return NULL;}
unsigned calculator_import_current_count(void){return current_currencies;}
bool calculator_import_current(unsigned i,calculator_snapshot_t *s){if(i>=current_currencies)return false;calculator_import_history(0,s);if(i){strcpy(s->code,"CNY");s->amount=500*CALCULATOR_SCALE;}return true;}
static bool require_pin;
static unsigned pin_attempts;
bool workspace_access_required(workspace_access_action_t a){(void)a;return require_pin;}
bool workspace_access_verify(workspace_access_action_t a,const char *p){(void)a;++pin_attempts;return p&&!strcmp(p,"2468");}
void workspace_access_cancel(void){}
uint32_t workspace_access_retry_delay_ms(void){return 0;}
static const char *delete_blocker;
const char *workspace_access_delete_blocker(uint32_t id){(void)id;return delete_blocker;}
bool workspace_access_delete(uint32_t id){if(delete_blocker)return false;workspace_model_t next=model;if(!workspace_remove_user(&next,id))return false;return workspace_store_save(&next);}
bool workspace_access_save(const workspace_model_t *m,bool auth){(void)auth;return workspace_store_save(m);}
static lv_obj_t *calculator_button(lv_obj_t *p,const char *text){
 if(!lv_obj_is_visible(p))return NULL;
 if(lv_obj_check_type(p,&lv_label_class)&&!strcmp(lv_label_get_text(p),text)&&lv_obj_get_child_cnt(lv_obj_get_parent(p))==1)return lv_obj_get_parent(p);
 for(unsigned i=0;i<lv_obj_get_child_cnt(p);++i){lv_obj_t *o=calculator_button(lv_obj_get_child(p,i),text);if(o)return o;}return NULL;
}
static void calculator_click(const char *text){
 lv_obj_t *o=calculator_button(lv_scr_act(),text);
 if(!o){
  /* Match the text LVGL actually stores after Arabic shaping. */
  lv_obj_t *probe=lv_label_create(lv_scr_act());lv_label_set_text(probe,text);
  char *shaped=strdup(lv_label_get_text(probe));assert(shaped);lv_obj_del(probe);
  o=calculator_button(lv_scr_act(),shaped);free(shaped);
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


#include "un260/font/boot_fonts.h"
#include "un260/lv_core/boot_anim/boot_welcome_locale.h"
#include "un260/font/boot_welcome_native.h"
static lv_obj_t *welcome_label(lv_obj_t *p,const char *text){
 if(lv_obj_check_type(p,&lv_label_class)&&!strcmp(lv_label_get_text(p),text))return p;
 for(unsigned i=0;i<lv_obj_get_child_cnt(p);++i){lv_obj_t *o=welcome_label(lv_obj_get_child(p,i),text);if(o)return o;}return NULL;
}
static void handwritten_welcome_locales(void){
 ui_page_t previous=current;current=UI_PAGE_BOOT;
 uint8_t native[500*128*4];
 for(unsigned i=0;i<BOOT_WELCOME_COUNT;++i){
  assert(ui_lang_prepare_resources(boot_welcome_ids[i]));ui_lang_set(boot_welcome_ids[i]);
  lv_font_glyph_dsc_t d={0};
  assert(lv_font_get_glyph_dsc(&lv_font_boot_welcome_compact,&d,0xe000+i,0));
  assert(d.resolved_font==&lv_font_boot_welcome_compact&&d.box_w<=430&&d.box_h<=92);
  const uint8_t *bits=lv_font_get_glyph_bitmap(&lv_font_boot_welcome_compact,0xe000+i);
  assert(bits);boot_welcome_native_apply(native,i);
  unsigned top=128-d.box_h-d.ofs_y,left=(500-d.box_w)/2;
  for(unsigned y=0;y<128;++y)for(unsigned x=0;x<500;++x){
   unsigned alpha=0;
   if(x>=left&&x<left+d.box_w&&y>=top&&y<top+d.box_h){
    unsigned n=(y-top)*d.box_w+x-left;alpha=((n&1)?bits[n/2]&15:bits[n/2]>>4)*17;
   }
   assert(native[(y*500+x)*4+3]==alpha);
  }
  ui_page_08_curr_create(lv_scr_act());ui_page_00_boot_anim_create(lv_layer_top());
  ui_page_00_boot_anim_set_startup_ready(false);ui_page_00_boot_anim_adopt_elapsed(4300);tick(40);
  lv_obj_t *word=welcome_label(lv_layer_top(),boot_welcome_texts[i]);assert(word);
  assert(lv_obj_get_style_text_font(word,0)==&lv_font_boot_welcome_compact);
  assert(lv_obj_get_x(word)==390&&lv_obj_get_y(word)==141);
  assert(lv_obj_get_style_base_dir(word,0)==LV_BASE_DIR_LTR);
  char file[64];snprintf(file,sizeof(file),"handwritten-welcome-%s",boot_welcome_tags[i]);raster(file);
  /* Preferences completion must not replace a visible word or reset opacity. */
  ui_lang_set(LANGUAGE_EN);ui_page_00_boot_anim_set_startup_ready(true);tick(40);
  assert(welcome_label(lv_layer_top(),boot_welcome_texts[i])==word);
  assert(lv_obj_get_style_text_opa(word,0)==LV_OPA_COVER);
  ui_page_00_boot_anim_destroy();ui_page_08_curr_destroy();
 }
 ui_lang_set(LANGUAGE_EN);current=previous;
 puts("PASS ten handwritten welcomes: native/LVGL alpha identity, bounds, direction, snapshot and lifecycle");
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
 const char *bg_path=getenv("UN260_CALCULATOR_BACKGROUND");assert(bg_path);
 FILE *f=fopen(bg_path,"rb");assert(f);assert(fread(background,1,sizeof(background),f)==sizeof(background));fclose(f);
 f=fopen(getenv("UN260_BOOT_BACKGROUND"),"rb");assert(f);assert(fread(boot_background,1,sizeof(boot_background),f)==sizeof(boot_background));fclose(f);
 f=fopen(getenv("UN260_READY_BACKGROUND"),"rb");assert(f);assert(fread(ready_background,1,sizeof(ready_background),f)==sizeof(ready_background));fclose(f);
 f=fopen(getenv("UN260_BOOT_EMBLEM"),"rb");assert(f);assert(fread(boot_emblem,1,sizeof(boot_emblem),f)==sizeof(boot_emblem));fclose(f);
 workspace_defaults(&model);strcpy(model.users[0].name,"Alex Morgan");
 lv_init();ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,draw_buffer,NULL,1280*64);lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=1280;d.ver_res=400;d.flush_cb=flush;d.draw_buf=&buf;lv_disp_t *display=lv_disp_drv_register(&d);assert(display);ui_scrollbar_init(display);
 lv_indev_drv_t in;lv_indev_drv_init(&in);in.type=LV_INDEV_TYPE_POINTER;in.read_cb=read_pointer;assert(lv_indev_drv_register(&in));
 lv_img_decoder_t *decoder=lv_img_decoder_create();lv_img_decoder_set_info_cb(decoder,calculator_asset_info);lv_img_decoder_set_open_cb(decoder,calculator_asset_open);lv_img_decoder_set_close_cb(decoder,asset_close);
 cached_pin_locales();
 handwritten_welcome_locales();
 early_boot_locales();
 current=UI_PAGE_CALCULATOR;ui_calculator_create(lv_scr_act());tick(250);calculator_click("1");calculator_click("2");calculator_click("+");calculator_click("3");calculator_click("=");assert(find(lv_scr_act(),"15"));check_single_line(lv_scr_act());raster("cash-calculator");
 calculator_click("Exchange rate");calculator_click("Import count");assert(find(lv_scr_act(),"1680.00"));check_single_line(lv_scr_act());raster("cash-fx");ui_calculator_destroy();tick(300);
 for(unsigned i=0;i<20;++i){ui_calculator_create(lv_scr_act());tick(20);ui_calculator_destroy();}tick(300);
 lv_obj_t *temporary=lv_obj_create(lv_scr_act());ui_calculator_create(temporary);lv_obj_del(temporary);tick(300);ui_calculator_destroy();
 ui_operators_create(lv_scr_act());tick(250);check_single_line(lv_scr_act());lv_obj_t *badge=find(lv_scr_act(),"A");assert(badge);lv_obj_t *number=lv_obj_get_child(badge,0);assert(lv_obj_get_style_text_align(number,0)==LV_TEXT_ALIGN_CENTER);assert(LV_ABS(lv_obj_get_x(number)*2+lv_obj_get_width(number)-lv_obj_get_width(badge))<=1);assert(LV_ABS(lv_obj_get_y(number)*2+lv_obj_get_height(number)-lv_obj_get_height(badge))<=1);assert(!calculator_button(lv_scr_act(),"Quick"));assert(find(lv_scr_act(),"Applies to all operators"));raster("cash-operators");
 unsigned people_before=model.user_count;char original_name[25];strcpy(original_name,model.users[0].name);
 calculator_click("Add operator");calculator_click("Save");tick(300);calculator_click("Enter name");calculator_click("N");calculator_click("Apply");calculator_click("Save");tick(300);assert(model.user_count==people_before+1&&!strcmp(model.users[0].name,original_name));

 uint32_t disposable;assert(workspace_add_user(&model,"Temporary",&disposable));revision++;tick(300);
 lv_obj_t *person=find(lv_scr_act(),"Temporary");assert(person);lv_event_send(person,LV_EVENT_CLICKED,NULL);tick(100);
 click("Operator details");delete_blocker="Checking operator records...";calculator_click("Delete operator");assert(lv_obj_has_state(calculator_button(lv_scr_act(),"Delete"),LV_STATE_DISABLED));assert(lv_obj_get_style_text_color(lv_obj_get_child(calculator_button(lv_scr_act(),"Delete"),0),0).full==lv_color_hex(0x627583).full);
 delete_blocker="Linked records prevent deletion.";tick(300);assert(find(lv_scr_act(),delete_blocker));raster("operator-delete-linked");calculator_click("Cancel");assert(workspace_find(&model,disposable));
 click("Operator details");delete_blocker=NULL;calculator_click("Delete operator");raster("operator-delete-confirm");require_pin=true;calculator_click("Delete");
 assert(workspace_find(&model,disposable));assert(find(lv_scr_act(),"Management authorization"));
 assert(lv_obj_get_x(find(lv_scr_act(),"Clear"))==486);
 assert(lv_obj_get_style_border_width(find(lv_scr_act(),"1"),0)==0);
 calculator_click("1");calculator_click("1");calculator_click("1");assert(pin_attempts==0);
 calculator_click("1");assert(pin_attempts==1&&workspace_find(&model,disposable));
 assert(find(lv_scr_act(),"Incorrect password. Try again."));raster("operator-pin-invalid");
 calculator_click("2");calculator_click("4");calculator_click("6");calculator_click("8");tick(300);
 assert(pin_attempts==2&&!workspace_find(&model,disposable));require_pin=false;
 ui_operators_destroy();
 model.disabled[1]=1;revision++;ui_operators_create(lv_scr_act());tick(200);
 lv_obj_t *disabled_person=find(lv_scr_act(),"N");assert(disabled_person);lv_event_send(disabled_person,LV_EVENT_CLICKED,NULL);tick(100);
 lv_obj_t *management=find(lv_scr_act(),"Device management");lv_obj_t *enable=lv_obj_get_child(management,4);assert(lv_obj_get_width(enable)==54);lv_event_send(enable,LV_EVENT_CLICKED,NULL);tick(300);assert(model.operators_enabled&&model.active_id==1);
 assert(lv_obj_has_state(calculator_button(lv_scr_act(),"Disabled"),LV_STATE_DISABLED));click("Operator details");delete_blocker="Linked records prevent deletion.";calculator_click("Delete operator");assert(find(lv_scr_act(),"Already disabled. Existing records are retained."));raster("operator-disabled-linked");calculator_click("Cancel");
 model.disabled[1]=0;revision++;tick(300);unsigned switch_notices=operation_starts;assert(model.active_id==1);calculator_click("Switch operator");assert(model.active_id==1&&operation_starts==switch_notices);calculator_click("Confirm switch");assert(model.active_id==model.users[1].id&&operation_started==APP_UI_NOTICE_OPERATOR_SWITCH);raster("operator-switch-confirmed");
 ui_operators_destroy();model.operators_enabled=0;model.active_id=1;revision++;
 for(unsigned i=model.user_count;i<8;++i){char name[25];snprintf(name,sizeof(name),"Operator %u",i+1);assert(workspace_add_user(&model,name,NULL));}revision++;
 ui_operators_create(lv_scr_act());tick(250);raster("operators-eight-scroll");
 lv_obj_t *first_row=find(lv_scr_act(),"Alex Morgan"); /* title can also match, locate by its badge */
 lv_obj_t *initial_badge=find(lv_scr_act(),"A");assert(initial_badge);first_row=lv_obj_get_parent(initial_badge);lv_obj_t *viewport=lv_obj_get_parent(first_row);
 assert(lv_obj_get_width(first_row)<=lv_obj_get_width(viewport)-UI_SCROLLBAR_GUTTER);
 lv_obj_scroll_to_y(viewport,200,LV_ANIM_OFF);tick(100);raster("operators-eight-scrolled");
 lv_coord_t old_scroll=lv_obj_get_scroll_y(viewport);lv_obj_t *row=lv_obj_get_child(viewport,4);lv_event_send(row,LV_EVENT_CLICKED,NULL);tick(100);
 viewport=lv_obj_get_child(lv_obj_get_parent(calculator_button(lv_scr_act(),"Scroll view")),0);assert(lv_obj_get_scroll_y(viewport)==old_scroll);
 calculator_click("Scroll view");calculator_click(">");tick(100);raster("operators-eight-paged");
 ui_operators_destroy();workspace_defaults(&model);revision++;
 lv_alnum_keyboard_config_t space_config={.title="Text search",.placeholder="",.apply_text="Apply",.clear_text="Clear",.cancel_text="Cancel",.max_length=32,.submit=space_submit,.symbols=true};lv_alnum_keyboard_t *space_keyboard=lv_alnum_keyboard_create(lv_scr_act(),&space_config);assert(space_keyboard);lv_alnum_keyboard_show(space_keyboard);tick(80);calculator_click("A");calculator_click("Space");calculator_click("B");raster("cash-text-space-keyboard");calculator_click("Apply");assert(!strcmp(space_result,"A B"));lv_alnum_keyboard_destroy(space_keyboard);
 assert(ui_lang_prepare_resources(LANGUAGE_CN));ui_lang_set(LANGUAGE_CN);ui_calculator_create(lv_scr_act());tick(250);assert(!calculator_button(lv_scr_act(),ui_tr("Quick")));check_single_line(lv_scr_act());raster("cash-calculator-cn");ui_calculator_destroy();ui_operators_create(lv_scr_act());tick(250);check_single_line(lv_scr_act());assert(find(lv_scr_act(),ui_tr("Device workspace")));raster("cash-operators-cn");unsigned ink=0;for(unsigned y=27;y<46;++y)for(unsigned x=1080;x<1250;++x)if(framebuffer[y*1280+x].full==lv_color_hex(0x7358AD).full)++ink;assert(ink>20);ui_operators_destroy();
 for(size_t language=1;language<ui_lang_count();++language){
  const ui_locale_t *locale=ui_lang_at(language);assert(ui_lang_prepare_resources(locale->id));ui_lang_set(locale->id);
  char file[80];
  ui_calculator_create(lv_scr_act());tick(250);check_single_line(lv_scr_act());
  snprintf(file,sizeof(file),"calculator-%s",locale->tag);raster(file);ui_calculator_destroy();
  ui_operators_create(lv_scr_act());tick(250);check_single_line(lv_scr_act());
  snprintf(file,sizeof(file),"operators-%s",locale->tag);raster(file);ui_operators_destroy();
  current=UI_PAGE_MENU;ui_page_03_menu_create(lv_scr_act());tick(150);
  for(unsigned tab=1;tab<6;++tab)for(unsigned sub=0;sub<subcounts[tab];++sub){
   menu.tab=tab;menu.sub=sub;
   menu.dirty=true;tick(120);
   check_single_line(lv_scr_act());
   snprintf(file,sizeof(file),"menu-%s-%u-%u",locale->tag,tab,sub);raster(file);
  }
  ui_page_03_menu_destroy();
 }
 ui_lang_set(LANGUAGE_EN);
 current=UI_PAGE_MENU;ui_page_03_menu_create(lv_scr_act());tick(150);
 check_header();
 ui_page_03_menu_destroy();
 puts("PASS native calculator/operators: arithmetic, read-only imports, FX, operator deletion authorization, numeric PIN geometry, CJK pixels, four-tab Menu and repeated lifecycle");return 0;
}
