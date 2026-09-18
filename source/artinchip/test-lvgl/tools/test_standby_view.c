#include <assert.h>
#include "aic_ui/compiled_asset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/lv_core/page_34_standby.c"
static standby_config_t saved;
static bool (*registered_drag)(void);
static bool (*registered_action)(gesture_action_t);
static bool store_busy, overlay;
static settings_detail_dialog_cb_t confirm_cb;
void gesture_service_set_page_policy(uint32_t owner,bool(*drag)(void),bool(*action)(gesture_action_t)){assert(owner==UI_PAGE_STANDBY_SETTING);registered_drag=drag;registered_action=action;}
void gesture_service_clear_page_policy(uint32_t owner){assert(owner==UI_PAGE_STANDBY_SETTING);registered_drag=NULL;registered_action=NULL;}
void ui_manager_clear_stack(void){}
void ui_manager_switch(ui_page_t p);

void lv_port_indev_set_drag_obj(lv_obj_t*o,bool enable){if(enable)lv_obj_add_flag(o,LV_OBJ_FLAG_USER_4|LV_OBJ_FLAG_PRESS_LOCK);else lv_obj_clear_flag(o,LV_OBJ_FLAG_USER_4|LV_OBJ_FLAG_PRESS_LOCK);}
void standby_defaults(standby_config_t*c){memset(c,0,sizeof(*c));c->version=2;c->minutes=5;const uint32_t palette[]={0x14232D,0xEDF1EC,0x30243C};for(int m=0;m<2;m++)for(int i=0;i<3;i++){c->layout[m][i]=(standby_layout_t){.x=44+i*328,.y=64,.date_x=44+i*328,.date_y=230,.dial_x=i?75:830,.dial_y=50,.auto_text=1,.text_color=m?0xFFFFFF:0x304957,.color=palette[i],.date_bits=14,.greeting=1,.scheduled=1,.photo=1};}}
const standby_config_t*standby_config(void){return &saved;}
bool standby_store_save(const standby_config_t*c){saved=*c;return true;}
bool standby_store_busy(void){return store_busy;}
bool standby_store_poll(char*m,unsigned n){(void)m;(void)n;return false;}
bool standby_store_import(void){return false;}
bool standby_store_delete(unsigned n){(void)n;return false;}
static bool with_imports;
bool standby_photo_exists(unsigned n){return with_imports&&n<6;}
const char*standby_photo_path(unsigned n){const char*paths[]={"L:/usr/local/share/lvgl_data/standby/silver.png","L:/usr/local/share/lvgl_data/standby/celadon.png","L:/usr/local/share/lvgl_data/standby/champagne.png"};return n==STANDBY_PHOTO_MIST?"L:/usr/local/share/lvgl_data/standby/mist.png":paths[n<3?n:1];}
void machine_time_get(machine_time_value_t*t){*t=(machine_time_value_t){2026,9,30,9,41,0};}
bool machine_time_is_valid(const machine_time_value_t*t){return t->year>=2024;}
static ui_page_t current_page=UI_PAGE_MAIN;
void ui_manager_switch(ui_page_t p){current_page=p;}
bool ui_manager_pop_page(void){current_page=UI_PAGE_MAIN;return true;}
void ui_manager_push_page(ui_page_t p){current_page=p;if(p==UI_PAGE_STANDBY)app_standby_runtime_enter();}
ui_page_t ui_manager_get_current_page(void){return current_page;}
bool ui_manager_is_transitioning(void){return false;}
bool settings_detail_dialog_show(const char*a,const char*b,const char*c,const char*d,settings_detail_dialog_cb_t e,settings_detail_dialog_cb_t f,void*g){(void)a;(void)b;(void)c;(void)d;confirm_cb=e;(void)f;(void)g;return true;}
void settings_detail_dialog_hide(void){}
bool settings_detail_overlay_is_open(void){return overlay;}
bool settings_detail_keyboard_show(const char*a,const char*b,uint16_t c,settings_detail_keyboard_mode_t d,settings_detail_keyboard_cb_t e,void*f){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return true;}
void settings_detail_keyboard_hide(void){}
lv_obj_t*settings_detail_create_button(lv_obj_t*p,lv_coord_t x,lv_coord_t y,lv_coord_t w,lv_coord_t h,const char*t,lv_color_t c,lv_event_cb_t cb,void*u){lv_obj_t*b=lv_btn_create(p);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);lv_obj_set_style_bg_color(b,c,0);lv_obj_set_style_bg_opa(b,255,0);lv_obj_t*l=lv_label_create(b);lv_label_set_text(l,t);lv_obj_set_style_text_font(l,&lv_font_instrument_sans_medium_16,0);lv_obj_center(l);lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,u);return b;}
#include "un260/app_service/app_standby_runtime.c"
static bool counting_block, fault_block;
bool lv_upgrade_popup_is_showing(void){return false;}
bool lv_qr_popup_is_showing(void){return false;}
void standby_store_init(void){}
bool app_command_runtime_count_start_busy(void){return counting_block;}
bool machine_state_aging_running(void){return false;}
bool fault_popup_is_showing(void){return fault_block;}
bool fault_popup_get_pending_fault(fault_source_t*a,uint8_t*b,uint8_t*c){(void)a;(void)b;(void)c;return false;}
static void runtime_test(void){
 saved.minutes=1;app_standby_runtime_poll(100);app_standby_runtime_poll(60099);assert(current_page==UI_PAGE_MAIN);
 app_standby_runtime_poll(60100);assert(current_page==UI_PAGE_STANDBY);
 assert(app_standby_runtime_touch(true));app_standby_runtime_poll(60101);assert(current_page==UI_PAGE_MAIN);
 assert(app_standby_runtime_touch(true));assert(app_standby_runtime_touch(false));assert(!app_standby_runtime_touch(false));
 counting_block=true;app_standby_runtime_poll(200000);assert(current_page==UI_PAGE_MAIN);
 counting_block=false;app_standby_runtime_poll(200001);assert(current_page==UI_PAGE_MAIN);
 fault_block=true;app_standby_runtime_poll(400000);assert(current_page==UI_PAGE_MAIN);
 fault_block=false;current_page=UI_PAGE_SETTING;app_standby_runtime_poll(800000);assert(current_page==UI_PAGE_SETTING);
 current_page=UI_PAGE_MAIN;saved.minutes=0;app_standby_runtime_poll(1000000);assert(current_page==UI_PAGE_MAIN);
 /* An idle release packet and an island-like top-layer button are not activity. */
 saved.minutes=1;activity=lv_tick_get();uint32_t base=activity;lv_obj_t*island=lv_btn_create(lv_layer_top());
 lv_tick_inc(30000);assert(!app_standby_runtime_touch(false));assert(activity==base);app_standby_runtime_poll(base+60000);assert(current_page==UI_PAGE_STANDBY);app_standby_runtime_poll(base+60001);assert(current_page==UI_PAGE_STANDBY);lv_obj_del(island);
 current_page=UI_PAGE_MAIN;app_standby_runtime_touch(true);ui_manager_push_page(UI_PAGE_STANDBY);assert(app_standby_runtime_touch(true));assert(!wake);assert(app_standby_runtime_touch(false));assert(!wake);assert(app_standby_runtime_touch(true));assert(wake);app_standby_runtime_touch(false);app_standby_runtime_poll(base+60002);assert(current_page==UI_PAGE_MAIN);
 app_standby_runtime_protocol_activity();base=activity;app_standby_runtime_poll(base+59999);assert(current_page==UI_PAGE_MAIN);app_standby_runtime_poll(base+60000);assert(current_page==UI_PAGE_STANDBY);app_standby_runtime_protocol_activity();app_standby_runtime_poll(base+60001);assert(current_page==UI_PAGE_MAIN);
 puts("PASS idle threshold, busy/fault/settings gates, wake contact capture, Never");
}
static lv_color_t pixels[1280*400],buffer[1280*40];
static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*p){for(int y=a->y1;y<=a->y2;y++)memcpy(pixels+y*1280+a->x1,p+(y-a->y1)*(a->x2-a->x1+1),(a->x2-a->x1+1)*4);lv_disp_flush_ready(d);}
static unsigned char*wallpaper,*gear,*silver,*champagne,*mist,*user_bg;
static lv_res_t info(lv_img_decoder_t*d,const void*src,lv_img_header_t*h){(void)d;if(lv_img_src_get_type(src)!=LV_IMG_SRC_FILE)return LV_RES_INV;memset(h,0,sizeof(*h));const un260_compiled_asset_t*a=un260_compiled_asset_find(src);if(a){h->w=a->width;h->h=a->height;h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;}bool icon=strstr(src,"un260-mark")!=NULL;h->w=icon?24:1280;h->h=icon?28:400;h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;}
static lv_res_t open_image(lv_img_decoder_t*d,lv_img_decoder_dsc_t*s){if(info(d,s->src,&s->header)!=LV_RES_OK)return LV_RES_INV;const un260_compiled_asset_t*a=un260_compiled_asset_find(s->src);if(a){s->img_data=a->pixels;return LV_RES_OK;}s->img_data=strstr(s->src,"standby/mist.png")?mist:strstr(s->src,"backgrounds/user.png")?user_bg:strstr(s->src,"un260-mark")?gear:strstr(s->src,"silver")?silver:strstr(s->src,"champagne")?champagne:wallpaper;return LV_RES_OK;}
static void assert_flat(lv_obj_t*o){assert(lv_obj_get_style_shadow_width(o,LV_PART_MAIN)==0);for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++)assert_flat(lv_obj_get_child(o,i));}
static void snapshot(const char*n){lv_obj_update_layout(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);char path[256];snprintf(path,sizeof(path),"%s/%s.bgra",getenv("OUT"),n);FILE*f=fopen(path,"wb");assert(f);assert(fwrite(pixels,4,1280*400,f)==1280*400);fclose(f);}
static unsigned char*load(const char*n,size_t size){char path[256];snprintf(path,sizeof(path),"%s/%s",getenv("OUT"),n);FILE*f=fopen(path,"rb");assert(f);unsigned char*p=malloc(size);assert(p&&fread(p,1,size,f)==size);fclose(f);return p;}
static void policy_test(void){
 assert(registered_drag&&registered_action);
 standby_config_t old=draft;draft=saved;tab=4;assert(registered_drag());
 assert(!registered_action(GESTURE_ACTION_HOME));
 draft.minutes=saved.minutes+1;confirm_cb=NULL;
 assert(registered_action(GESTURE_ACTION_HOME)&&confirm_cb);
 assert(draft.minutes==saved.minutes+1); /* Keep editing changes no state. */
 current_page=UI_PAGE_STANDBY_SETTING;confirm_cb(NULL);assert(current_page==UI_PAGE_MAIN);
 assert(!registered_action(GESTURE_ACTION_EXPORT)); /* Export never discards draft. */
 overlay=true;assert(registered_action(GESTURE_ACTION_HOME));assert(registered_action(GESTURE_ACTION_EXPORT));overlay=false;
 store_busy=true;assert(registered_action(GESTURE_ACTION_HOME));store_busy=false;
 draft=old;puts("PASS page policy registration, dirty Home confirmation, export, modal/busy guards");
}
static void position_test(void){
 standby_layout_t*p=layout();
 p->scale_percent=75;render();assert(preview.clock_font.percent==75);
 p->scale_percent=100;p->x=44;p->y=64;p->date_x=44;p->date_y=230;p->dial_x=830;p->dial_y=50;render();editor_apply();lv_obj_update_layout(page);
 assert(lv_obj_has_flag(preview.group,LV_OBJ_FLAG_USER_4));assert(lv_obj_has_flag(preview.date_group,LV_OBJ_FLAG_PRESS_LOCK));assert(lv_obj_get_x(preview.time)==20);assert(lv_obj_get_x(preview.date)==20);
 assert(lv_obj_get_width(preview.date_group)==lv_obj_get_width(preview.date)+40);
 assert(lv_obj_get_height(preview.date_group)==lv_obj_get_height(preview.date)+40);
 int dx=p->dial_x,dy=p->dial_y,datex=p->date_x,datey=p->date_y;p->x=200;editor_apply();assert(p->dial_x==dx&&p->dial_y==dy&&p->date_x==datex&&p->date_y==datey);
 p->x=44;p->date_x=44;editor_apply();move_begin(0);p->x=p->dial_x+20;p->y=80;editor_apply();move_finish();assert(p->x==44);assert(strstr(lv_label_get_text(edit_hint),"Not enough room"));p->date_x=450;editor_apply();move_begin(0);p->x=p->dial_x+20;p->y=80;editor_apply();assert(overlap_half(frame_area(0),frame_area(2)));move_finish();assert(p->x>700&&p->dial_x<300);snapshot("position-swapped");
 p->date_bits=0;render();assert(lv_obj_has_flag(preview.date_group,LV_OBJ_FLAG_HIDDEN));p->date_bits=15;
 assert(!overlap_half((lv_area_t){0,0,19,9},(lv_area_t){0,30,19,69}));
 assert(!overlap_half((lv_area_t){0,0,19,9},(lv_area_t){10,0,29,39}));
 assert(overlap_half((lv_area_t){0,0,19,9},(lv_area_t){9,0,28,39}));
 p->auto_text=1;p->color=0xFFFFFF;p->text_color=0xCC3366;assert(text_ink(&draft,p)==0);p->color=0;assert(text_ink(&draft,p)==0xFFFFFF);p->auto_text=0;assert(text_ink(&draft,p)==0xCC3366);
 tab=5;render();snapshot("text-custom");p->auto_text=1;render();snapshot("text-auto");assert(p->text_color==0xCC3366);draft.mode=0;assert(text_ink(&draft,layout())==layout()->text_color);
 puts("PASS content+20 frames, independent date/dial, overlap area threshold, swap, hidden date, automatic/manual colour retention");
}
static void advance_fade(unsigned ms){lv_tick_inc(ms);lv_anim_refr_now();}
static void fade_test(void){
 touching=false;current_page=UI_PAGE_STANDBY;
 ui_page_35_standby_create(lv_scr_act());lv_anim_refr_now();
 assert(lv_obj_get_style_bg_opa(fade_cover,0)==255);snapshot("fade-in-start");
 advance_fade(150);int mid=lv_obj_get_style_bg_opa(fade_cover,0);
 assert(mid>0&&mid<255);snapshot("fade-in-middle");
 advance_fade(150);assert(lv_obj_get_style_bg_opa(fade_cover,0)==0);
 snapshot("fade-in-end");
 assert(app_standby_runtime_touch(true));
 app_standby_runtime_poll(lv_tick_get());assert(current_page==UI_PAGE_STANDBY);
 assert(fade_exiting&&!fade_finished);
 advance_fade(75);snapshot("fade-out-middle");
 app_standby_runtime_poll(lv_tick_get());assert(current_page==UI_PAGE_STANDBY);
 advance_fade(74);assert(!ui_page_35_standby_fade_out());
 advance_fade(1);assert(ui_page_35_standby_fade_out());snapshot("fade-out-end");
 app_standby_runtime_poll(lv_tick_get());assert(current_page==UI_PAGE_MAIN);
 assert(app_standby_runtime_touch(false));ui_page_35_standby_destroy();
 /* Reversal and forced destruction never retain animation callbacks. */
 current_page=UI_PAGE_STANDBY;ui_page_35_standby_create(lv_scr_act());lv_anim_refr_now();
 advance_fade(100);assert(!ui_page_35_standby_fade_out());
 advance_fade(149);assert(!ui_page_35_standby_fade_out());
 ui_page_35_standby_destroy();advance_fade(400);
 assert(!fade_cover&&!full.root&&!clock_timer);
 current_page=UI_PAGE_MAIN;
 puts("PASS 300ms entry / 150ms exit, halfway opacity, deferred wake, reversal and destruction");
}
int main(void){standby_defaults(&saved);lv_init();mist=load("mist.bgra",1280*400*4);user_bg=load("user.bgra",1280*400*4);wallpaper=load("wallpaper.bgra",1280*400*4);gear=load("gear.bgra",24*28*4);lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_drv_register(&dd);lv_img_decoder_t*dec=lv_img_decoder_create();lv_img_decoder_set_info_cb(dec,info);lv_img_decoder_set_open_cb(dec,open_image);
 silver=load("silver.bgra",1280*400*4);champagne=load("champagne.bgra",1280*400*4);
 ui_page_34_standby_create(lv_scr_act());snapshot("settings");assert_flat(page);
 assert(lv_obj_get_style_bg_opa(body,LV_PART_MAIN)==LV_OPA_TRANSP);
 assert(lv_obj_get_child_cnt(body)==2); /* mode selector and settings card only */
 assert(lv_obj_get_y(lv_obj_get_child(body,0))==0);
 notice_visible=true;notice_tick=lv_tick_get();lv_obj_clear_flag(note,LV_OBJ_FLAG_HIDDEN);lv_tick_inc(2499);settings_tick(NULL);assert(!lv_obj_has_flag(note,LV_OBJ_FLAG_HIDDEN));lv_tick_inc(1);settings_tick(NULL);assert(lv_obj_has_flag(note,LV_OBJ_FLAG_HIDDEN));
 tab=2;render();assert(ui_page_34_standby_request_back());assert(tab==0);tab=4;assert(owns_single_drag());tab=0;draft.mode=1;render();snapshot("settings-type");assert_flat(page);
 lv_obj_t*probe=button(page,0,0,150,"Pressed",3,true);lv_obj_add_state(probe,LV_STATE_PRESSED);lv_obj_update_layout(probe);assert(lv_obj_get_style_shadow_width(probe,LV_PART_MAIN)==0);assert(lv_color_to32(lv_obj_get_style_bg_color(probe,LV_PART_MAIN))!=lv_color_to32(lv_color_hex(0x176FE8)));lv_obj_del(probe);
 tab=1;render();snapshot("timeout");assert_flat(page);tab=2;draft.layout[1][0].date_bits=15;render();snapshot("date");assert_flat(page);tab=3;draft.mode=0;render();snapshot("photos");assert_flat(page);assert(lv_obj_get_child_cnt(gallery)==4);lv_event_send(lv_obj_get_child(gallery,3),LV_EVENT_CLICKED,NULL);assert(draft.layout[0][draft.active[0]].photo==STANDBY_PHOTO_MIST);assert(!draft.layout[0][draft.active[0]].scheduled);scene_update(&preview,&draft,true);assert(preview.photo==STANDBY_PHOTO_MIST);render();snapshot("photos-mist");assert(!lv_obj_has_flag(body,LV_OBJ_FLAG_SCROLLABLE));with_imports=true;render();lv_obj_update_layout(page);lv_obj_scroll_to_y(gallery,100,LV_ANIM_OFF);assert(lv_obj_get_scroll_y(gallery)>0);assert(lv_obj_get_scroll_y(body)==0);snapshot("photos-scroll");with_imports=false;draft.mode=1;render();snapshot("palette");tab=4;render();snapshot("position");position_test();policy_test();ui_page_34_standby_destroy();assert(!registered_drag&&!registered_action);
 ui_page_35_standby_create(lv_scr_act());advance_fade(300);snapshot("standby-photo");ui_page_35_standby_destroy();saved.mode=1;ui_page_35_standby_create(lv_scr_act());advance_fade(300);snapshot("standby-type");ui_page_35_standby_destroy();
 for(unsigned i=0;i<10;i++){ui_page_34_standby_create(lv_scr_act());ui_page_34_standby_destroy();}assert(!page&&!settings_timer&&!clock_timer);runtime_test();fade_test();puts("PASS actual LVGL standby view renders and repeated lifecycle");return 0;}
