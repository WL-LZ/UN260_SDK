
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_core/page_37_touch_calibration.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/app_service/touch_calibration_service.h"
#include "un260/gesture/gesture_service.h"
#include "tools/test_page_background_asset.h"
#include "test_settings_actions.h"
#include "actual_upgrade_pointer.h"
static touch_calibration_snapshot_t actual;
static unsigned begins, saves, cancels, pops, toggles;
static bool counting;
static bool (*policy)(gesture_action_t);
uint32_t app_clock_uptime_ms(void){return lv_tick_get();}
bool app_command_runtime_count_start_busy(void){return counting;}
void touch_calibration_poll(uint32_t now){(void)now;}
void touch_calibration_get_snapshot(touch_calibration_snapshot_t *s){*s=actual;}
bool touch_calibration_begin(unsigned w,unsigned h,uint32_t now){
    (void)now;assert(w==1280&&h==400);begins++;
    actual.phase=TOUCH_CAL_COLLECT;actual.target_index=0;actual.target_count=5;
    actual.target_x=64;actual.target_y=96;actual.remaining_ms=30000;return true;
}
bool touch_calibration_cancel(void){
    if(actual.phase==TOUCH_CAL_SAVING)return false;
    cancels++;actual.phase=TOUCH_CAL_IDLE;actual.error=TOUCH_CAL_ERROR_NONE;return true;
}
bool touch_calibration_can_save(void){return actual.phase==TOUCH_CAL_READY||(actual.phase==TOUCH_CAL_FAILED&&actual.error==TOUCH_CAL_ERROR_STORE);}
bool touch_calibration_save(void){saves++;assert(touch_calibration_can_save());actual.phase=TOUCH_CAL_SAVING;return true;}
bool touch_calibration_set_enabled(bool enabled){
    assert(actual.calibrated);toggles++;(void)enabled;actual.phase=TOUCH_CAL_SAVING;return true;
}
bool ui_manager_pop_page(void){pops++;return true;}
void ui_manager_switch(ui_page_t page){(void)page;}
void gesture_service_set_page_policy(uint32_t owner,bool(*drag)(void),bool(*handler)(gesture_action_t)){
    assert(owner==UI_PAGE_TOUCH_CALIBRATION);(void)drag;policy=handler;
}
void gesture_service_clear_page_policy(uint32_t owner){assert(owner==UI_PAGE_TOUCH_CALIBRATION);policy=NULL;}
static lv_obj_t *find_toggle(lv_obj_t *o){
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_CLICKABLE)&&lv_obj_get_style_width(o,0)==54&&lv_obj_get_style_height(o,0)==30)return o;
    for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++){lv_obj_t *r=find_toggle(lv_obj_get_child(o,i));if(r)return r;}return NULL;
}
static lv_color_t pixels[1280*400], buffer[1280*40];
static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *color) {
    for (int y=area->y1;y<=area->y2;y++)
        memcpy(pixels+y*1280+area->x1,color+(y-area->y1)*(area->x2-area->x1+1),(area->x2-area->x1+1)*4);
    lv_disp_flush_ready(driver);
}
static lv_res_t image_info(lv_img_decoder_t *decoder,const void *source,lv_img_header_t *header) {
    (void)decoder;if(lv_img_src_get_type(source)!=LV_IMG_SRC_FILE)return LV_RES_INV;
    const un260_compiled_asset_t *asset=test_page_asset_find(source);
    if(!asset){fprintf(stderr,"Missing asset: %s\n",(const char*)source);abort();}
    memset(header,0,sizeof(*header));header->w=asset->width;header->h=asset->height;
    header->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;
}
static lv_res_t image_open(lv_img_decoder_t *decoder,lv_img_decoder_dsc_t *source) {
    if(image_info(decoder,source->src,&source->header)!=LV_RES_OK)return LV_RES_INV;
    source->img_data=test_page_asset_find(source->src)->pixels;return LV_RES_OK;
}
static void snapshot(const char *name) {
    lv_obj_update_layout(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);
    char path[512];snprintf(path,sizeof(path),"%s/%s.bgra",getenv("OUT"),name);
    FILE *file=fopen(path,"wb");assert(file);assert(fwrite(pixels,4,1280*400,file)==1280*400);fclose(file);
}
static lv_obj_t *find(lv_obj_t *obj,const char *text) {
    if(lv_obj_check_type(obj,&lv_label_class)&&!strcmp(lv_label_get_text(obj),text))return obj;
    for(unsigned i=0;i<lv_obj_get_child_cnt(obj);i++){lv_obj_t *result=find(lv_obj_get_child(obj,i),text);if(result)return result;}
    return NULL;
}
static lv_obj_t *button(const char *text) {
    lv_obj_t *obj=find(lv_scr_act(),text);assert(obj);
    while(obj&&!lv_obj_has_flag(obj,LV_OBJ_FLAG_CLICKABLE))obj=lv_obj_get_parent(obj);
    assert(obj);return obj;
}
static void click(const char *text) { lv_event_send(button(text),LV_EVENT_CLICKED,NULL); }
static void tick(unsigned ms) {for(unsigned i=0;i<ms;i+=20){lv_tick_inc(20);lv_timer_handler();}}

