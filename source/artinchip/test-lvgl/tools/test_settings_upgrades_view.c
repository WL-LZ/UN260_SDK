#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_core/page_14_main_upgrade.h"
#include "un260/lv_core/page_15_image_upgrade.h"
#include "un260/lv_core/page_16_ui_upgrade.h"
#include "un260/lv_core/ui_upgrade_service.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_core/lv_page_manager.h"
#include "test_settings_actions.h"
#include "un260/gesture/gesture_service.h"
#include "un260/app_service/upgrade_session.h"
#include "tools/test_page_background_asset.h"
#include "actual_upgrade_pointer.h"

static unsigned sends, pops, homes, starts, resets, results, detects;
static uint8_t sent_cmd;
static bool transport_ready = true, send_ok = true, popup;
static bool (*policy)(gesture_action_t);
static settings_detail_dialog_cb_t confirm;
static void *confirm_data;
static ui_upgrade_service_status_t actual;
static ui_upgrade_detect_info_t detected;
static ui_upgrade_start_result_t start_result;
bool protocol_send_is_ready(void) { return transport_ready; }
int protocol_send(uint8_t cmd, const uint8_t *data, uint16_t length) {
    assert(length == 1 && data[0] == 1); sends++; sent_cmd = cmd; return send_ok ? 6 : -1;
}
bool ui_manager_pop_page(void) { pops++; return true; }
void ui_manager_switch(ui_page_t p) { assert(p == UI_PAGE_MAIN); homes++; }
void ui_manager_clear_stack(void) {}
bool device_info_is_valid(void) { return true; }
const char *device_info_main_app(void) { return "2.6.19"; }
const char *device_info_image_app(void) { return "1.4.8"; }
const char *device_info_display_app(void) { return "1.0.0"; }
void gesture_service_set_page_policy(uint32_t owner, bool(*drag)(void), bool(*handler)(gesture_action_t)) {
    (void)owner; (void)drag; policy = handler;
}
void gesture_service_clear_page_policy(uint32_t owner) { (void)owner; policy = NULL; }
bool settings_detail_overlay_is_open(void) { return confirm != NULL; }
void settings_detail_dialog_hide(void) { confirm = NULL; confirm_data = NULL; }
bool settings_detail_dialog_show_ex(settings_detail_dialog_kind_t kind, const char *title,
    const char *content, const char *yes, const char *no, settings_detail_dialog_cb_t cb,
    settings_detail_dialog_cb_t cancel, void *data) {
    assert(kind == SETTINGS_DIALOG_WARNING); assert(strstr(content, "does not stop"));
    (void)title; (void)yes; (void)no; (void)cancel; confirm = cb; confirm_data = data; return true;
}
void ui_upgrade_service_reset(void) { assert(!actual.running); resets++; memset(&actual, 0, sizeof(actual)); }
void ui_upgrade_service_detect(ui_upgrade_detect_info_t *info) { detects++; *info = detected; }
void ui_upgrade_service_poll(ui_upgrade_service_status_t *status) { *status = actual; }
ui_upgrade_start_result_t ui_upgrade_service_start(void) {
    starts++;
    if (start_result == UI_UPGRADE_START_OK) {
        if (!upgrade_session_begin(UPGRADE_SESSION_UI)) return UI_UPGRADE_START_BUSY;
        actual.running = true; actual.stage = UI_UPGRADE_STAGE_PREPARE;
        strcpy(actual.step_text, "Checking update package");
    }
    return start_result;
}
void lv_upgrade_popup_show_result(bool success, const char *description) {
    (void)success; (void)description; results++; popup = true;
}
bool lv_upgrade_popup_is_showing(void) { return popup; }
const char *ui_text_get(ui_text_id_t id) { (void)id; return "Back"; }

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
static void remote_update_tests(void) {
    ui_page_14_main_upgrade_create(lv_scr_act());snapshot("controller-ready");
    transport_ready=false;click("Start update");assert(sends==0);transport_ready=true;
    send_ok=false;click("Start update");assert(sends==1);send_ok=true;
    click("Start update");assert(sent_cmd==0xA1&&sends==2&&policy(GESTURE_ACTION_HOME));
    assert(action_blocked(button("Back")));
    click("Start update");click("Back");assert(sends==2&&pops==0);
    tick(19000);ui_page_14_main_upgrade_on_reply(0xA1,2);tick(19000);
    assert(!find(lv_scr_act(),"Result not received"));snapshot("controller-installing");
    tick(1200);assert(find(lv_scr_act(),"Result not received"));snapshot("controller-timeout");
    assert(policy(GESTURE_ACTION_HOME)&&confirm);assert(!homes);
    settings_detail_dialog_cb_t cb=confirm;void *data=confirm_data;settings_detail_dialog_hide();cb(data);assert(homes==1);
    click("Start update");assert(sends==2);
    ui_page_14_main_upgrade_destroy();
    assert(upgrade_session_owner()==UPGRADE_SESSION_CONTROLLER);
    ui_page_15_image_upgrade_create(lv_scr_act());assert(find(lv_scr_act(),"Another update is active"));
    click("Start update");assert(sends==2);ui_page_15_image_upgrade_destroy();
    ui_page_16_ui_upgrade_create(lv_scr_act());assert(find(lv_scr_act(),"Another update is active"));
    click("Start update");assert(starts==0);ui_page_16_ui_upgrade_destroy();
    ui_page_14_main_upgrade_on_reply(0xA1,3);assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);
    ui_page_14_main_upgrade_create(lv_scr_act());assert(find(lv_scr_act(),"Update complete"));
    assert(!action_blocked(button("Start update")));snapshot("controller-complete");
    click("Start update");ui_page_14_main_upgrade_on_reply(0xA1,0xF2);
    assert(find(lv_scr_act(),"Update not completed"));snapshot("controller-file-mismatch");
    assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);
    click("Start update");ui_page_14_main_upgrade_on_reply(0xA1,0xF1);assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);
    click("Start update");ui_page_14_main_upgrade_on_reply(0xA1,0xF3);assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);
    ui_page_14_main_upgrade_destroy();
    ui_page_15_image_upgrade_create(lv_scr_act());click("Start update");assert(sent_cmd==0xB0);
    tick(39900);assert(!find(lv_scr_act(),"Result not received"));tick(300);assert(find(lv_scr_act(),"Result not received"));
    ui_page_15_image_upgrade_on_reply(0xB0,2);assert(find(lv_scr_act(),"Installing update"));
    ui_page_15_image_upgrade_on_reply(0xB0,4);assert(find(lv_scr_act(),"Update complete"));snapshot("image-update-complete");
    ui_page_15_image_upgrade_destroy();assert(!policy);
    puts("PASS remote upgrade ACKs, 20s/40s refresh, duplicate guard, unknown-result leave, hidden late result and lifecycle");
}
static void ui_update_tests(void) {
    ui_page_16_ui_upgrade_create(lv_scr_act());snapshot("ui-no-usb");assert(action_blocked(button("Start update")));
    click("Start update");assert(!starts);
    detected=(ui_upgrade_detect_info_t){true,true,true,UI_UPGRADE_PACKAGE_HASH_DIFFERENT};
    tick(1100);snapshot("ui-ready");
    start_result=UI_UPGRADE_START_SCRIPT_NOT_FOUND;click("Start update");
    assert(find(lv_scr_act(),"Update could not start"));assert(resets==1);snapshot("ui-start-failed");
    start_result=UI_UPGRADE_START_OK;click("Start update");assert(starts==2&&actual.running&&policy(GESTURE_ACTION_HOME));
    assert(find(lv_scr_act(),"0%"));click("Start update");assert(starts==2);unsigned before_detects=detects;
    actual.progress=37;actual.stage=UI_UPGRADE_STAGE_INSTALL;strcpy(actual.step_text,"Installing application and resources");
    tick(220);assert(find(lv_scr_act(),"37%"));snapshot("ui-installing");
    tick(1600);assert(find(lv_scr_act(),"37%")&&detects==before_detects);
    unsigned before_resets=resets;ui_page_16_ui_upgrade_destroy();assert(resets==before_resets);
    ui_page_14_main_upgrade_create(lv_scr_act());assert(find(lv_scr_act(),"Another update is active"));
    unsigned before_sends=sends;click("Start update");assert(sends==before_sends);ui_page_14_main_upgrade_destroy();
    ui_page_16_ui_upgrade_create(lv_scr_act());assert(find(lv_scr_act(),"37%")&&policy(GESTURE_ACTION_HOME));
    actual.running=false;actual.finished=true;actual.success=true;actual.progress=100;actual.stage=UI_UPGRADE_STAGE_SUCCESS;
    upgrade_session_end(UPGRADE_SESSION_UI);
    strcpy(actual.result_text,"UI update installed. Restart to apply it.");tick(220);
    assert(results==1&&find(lv_scr_act(),"100%"));snapshot("ui-complete");popup=false;
    ui_upgrade_service_reset();tick(600);assert(results==1&&find(lv_scr_act(),"100%"));
    ui_page_16_ui_upgrade_destroy();ui_page_16_ui_upgrade_create(lv_scr_act());assert(results==1);
    /* An updater-monitor failure holds the UI owner without presenting success. */
    assert(upgrade_session_begin(UPGRADE_SESSION_UI));
    actual=(ui_upgrade_service_status_t){.finished=true,.success=false,.progress=100,.stage=UI_UPGRADE_STAGE_FAIL};
    strcpy(actual.result_text,"The updater process result could not be confirmed. Keep power connected.");
    tick(220);assert(find(lv_scr_act(),"Update result unknown")&&results==1);
    assert(lv_obj_has_flag(find(lv_scr_act(),"100%"),LV_OBJ_FLAG_HIDDEN));
    assert(policy(GESTURE_ACTION_HOME)&&confirm);settings_detail_dialog_hide();
    snapshot("ui-result-unknown");unsigned before_starts=starts;click("Start update");assert(starts==before_starts);
    ui_page_16_ui_upgrade_destroy();assert(!policy&&upgrade_session_owner()==UPGRADE_SESSION_UI);
    ui_upgrade_service_reset();ui_page_16_ui_upgrade_create(lv_scr_act());
    assert(find(lv_scr_act(),"Update result unknown"));ui_page_16_ui_upgrade_destroy();
    upgrade_session_end(UPGRADE_SESSION_UI);
    puts("PASS UI preflight, start errors, actual-only progress, busy guards, no hidden polling/reset and one result per run");
}
int main(void) {
    lv_init();lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);
    lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_drv_register(&dd);
    lv_img_decoder_t *decoder=lv_img_decoder_create();lv_img_decoder_set_info_cb(decoder,image_info);lv_img_decoder_set_open_cb(decoder,image_open);
    remote_update_tests();ui_update_tests();puts("PASS actual LVGL upgrade pages");return 0;
}
