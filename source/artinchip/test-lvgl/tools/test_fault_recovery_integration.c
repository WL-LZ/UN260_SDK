/* Real LVGL, fault popup, island and recovery service; capture UART only. */
#include "lvgl/lvgl.h"
#define HOST_REAL_FAULT
#define HOST_REAL_NOTICE
#define HOST_ISLAND_ONLY
#define main main_raster_entry_unused
#include "test_main_view.c"
#undef main
#define label notice_test_label
#define present notice_test_present
#include "un260/lv_components/ui_notice.c"
#undef label
#undef present
#define box fault_test_box
#define label fault_test_label
#include "un260/lv_components/lv_fault_popup.c"
#undef box
#undef label
#include "un260/app_service/app_fault_recovery.h"

/* The runner links --wrap app_clock_uptime_ms so service time advances with LVGL. */
uint32_t __wrap_app_clock_uptime_ms(void) {return lv_tick_get();}

static void fault(uint8_t code)
{
    app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_START,2,code});
    fault_popup_report_start_fault(2,code);
}
static void test_independent_switches(void)
{
    const machine_fault_key_t door={MACHINE_FAULT_START,2,9};
    for(unsigned top=0;top<2;++top)for(unsigned automatic=0;automatic<2;++automatic) {
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        ui_notice_deinit();ui_notice_set_enabled(top!=0);fault_popup_set_auto_enabled(automatic!=0);
        unsigned tx=protocol_calls;
        ui_notice_post_text(UI_NOTICE_SUCCESS,"menu.settings","Confirm","Saved");
        assert(ui_notice_is_visible()==(top!=0));
        assert(notice.state.has_active==(top!=0));
        fault(9);
        assert(fault_popup_get_auto_enabled()==(automatic!=0) && ui_notice_get_enabled()==(top!=0));
        assert(fault_popup_is_showing()==(automatic!=0));
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING && g_si_ctx.warning.marquee_running);
        assert(g_si_ctx.warning.fault.valid && g_si_ctx.warning.fault.source==MACHINE_FAULT_START && g_si_ctx.warning.fault.code==9);
        assert(protocol_calls==tx);
        /* A machine modal may cover the banner; the switch never blocks a report. */
        assert(ui_notice_is_visible()==(top && !automatic));
        assert(notice.state.has_active==(top!=0));
        tick(1500);
        lv_obj_t *same_overlay=popup.overlay;
        lv_coord_t island_x=lv_obj_get_x(g_si_ctx.objects.title);
        if(automatic)lv_event_send(popup.steps[1],LV_EVENT_CLICKED,NULL);
        uint32_t model_elapsed=popup.model.elapsed_ms;
        unsigned active_step=popup.step;
        fault(9);
        assert(popup.overlay==same_overlay && popup.step==active_step);
        assert(popup.model.elapsed_ms==model_elapsed);
        assert(lv_obj_get_x(g_si_ctx.objects.title)==island_x && protocol_calls==tx);
        ui_notice_set_enabled(false);
        assert(!ui_notice_is_visible() && !notice.state.has_active && !notice.state.queued_count);
        assert(fault_popup_get_auto_enabled()==(automatic!=0) && popup.overlay==same_overlay);
        assert(g_si_ctx.warning.fault.valid && g_si_ctx.warning.marquee_running);
        assert(lv_obj_get_x(g_si_ctx.objects.title)==island_x && protocol_calls==tx);
        tick(18000);app_fault_recovery_poll();
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE && protocol_calls==tx);
        assert(fault_popup_is_showing()==(automatic!=0) && machine_fault_find(door,NULL));
        ui_notice_set_enabled(top!=0);
        assert(!ui_notice_is_visible()); /* Enabling cannot replay old menu results. */
        for(unsigned attempt=0;attempt<3;++attempt) {
            if(!fault_popup_is_showing())assert(fault_popup_show_key(door));
            confirm(NULL);
            machine_fault_record_t record;
            assert(!fault_popup_is_showing() && machine_fault_find(door,&record) && record.acknowledged);
            assert(protocol_calls==tx); /* Confirm is read acknowledgement, never 0x3D. */
            fault(9);
            assert(machine_fault_find(door,&record) && !record.acknowledged);
            assert(fault_popup_is_showing()==(automatic!=0) && protocol_calls==tx);
            assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING && g_si_ctx.warning.fault.valid);
        }
        if(fault_popup_is_showing())confirm(NULL);
        ui_notice_post_text(UI_NOTICE_SUCCESS,"menu.settings","Confirm","Saved");
        assert(ui_notice_is_visible()==(top!=0) && protocol_calls==tx);
        assert(machine_fault_find(door,NULL)); /* A menu result never clears a physical fault. */
    }
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    ui_notice_deinit();ui_notice_set_enabled(true);fault_popup_set_auto_enabled(false);
}
static void test_recovery_switch_independence(void)
{
    const machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};
    for(unsigned top=0;top<2;++top)for(unsigned automatic=0;automatic<2;++automatic) {
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        ui_notice_deinit();ui_notice_set_enabled(top!=0);fault_popup_set_auto_enabled(automatic!=0);
        unsigned tx=protocol_calls;
        app_fault_recovery_report(jam);fault_popup_report_runtime_fault(2);
        lv_obj_t *same_overlay=popup.overlay;
        ui_notice_set_enabled(top==0);
        assert(popup.overlay==same_overlay && machine_fault_find(jam,NULL) && protocol_calls==tx);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING && g_si_ctx.warning.marquee_running);
        tick(18000);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
        assert(protocol_calls==tx+(automatic?0:1));
        if(automatic)confirm(NULL);
        assert(protocol_calls==tx+1 && machine_fault_find(jam,NULL));
        /* A preference change cannot dispatch or acknowledge an in-flight clear. */
        fault_popup_set_auto_enabled(!automatic);fault_popup_set_auto_enabled(automatic!=0);
        assert(protocol_calls==tx+1 && !fault_popup_is_showing() && machine_fault_find(jam,NULL));
        app_fault_recovery_handle_clear_result(2);
        assert(machine_fault_find(jam,NULL) && fault_popup_is_showing()==(automatic!=0));
        same_overlay=popup.overlay;
        fault_popup_set_auto_enabled(!automatic);fault_popup_set_auto_enabled(automatic!=0);
        assert(popup.overlay==same_overlay && protocol_calls==tx+1 && machine_fault_find(jam,NULL));
        ui_notice_set_enabled(top!=0);
        if(automatic)confirm(NULL);else tick(18000);
        assert(protocol_calls==tx+2 && machine_fault_find(jam,NULL));
        app_fault_recovery_handle_clear_result(1);
        assert(!machine_fault_find(jam,NULL) && !fault_popup_is_showing());
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE && protocol_calls==tx+2);
    }
    ui_notice_deinit();ui_notice_set_enabled(true);fault_popup_set_auto_enabled(false);
    app_fault_recovery_clear();
}
static void test_popup_preference_only(void)
{
    const machine_fault_key_t door={MACHINE_FAULT_START,2,9};
    for(unsigned top=0;top<2;++top) {
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        ui_notice_deinit();ui_notice_set_enabled(top!=0);fault_popup_set_auto_enabled(false);
        unsigned tx=protocol_calls;
        fault(9);tick(1500);
        lv_coord_t island_x=lv_obj_get_x(g_si_ctx.objects.title);
        machine_fault_record_t record;
        assert(g_si_ctx.warning.marquee_running && machine_fault_find(door,&record) && !record.acknowledged);
        fault_popup_set_auto_enabled(true);
        assert(!fault_popup_is_showing() && protocol_calls==tx && ui_notice_get_enabled()==(top!=0));
        assert(lv_obj_get_x(g_si_ctx.objects.title)==island_x && g_si_ctx.warning.marquee_running);
        assert(machine_fault_find(door,&record) && !record.acknowledged);
        tick(18000);app_fault_recovery_poll();
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE && !fault_popup_is_showing() && protocol_calls==tx);
        fault_popup_set_auto_enabled(false);fault_popup_set_auto_enabled(true);
        fault_popup_set_auto_enabled(true); /* The same setter restores persisted ON during initialization. */
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE && !fault_popup_is_showing() && protocol_calls==tx);
        assert(machine_fault_find(door,&record) && !record.acknowledged);
        fault(9);assert(fault_popup_is_showing()); /* Only a new controller START report opens it. */
        lv_event_send(popup.steps[1],LV_EVENT_CLICKED,NULL);tick(800);
        lv_obj_t *same_overlay=popup.overlay;
        uint32_t elapsed=popup.model.elapsed_ms;
        island_x=lv_obj_get_x(g_si_ctx.objects.title);
        fault_popup_set_auto_enabled(false);
        assert(popup.overlay==same_overlay && popup.step==1 && popup.model.elapsed_ms==elapsed);
        assert(lv_obj_get_x(g_si_ctx.objects.title)==island_x && protocol_calls==tx);
        assert(machine_fault_find(door,&record) && !record.acknowledged);
        fault_popup_set_auto_enabled(true);
        assert(popup.overlay==same_overlay && popup.step==1 && popup.model.elapsed_ms==elapsed && protocol_calls==tx);
        confirm(NULL);
        assert(!fault_popup_is_showing() && machine_fault_find(door,&record) && record.acknowledged && protocol_calls==tx);
        fault_popup_set_auto_enabled(false);fault_popup_set_auto_enabled(true);
        assert(!fault_popup_is_showing() && machine_fault_find(door,&record) && record.acknowledged && protocol_calls==tx);
        fault(9);assert(fault_popup_is_showing() && machine_fault_find(door,&record) && !record.acknowledged);
    }
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    ui_notice_deinit();ui_notice_set_enabled(true);fault_popup_set_auto_enabled(false);
}
static void test_late_recovery_results(void)
{
    const machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};
    for(unsigned automatic=0;automatic<2;++automatic)for(unsigned outcome=0;outcome<3;++outcome) {
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        fault_popup_set_auto_enabled(automatic!=0);
        unsigned tx=protocol_calls;
        app_fault_recovery_report(jam);fault_popup_report_runtime_fault(2);tick(1500);
        lv_obj_t *same_overlay=popup.overlay;
        fault_popup_set_auto_enabled(!automatic);
        assert(popup.overlay==same_overlay && machine_fault_find(jam,NULL) && protocol_calls==tx);
        tick(18000);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE && protocol_calls==tx+(automatic?0:1));
        if(automatic)confirm(NULL);
        assert(protocol_calls==tx+1 && !fault_popup_is_showing() && machine_fault_find(jam,NULL));
        if(outcome==0)app_fault_recovery_handle_clear_result(1);
        else if(outcome==1)app_fault_recovery_handle_clear_result(2);
        else {tick(3000);app_fault_recovery_poll();}
        assert(protocol_calls==tx+1);
        if(outcome==0) {
            assert(!machine_fault_find(jam,NULL) && !fault_popup_is_showing());
        } else {
            /* The original flow owns a late result, despite the opposite current preference. */
            assert(machine_fault_find(jam,NULL) && fault_popup_is_showing()==(automatic!=0));
            if(!automatic)assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING && g_si_ctx.warning.marquee_running);
        }
        /* A following, distinct controller report uses the new preference. */
        hide_fault_popup();
        const machine_fault_key_t next={MACHINE_FAULT_START,2,9};
        app_fault_recovery_report(next);fault_popup_report_start_fault(2,9);
        assert(fault_popup_is_showing()==(automatic==0) && protocol_calls==tx+1);
        assert(machine_fault_find(next,NULL));
    }
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    fault_popup_set_auto_enabled(false);
}
int main(void)
{
    lv_init();
    static lv_color_t pixels[1280*40];static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer,pixels,NULL,1280*40);
    static lv_disp_drv_t display;lv_disp_drv_init(&display);
    display.hor_res=1280;display.ver_res=400;display.draw_buf=&buffer;display.flush_cb=flush;
    assert(lv_disp_drv_register(&display));
    lv_img_decoder_t *decoder=lv_img_decoder_create();assert(decoder);
    lv_img_decoder_set_info_cb(decoder,host_image_info);lv_img_decoder_set_open_cb(decoder,host_image_open);
    lv_img_decoder_set_close_cb(decoder,host_image_close);
    fixture(false);assert(currency_state_confirm_active_code("USD"));ui_main_create(lv_scr_act());tick(400);
    app_fault_recovery_init();fault_popup_set_auto_enabled(false);
    test_independent_switches();
    test_recovery_switch_independence();
    test_popup_preference_only();
    test_late_recovery_results();
    unsigned tx=protocol_calls;
    fault(7);assert(protocol_calls==tx); /* start fault does not clear by command */
    tick(18000);assert(protocol_calls==tx);
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    fault(7);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING && protocol_calls==tx);
    tick(1500);lv_coord_t x=lv_obj_get_x(g_si_ctx.objects.title);
    fault(7);assert(lv_obj_get_x(g_si_ctx.objects.title)==x && protocol_calls==tx);
    tick(18000);assert(protocol_calls==tx);
    fault(7);app_fault_recovery_stacker_cleared();tx=protocol_calls;
    tick(18000);assert(protocol_calls==tx && g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    /* Door fault repeats after a completed notice, without restarting a live one. */
    fault(9);tick(18000);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    fault(9);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING);
    /* No-note remains a yellow, keyed warning and never sends 0x3D. */
    app_fault_recovery_clear();fault_popup_report_start_no_note();tx=protocol_calls;
    assert(g_si_ctx.warning.fault.valid && g_si_ctx.warning.level==SMART_ISLAND_WARNING_LEVEL_WARNING);
    assert(g_si_ctx.warning.fault.source==MACHINE_FAULT_START && g_si_ctx.warning.fault.code==2);
    tick(18000);app_fault_recovery_poll();assert(protocol_calls==tx);
    fault_popup_report_start_no_note();assert(g_si_ctx.warning.fault.valid);
    assert(g_si_ctx.warning.level==SMART_ISLAND_WARNING_LEVEL_WARNING && protocol_calls==tx);
    /* The displayed identity survives unrelated unread boot faults. */
    fault_popup_record_boot_result(5,2);fault(7);
    assert(g_si_ctx.warning.fault.source==MACHINE_FAULT_START && g_si_ctx.warning.fault.code==7);
    lv_event_send(g_si_ctx.objects.root,LV_EVENT_CLICKED,NULL);
    assert(popup.key.source==MACHINE_FAULT_START && popup.key.code==7);
    hide_fault_popup();
    fault_popup_set_auto_enabled(true);assert(!fault_popup_is_showing());
    assert(fault_popup_show_key((machine_fault_key_t){MACHINE_FAULT_START,2,7}));
    tx=protocol_calls;confirm(NULL);assert(protocol_calls==tx); /* local read acknowledgement */
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    fault_popup_set_auto_enabled(true);
    fault_popup_report_start_no_note();assert(fault_popup_is_showing());
    tx=protocol_calls;confirm(NULL);assert(protocol_calls==tx&&!fault_popup_is_showing());
    fault_popup_report_start_no_note();assert(fault_popup_is_showing());
    confirm(NULL);assert(protocol_calls==tx&&!fault_popup_is_showing());
    machine_fault_clear();smart_island_faults_changed();
    fault_popup_set_auto_enabled(false);fault(7);tx=protocol_calls;
    page_01_main_suspend();tick(18000);app_fault_recovery_poll();assert(protocol_calls==tx);
    smart_island_notify_warning("A settings warning while Main is hidden");
    assert(g_si_ctx.warning.fault.valid && g_si_ctx.warning.fault.code==7);
    app_fault_recovery_clear();fault_popup_clear_runtime();assert(page_01_main_resume());tick(18000);
    assert(protocol_calls==tx && g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    fault(7);tx=protocol_calls;ui_main_destroy();tick(18000);app_fault_recovery_poll();assert(protocol_calls==tx);
    counting_data_clear_serials(counting_data_mutable());counting_data_clear_errors(counting_data_mutable());
    ui_notice_deinit();lv_img_decoder_delete(decoder);lv_deinit();host_external_assets_release();
    puts("PASS real notice+island+popup+recovery: preference changes never open old faults or send/acknowledge, live and completed island retained, repeated START opens, in-flight mode survives success/failure/timeout, four switch combinations, runtime whitelist, suspend/destroy");
}
