#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_components/lv_fault_popup.c"

static lv_color_t pixels[1280*400],buffer[1280*40];
static uint32_t suspended;
static unsigned notice_posts,notice_clears;
static char last_notice_key[48],last_notice_title[192],last_notice_code[48];
static language_t host_language=LANGUAGE_EN;
language_t ui_lang_get(void) {return host_language;}
void smart_island_refresh_summary(void) {}
void ui_notice_set_suspended(uint32_t reason,bool value) {if(value)suspended|=reason;else suspended&=~reason;}
void ui_notice_post(ui_notice_kind_t kind,const char *key,const char *title,const char *detail)
{
    (void)kind;++notice_posts;
    snprintf(last_notice_key,sizeof(last_notice_key),"%s",key);
    snprintf(last_notice_title,sizeof(last_notice_title),"%s",title);
    snprintf(last_notice_code,sizeof(last_notice_code),"%s",detail);
}
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
    /* Queue titles use a separate 16 px face even when the queue is closed. */
    verify_text_font(text(popup.guide.title),ui_message_font(&lv_font_instrument_sans_medium_16));
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
    pause_play(NULL);advance(1000);assert(popup.model.elapsed_ms==elapsed&&popup.model.timer->paused);
    replay(NULL);assert(popup.model.playing&&popup.model.elapsed_ms==0);
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
    fault_popup_report_sensor_mask((1U<<1)|(1U<<23));advance(1000);pause_play(NULL);
    elapsed=popup.model.elapsed_ms;
    fault_popup_report_sensor_mask(1U<<1);
    assert(popup.key.code==1 && !popup.model.playing && popup.model.elapsed_ms==elapsed);
    fault_popup_report_sensor_mask((1U<<1)|(1U<<2));assert(popup.key.code==2 && popup.model.playing);
    fault_popup_report_sensor_mask(0);assert(!fault_popup_is_showing());
    fault_popup_record_runtime_notice(225);assert(fault_popup_show_pending_now());capture("unknown");
    fault_popup_record_boot_result(5,2);toggle_queue(NULL);assert(popup.queue);
    lv_obj_del(popup.overlay);assert(!popup.overlay&&!popup.model.timer&&!suspended);
    assert(!popup.queue);assert(fault_popup_show_pending_now());assert(popup.step==0);hide_fault_popup();
    machine_fault_clear();
    fault_popup_set_auto_enabled(false);
    unsigned posted=notice_posts,cleared=notice_clears;
    fault_popup_report_runtime_fault(2);
    assert(!fault_popup_is_showing() && notice_posts==posted+1 && !strcmp(last_notice_key,"machine.fault"));
    assert(!strcmp(last_notice_title,"Upper passage jam")&&!strcmp(last_notice_code,"0x0F/0x02"));
    fault_popup_report_runtime_fault(2);assert(notice_posts==posted+1);
    fault_popup_clear_runtime();assert(notice_clears==cleared+1);
    fault_popup_report_sensor_mask((1U<<1)|(1U<<23));assert(!fault_popup_is_showing()&&notice_posts==posted+2);
    assert(!strcmp(last_notice_code,"0x02/bit1"));
    fault_popup_report_sensor_mask((1U<<1)|(1U<<23));assert(notice_posts==posted+2);
    fault_popup_set_auto_enabled(true);assert(fault_popup_is_showing()&&notice_clears==cleared+2);
    click_confirm();click_confirm();fault_popup_report_sensor_mask(0);
    fault_popup_set_auto_enabled(false);fault_popup_report_runtime_fault(2);posted=notice_posts;cleared=notice_clears;
    fault_popup_record_runtime_notice(225);assert(notice_posts==posted&&!fault_popup_is_showing()&&notice_clears==cleared+1);
    fault_popup_clear_runtime();fault_popup_set_auto_enabled(true);
    for(unsigned language=0;language<2;++language) {
    host_language=language?LANGUAGE_CN:LANGUAGE_EN;
    for(unsigned source=0;source<4;++source)for(unsigned code=1;code<(source==0?6:source==1?14:source==2?8:32);++code) {
        machine_fault_key_t k={(machine_fault_source_t)source,source==0||source==1?2:0,(uint8_t)code};
        present(k);
        for(unsigned s=0;s<popup.guide.step_count;++s){popup.step=s;render_step();verify_layout();}
        hide_fault_popup();
    }
    }
    fault_popup_report_runtime_fault(2);advance(3200);capture("upper-open-zh");hide_fault_popup();
    assert(lv_obj_get_child_cnt(lv_layer_top())==initial_children);
    advance(1000);assert(!popup.model.timer);
    puts("machine_fault_view: PASS (actual LVGL geometry, active-step loop, acknowledge, dedupe, multi-fault, independent recovery, deletion/recreation)");
    return 0;
}
