#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_components/lv_fault_popup.c"
#include "un260/app_service/app_boot_runtime.h"
#include "un260/lv_core/lv_page_manager.h"
#ifdef UN260_FAULT_TEST_LOCALE_BANK
#include "un260/font/ui_locale_font_bank.h"
#endif
static ui_page_t current_page=UI_PAGE_MAIN;
static unsigned navigations;
ui_page_t ui_manager_get_current_page(void){return current_page;}
void ui_manager_switch(ui_page_t page){assert(!fault_popup_is_showing());current_page=page;++navigations;}


static lv_color_t pixels[1280*400],buffer[1280*40];
static uint32_t suspended;
static unsigned notice_posts,notice_clears;
static char last_notice_key[48],last_notice_title[192],last_notice_code[48];
static unsigned island_posts;
static smart_island_warning_level_t island_level;
static char island_title[192];
void smart_island_notify_warning_level(const char *value,smart_island_warning_level_t level)
{island_level=level;++island_posts;snprintf(island_title,sizeof(island_title),"%s",value);}
void smart_island_notify_warning(const char *value){smart_island_notify_warning_level(value,SMART_ISLAND_WARNING_LEVEL_WARNING);}
void smart_island_refresh_summary(void) {}
void smart_island_faults_changed(void) {}
bool smart_island_active_fault_notice(machine_fault_key_t *key) {(void)key;return false;}
void smart_island_cancel_fault_notice(machine_fault_key_t key) {(void)key;}
void smart_island_notify_fault(const char *text,machine_fault_key_t key)
{(void)key;smart_island_notify_warning_level(text,SMART_ISLAND_WARNING_LEVEL_ERROR);}
void smart_island_notify_no_note(machine_fault_key_t key)
{(void)key;smart_island_notify_warning_level("No banknotes detected",SMART_ISLAND_WARNING_LEVEL_WARNING);}
void smart_island_notify_preset_full(machine_fault_key_t key)
{(void)key;smart_island_notify_warning_level("Batch full",SMART_ISLAND_WARNING_LEVEL_PRESET);}
void app_fault_recovery_report(machine_fault_key_t key){(void)key;}
bool app_fault_recovery_request_clear(machine_fault_key_t key){(void)key;return true;}
void ui_notice_set_suspended(uint32_t reason,bool value) {if(value)suspended|=reason;else suspended&=~reason;}
void ui_notice_post(ui_notice_kind_t kind,const char *key,const char *title,const char *detail)
{
    (void)kind;++notice_posts;
    snprintf(last_notice_key,sizeof(last_notice_key),"%s",key);
    snprintf(last_notice_title,sizeof(last_notice_title),"%s",title);
    snprintf(last_notice_code,sizeof(last_notice_code),"%s",detail);
}
void ui_notice_post_text(ui_notice_kind_t kind,const char *key,const char *title,const char *detail)
{ui_notice_post(kind,key,ui_tr(title),ui_tr(detail));}
void ui_notice_post_message(ui_notice_kind_t kind,const char *key,const char *title,const ui_message_t *value)
{char detail[320];ui_message_render(value,detail,sizeof(detail));ui_notice_post(kind,key,ui_tr(title),detail);}
void ui_notice_clear(const char *key) {assert(!strcmp(key,"machine.fault"));++notice_clears;}
void lv_settings_action_style(lv_obj_t *obj,lv_settings_action_role_t role)
{
    lv_obj_set_style_bg_color(obj,lv_color_hex(role==LV_SETTINGS_ACTION_PRIMARY?0x1559B7:0xF4F6F7),0);
    lv_obj_set_style_border_color(obj,lv_color_hex(0xDCE4E8),0);
    lv_obj_set_style_border_width(obj,role==LV_SETTINGS_ACTION_PRIMARY?0:1,0);
}
static void flush(lv_disp_drv_t *driver,const lv_area_t *area,lv_color_t *color)
{
    for(int y=area->y1;y<=area->y2;++y)for(int x=area->x1;x<=area->x2;++x)pixels[y*1280+x]=*color++;
    lv_disp_flush_ready(driver);
}
static void advance(unsigned ms)
{
    while(ms){unsigned step=ms>10?10:ms;lv_tick_inc(step);lv_timer_handler();ms-=step;}
}
static void capture(const char *name)
{
    char path[512];const char *out=getenv("FAULT_OUTPUT");if(!out)out="/tmp/un260-fault-renders";
    snprintf(path,sizeof(path),"%s/%s.bgra",out,name);lv_refr_now(NULL);
    FILE *file=fopen(path,"wb");assert(file);assert(fwrite(pixels,sizeof(pixels),1,file)==1);fclose(file);
}
static lv_obj_t *find_label(lv_obj_t *parent,const char *value)
{
    if(lv_obj_check_type(parent,&lv_label_class)&&!strcmp(lv_label_get_text(parent),value))return parent;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);++i){lv_obj_t *found=find_label(lv_obj_get_child(parent,i),value);if(found)return found;}
    return NULL;
}
static void click_confirm(void)
{
    lv_obj_t *copy=find_label(popup.card,"Confirm");assert(copy);lv_event_send(lv_obj_get_parent(copy),LV_EVENT_CLICKED,NULL);
}
static void verify_text_font(const char *text,const lv_font_t *font)
{
    uint32_t offset=0,codepoint;
    lv_font_glyph_dsc_t glyph;
    while((codepoint=_lv_txt_encoded_next(text,&offset))!=0) {
        if(codepoint<=32)continue;
        if(!lv_font_get_glyph_dsc(font,&glyph,codepoint,0)) {
            fprintf(stderr,"Missing fault glyph U+%04X: %s\n",codepoint,text);
            assert(false);
        }
    }
}
static void verify_label_fonts(lv_obj_t *parent)
{
    if(lv_obj_check_type(parent,&lv_label_class))
        verify_text_font(lv_label_get_text(parent),lv_obj_get_style_text_font(parent,0));
    for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);++i)
        verify_label_fonts(lv_obj_get_child(parent,i));
}
static void verify_layout(void)
{
    lv_obj_update_layout(popup.overlay);
    verify_label_fonts(popup.overlay);
    /* The stage contains only the machine view and its location label. */
    assert(lv_obj_get_child_cnt(lv_obj_get_parent(popup.model.root))==2);
    assert(lv_obj_get_x(popup.card)==36 && lv_obj_get_y(popup.card)==15);
    assert(lv_obj_get_width(popup.model.root)==510&&lv_obj_get_height(popup.model.root)==273);
    assert(lv_obj_get_y(popup.title)+lv_obj_get_height(popup.title)<86);
    assert(lv_obj_get_y(popup.step_title)+lv_obj_get_height(popup.step_title)<lv_obj_get_y(popup.step_body));
    assert(lv_obj_get_y(popup.step_body)+lv_obj_get_height(popup.step_body)<300);
    assert(lv_obj_get_y(popup.location)+lv_obj_get_height(popup.location)<327);
}
int main(void)
{
    static lv_disp_draw_buf_t draw_buffer;static lv_disp_drv_t driver;
    lv_init();lv_disp_draw_buf_init(&draw_buffer,buffer,NULL,1280*40);lv_disp_drv_init(&driver);
    driver.hor_res=1280;driver.ver_res=400;driver.draw_buf=&draw_buffer;driver.flush_cb=flush;assert(lv_disp_drv_register(&driver));
    lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0xF9FAFB),0);
    unsigned initial_children=lv_obj_get_child_cnt(lv_layer_top());
    fault_popup_report_runtime_fault(2);
    assert(popup.step==0&&popup.model.playing&&popup.model.timer&&!popup.model.timer->paused);
    assert(suspended&UI_NOTICE_SUSPEND_FAULT);verify_layout();capture("upper-closed");
    advance(3200);assert(popup.step==0&&popup.model.lid_frame==11);capture("upper-open");
    lv_event_send(popup.steps[1],LV_EVENT_CLICKED,NULL);assert(popup.step==1);advance(1500);capture("upper-remove");
    uint32_t elapsed=popup.model.elapsed_ms;fault_popup_report_runtime_fault(2);assert(popup.step==1&&popup.model.elapsed_ms==elapsed);
    advance(1000);assert(popup.model.elapsed_ms>elapsed&&!popup.model.timer->paused);
    elapsed=popup.model.elapsed_ms;
    lv_obj_t *same_overlay=popup.overlay;
    ui_lang_set(LANGUAGE_CN);fault_popup_language_changed();
    assert(popup.overlay==same_overlay&&popup.step==1&&popup.model.playing&&popup.model.elapsed_ms==elapsed);
    assert(!strcmp(lv_label_get_text(popup.title),"上通道卡钞"));verify_layout();
    ui_lang_set(LANGUAGE_EN);fault_popup_language_changed();
    assert(popup.overlay==same_overlay&&popup.step==1&&popup.model.playing&&popup.model.elapsed_ms==elapsed);
    click_confirm();assert(!fault_popup_is_showing()&&fault_popup_get_pending_fault(NULL,NULL,NULL)&&!suspended);
    fault_popup_report_runtime_fault(2);assert(!fault_popup_is_showing());
    assert(fault_popup_show_pending_now());assert(popup.step==0);click_confirm();
    fault_popup_report_runtime_fault(3);assert(fault_popup_is_showing());advance(3200);capture("lower-open");
    lv_event_send(popup.steps[1],LV_EVENT_CLICKED,NULL);advance(1500);capture("lower-remove");
    fault_popup_clear_runtime();assert(!fault_popup_is_showing()&&!machine_fault_count());
    fault_popup_record_boot_result(5,2);fault_popup_record_boot_result(2,2);assert(!fault_popup_is_showing());
    assert(fault_popup_show_pending_now());capture("image-board");click_confirm();assert(popup.key.code==2);click_confirm();
    fault_popup_report_runtime_fault(2);fault_popup_clear_runtime();assert(machine_fault_count()==2&&!fault_popup_is_showing());
    fault_popup_report_boot_result(5,1);assert(machine_fault_count()==1);fault_popup_report_boot_result(2,1);assert(!machine_fault_count());
    fault_popup_report_sensor_mask((1U<<1)|(1U<<23));assert(machine_fault_count()==2);click_confirm();assert(popup.key.code==23);capture("encoders");click_confirm();
    fault_popup_report_sensor_mask((1U<<1)|(1U<<23));assert(!fault_popup_is_showing());
    fault_popup_report_sensor_mask(0);assert(!machine_fault_count());
    fault_popup_report_sensor_mask((1U<<1)|(1U<<23));advance(1000);
    elapsed=popup.model.elapsed_ms;
    fault_popup_report_sensor_mask(1U<<1);
    assert(popup.key.code==1 && popup.model.playing && popup.model.elapsed_ms==elapsed);
    fault_popup_report_sensor_mask((1U<<1)|(1U<<2));assert(popup.key.code==2 && popup.model.playing);
    fault_popup_report_sensor_mask(0);assert(!fault_popup_is_showing());
    fault_popup_record_runtime_notice(225);assert(fault_popup_show_pending_now());capture("unknown");
    for(unsigned step=1;step<8;++step)fault_popup_record_boot_result(step,2);
    assert(popup.overlay);
    same_overlay=popup.overlay;
    ui_lang_set(LANGUAGE_CN);fault_popup_language_changed();
    assert(popup.overlay==same_overlay);
    ui_lang_set(LANGUAGE_EN);fault_popup_language_changed();
    assert(popup.overlay==same_overlay);
    lv_obj_del(popup.overlay);assert(!popup.overlay&&!popup.model.timer&&!suspended);
    assert(fault_popup_show_pending_now());assert(popup.step==0);hide_fault_popup();
    machine_fault_clear();
    fault_popup_set_auto_enabled(false);
    unsigned posted=island_posts, banners=notice_posts;
    fault_popup_report_runtime_fault(2);
    assert(!fault_popup_is_showing() && island_posts==posted+1);
    assert(!strcmp(island_title,"Upper passage Jam") && notice_posts==banners);
    fault_popup_report_runtime_fault(2);assert(island_posts==posted+2);
    fault_popup_clear_runtime();
    fault_popup_report_sensor_mask((1U<<1)|(1U<<23));assert(!fault_popup_is_showing()&&island_posts==posted+3);
    fault_popup_report_sensor_mask((1U<<1)|(1U<<23));assert(island_posts==posted+3);
    fault_popup_set_auto_enabled(true);
    machine_fault_record_t retained_sensor;
    assert(!fault_popup_is_showing() && island_posts==posted+3 && machine_fault_count()==2);
    assert(machine_fault_first_unread(&retained_sensor) && retained_sensor.key.source==MACHINE_FAULT_SENSOR);
    fault_popup_set_auto_enabled(true); /* Restoring an already-enabled preference is inert. */
    fault_popup_report_sensor_mask((1U<<1)|(1U<<23));
    assert(!fault_popup_is_showing() && island_posts==posted+3);
    assert(fault_popup_show_pending_now()); /* Explicit island inspection remains available. */
    click_confirm();click_confirm();assert(island_posts==posted+4);
    fault_popup_report_sensor_mask((1U<<1)|(1U<<2)|(1U<<23));
    assert(fault_popup_is_showing() && popup.key.code==2 && island_posts==posted+5);
    assert(machine_fault_find((machine_fault_key_t){MACHINE_FAULT_SENSOR,0,1},&retained_sensor) && retained_sensor.acknowledged);
    assert(machine_fault_find((machine_fault_key_t){MACHINE_FAULT_SENSOR,0,2},&retained_sensor) && !retained_sensor.acknowledged);
    same_overlay=popup.overlay;elapsed=popup.model.elapsed_ms;
    fault_popup_set_auto_enabled(false);
    assert(popup.overlay==same_overlay && popup.model.elapsed_ms==elapsed && island_posts==posted+5);
    assert(machine_fault_find((machine_fault_key_t){MACHINE_FAULT_SENSOR,0,2},&retained_sensor) && !retained_sensor.acknowledged);
    fault_popup_set_auto_enabled(true);
    assert(popup.overlay==same_overlay && popup.model.elapsed_ms==elapsed && island_posts==posted+5);
    fault_popup_report_sensor_mask(0);
    fault_popup_set_auto_enabled(false);fault_popup_report_runtime_fault(2);posted=island_posts;
    fault_popup_record_runtime_notice(225);assert(island_posts==posted&&!fault_popup_is_showing());
    fault_popup_clear_runtime();fault_popup_set_auto_enabled(true);
    /* A single Confirm on SELF_TEST restores the legacy sensor route,
     * even with multiple reports. Ordinary faults never inherit that route. */
    fault_popup_set_confirm_handler(app_boot_runtime_confirm_fault);
    current_page=UI_PAGE_BOOT;
    fault_popup_record_boot_result(1,2);fault_popup_record_boot_result(5,2);
    assert(fault_popup_show_pending_now());posted=island_posts;
    click_confirm();
    assert(current_page==UI_PAGE_SENSOR && navigations==1 && !popup.overlay);
    assert(machine_fault_count()==2 && island_posts==posted && !suspended);
    machine_fault_record_t remaining;
    assert(machine_fault_first_unread(&remaining) && remaining.key.code==5);
    confirm(NULL);assert(navigations==1); /* stale click after close is inert */
    machine_fault_clear();
    current_page=UI_PAGE_BOOT;fault_popup_report_sensor_mask(1U<<23);
    click_confirm();assert(current_page==UI_PAGE_SENSOR && navigations==2);
    fault_popup_report_sensor_mask(0);
    current_page=UI_PAGE_BOOT;fault_popup_report_runtime_fault(2);click_confirm();
    assert(current_page==UI_PAGE_BOOT && navigations==2);fault_popup_clear_runtime();
    current_page=UI_PAGE_MAIN;fault_popup_report_boot_result(5,2);click_confirm();
    assert(current_page==UI_PAGE_MAIN && navigations==2);
    machine_fault_clear();fault_popup_set_confirm_handler(NULL);
    fault_popup_set_auto_enabled(true);
    fault_popup_report_start_no_note();assert(fault_popup_is_showing());
    click_confirm();assert(!fault_popup_is_showing()&&island_level==SMART_ISLAND_WARNING_LEVEL_WARNING);
    fault_popup_report_start_no_note();assert(fault_popup_is_showing());
    click_confirm();assert(!fault_popup_is_showing()&&island_level==SMART_ISLAND_WARNING_LEVEL_WARNING);
    fault_popup_set_auto_enabled(false);posted=island_posts;
    fault_popup_report_start_no_note();assert(!fault_popup_is_showing()&&island_posts==posted+1);
    assert(island_level==SMART_ISLAND_WARNING_LEVEL_WARNING);
    assert(fault_popup_show_key((machine_fault_key_t){MACHINE_FAULT_START,1,2}));
    click_confirm();machine_fault_clear();fault_popup_set_auto_enabled(true);
    /* Each new START report reopens an acknowledged door fault; an already
     * visible guide keeps its selected step and animation progress. */
    const machine_fault_key_t door={MACHINE_FAULT_START,2,9};
    posted=island_posts;
    fault_popup_report_start_fault(2,9);
    assert(fault_popup_is_showing() && island_posts==posted+1);
    for(unsigned attempt=0;attempt<3;++attempt) {
        click_confirm();
        machine_fault_record_t record;
        assert(!fault_popup_is_showing() && machine_fault_find(door,&record) && record.acknowledged);
        posted=island_posts;
        fault_popup_report_start_fault(2,9);
        assert(fault_popup_is_showing() && island_posts==posted+1);
        assert(machine_fault_find(door,&record) && !record.acknowledged);
    }
    lv_event_send(popup.steps[1],LV_EVENT_CLICKED,NULL);advance(1500);
    same_overlay=popup.overlay;elapsed=popup.model.elapsed_ms;posted=island_posts;
    fault_popup_report_start_fault(2,9);
    assert(popup.overlay==same_overlay && popup.step==1 && popup.model.elapsed_ms==elapsed);
    assert(island_posts==posted+1);
    click_confirm();
    fault_popup_set_auto_enabled(false);posted=island_posts;
    fault_popup_report_start_fault(2,9);
    assert(!fault_popup_is_showing() && island_posts==posted+1);
    posted=island_posts;
    fault_popup_set_auto_enabled(true);
    machine_fault_record_t retained_door;
    assert(!fault_popup_is_showing() && island_posts==posted && machine_fault_find(door,&retained_door) && !retained_door.acknowledged);
    fault_popup_set_auto_enabled(false);fault_popup_set_auto_enabled(true);
    assert(!fault_popup_is_showing() && island_posts==posted);
    fault_popup_report_start_fault(2,9);
    assert(fault_popup_is_showing() && island_posts==posted+1);
    lv_event_send(popup.steps[1],LV_EVENT_CLICKED,NULL);advance(800);
    same_overlay=popup.overlay;elapsed=popup.model.elapsed_ms;posted=island_posts;
    fault_popup_set_auto_enabled(false);fault_popup_set_auto_enabled(true);
    assert(popup.overlay==same_overlay && popup.step==1 && popup.model.elapsed_ms==elapsed && island_posts==posted);
    assert(machine_fault_find(door,&retained_door) && !retained_door.acknowledged);
    click_confirm();
    assert(machine_fault_find(door,NULL));
    fault_popup_clear_runtime();fault_popup_set_auto_enabled(true);
    for(unsigned language=0;language<2;++language) {
    ui_lang_set(language?LANGUAGE_CN:LANGUAGE_EN);
    for(unsigned source=0;source<4;++source)for(unsigned code=1;code<(source==0?6:source==1?14:source==2?8:32);++code) {
        machine_fault_key_t k={(machine_fault_source_t)source,source==0||source==1?2:0,(uint8_t)code};
        present(k);
        for(unsigned s=0;s<popup.guide.step_count;++s){popup.step=s;render_step(true);verify_layout();}
        hide_fault_popup();
    }
    }
    fault_popup_report_runtime_fault(2);advance(3200);capture("upper-open-zh");hide_fault_popup();
    ui_lang_set(LANGUAGE_EN);
    present((machine_fault_key_t){MACHINE_FAULT_PRESET,0,4});
    assert(!strcmp(lv_label_get_text(popup.title),"Batch full"));
    assert(!strcmp(popup.guide.location.key,"Lower front · Stacker pocket ST"));
    assert(!strcmp(popup.guide.steps[0].title.key,"Empty the stacker pocket"));
    assert(popup.guide.step_count==1 && popup.guide.steps[0].view==MF_FRONT);
    assert(popup.guide.steps[0].zone==MF_STACKER && popup.guide.steps[0].action==MF_REMOVE);
    advance(1500);verify_layout();capture("batch-full-en");hide_fault_popup();
    ui_lang_set(LANGUAGE_CN);
    present((machine_fault_key_t){MACHINE_FAULT_PRESET,0,4});
    assert(!strcmp(lv_label_get_text(popup.title),"预置满"));
    assert(!strcmp(lv_label_get_text(popup.step_title),"取出接钞口钞票"));
    advance(1500);verify_layout();capture("batch-full-zh");hide_fault_popup();