static void calibration_view_tests(void){
    actual=(touch_calibration_snapshot_t){.available=true};
    ui_page_37_touch_calibration_create(lv_scr_act());snapshot("touch-ready");
    assert(action_blocked(button("Save")));
    click("Save");assert(!saves);
    lv_obj_t *toggle=find_toggle(lv_scr_act());assert(toggle&&action_blocked(toggle));
    counting=true;tick(140);click("Calibrate");assert(!begins);counting=false;
    tick(140);click("Calibrate");assert(begins==1&&policy(GESTURE_ACTION_HOME));snapshot("touch-point-1");
    assert(find(lv_scr_act(),"Calibration point 1 / 5"));
    actual.target_index=4;actual.target_x=640;actual.target_y=200;tick(140);snapshot("touch-point-5");
    actual.error=TOUCH_CAL_ERROR_MULTITOUCH;tick(140);assert(find(lv_scr_act(),"Use one finger. Lift all fingers before trying again."));
    click("Cancel");assert(actual.phase==TOUCH_CAL_IDLE&&cancels==1);assert(!policy(GESTURE_ACTION_HOME));
    click("Calibrate");actual.phase=TOUCH_CAL_VALIDATE;actual.error=TOUCH_CAL_ERROR_NONE;
    actual.target_index=1;actual.target_count=3;actual.target_x=960;actual.target_y=259;
    tick(140);assert(find(lv_scr_act(),"Accuracy check 2 / 3"));snapshot("touch-accuracy");
    actual.phase=TOUCH_CAL_READY;tick(140);snapshot("touch-verified");
    assert(!action_blocked(button("Save"))&&policy(GESTURE_ACTION_HOME));
    click("Save");assert(saves==1&&action_blocked(button("Back")));click("Back");assert(!pops);snapshot("touch-saving");
    actual.phase=TOUCH_CAL_FAILED;actual.error=TOUCH_CAL_ERROR_STORE;tick(140);snapshot("touch-save-failed");
    assert(!action_blocked(button("Back")));assert(find(lv_scr_act(),"Factory touch coordinates" )==NULL);
    assert(!action_blocked(button("Save")));click("Save");assert(saves==2);
    actual.phase=TOUCH_CAL_SAVED;actual.calibrated=true;actual.enabled=true;tick(140);snapshot("touch-saved");
    assert(!action_blocked(toggle));lv_event_send(toggle,LV_EVENT_CLICKED,NULL);assert(toggles==1&&actual.enabled);
    actual.phase=TOUCH_CAL_SAVED;actual.enabled=false;tick(140);snapshot("touch-disabled");
    ui_page_37_touch_calibration_destroy();assert(!policy);
    ui_page_37_touch_calibration_create(lv_scr_act());assert(find(lv_scr_act(),"Calibration disabled"));
    actual.load_error=TOUCH_CAL_ERROR_STORE;actual.calibrated=false;tick(140);snapshot("touch-corrupt-fallback");
    click("Back");assert(pops==1);ui_page_37_touch_calibration_destroy();
    actual=(touch_calibration_snapshot_t){0};ui_page_37_touch_calibration_create(lv_scr_act());
    assert(action_blocked(button("Calibrate")));ui_page_37_touch_calibration_destroy();
    puts("PASS real LVGL calibration page: operation guards, all stages, cancel/save/error, persistent enable, re-create and unavailable input");
}
int main(void){
    lv_init();lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);
    lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_drv_register(&dd);
    lv_img_decoder_t *decoder=lv_img_decoder_create();lv_img_decoder_set_info_cb(decoder,image_info);lv_img_decoder_set_open_cb(decoder,image_open);
    calibration_view_tests();return 0;
}