#ifdef UN260_FAULT_TEST_LOCALE_BANK
    /* Opt-in QA links the production bank and verifies every shipped locale. */
    ui_message_font_set_provider(ui_locale_font_compose);
    for(size_t i=0;i<ui_lang_count();++i) {
        const ui_locale_t *locale=ui_lang_at(i);
        static const unsigned sizes[]={12,14,16,18,20,22,24,28};
        for(size_t s=0;s<sizeof(sizes)/sizeof(sizes[0]);++s)
            assert(ui_locale_font_prepare_size(locale->id,sizes[s]));
        ui_lang_set(locale->id);
        present((machine_fault_key_t){MACHINE_FAULT_PRESET,0,4});
        assert(!strcmp(fault_guide_text(popup.guide.title),ui_tr("Batch full")));
        assert(popup.guide.steps[0].zone==MF_STACKER && popup.guide.steps[0].action==MF_REMOVE);
        advance(1500);verify_layout();
        assert(lv_obj_get_height(popup.title)==lv_obj_get_style_text_font(popup.title,0)->line_height);
        char name[64];snprintf(name,sizeof(name),"batch-full-%s",locale->tag);capture(name);
        printf("Batch locale PASS: %s | %s\n",locale->tag,ui_tr("Batch full"));
        hide_fault_popup();
    }
    ui_lang_set(LANGUAGE_CN);
#endif
    present((machine_fault_key_t){MACHINE_FAULT_BOOT,2,1});
    assert(popup.guide.steps[0].view==MF_FRONT);capture("sensor-self-test-front");hide_fault_popup();
    present((machine_fault_key_t){MACHINE_FAULT_BOOT,2,4});
    assert(popup.guide.steps[0].view==MF_FRONT);hide_fault_popup();
    assert(lv_obj_get_child_cnt(lv_layer_top())==initial_children);
    advance(1000);assert(!popup.model.timer);
    puts("machine_fault_view: PASS (Popup preference never opens old START/sensor reports, new reports still open, toggles preserve live guide/read status, repeated START after Confirm, actual LVGL geometry, multi-fault and lifecycle)");
    return 0;
}
