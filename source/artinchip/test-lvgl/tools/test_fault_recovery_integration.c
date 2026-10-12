/* Real LVGL, fault popup, island and recovery service; capture UART only. */
#include "lvgl/lvgl.h"
#define HOST_REAL_FAULT
#define HOST_REAL_NOTICE
#define HOST_ISLAND_ONLY
#define main main_raster_entry_unused
#define protocol_send host_protocol_send
#include "test_main_view.c"
#undef protocol_send
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
#include "un260/counting/counting_info_reply.c"
#include "un260/protocol/protocol_request.h"
static protocol_request_start_observer_t observed_begin;
void protocol_request_set_start_observer(protocol_request_start_observer_t observer){observed_begin=observer;}
bool ui_factory_reset_blocks_navigation(void){return false;}
bool touch_calibration_busy(void){return false;}
bool lv_upgrade_popup_is_showing(void){return false;}
bool lv_qr_popup_is_showing(void){return false;}
/* This unrelated setting is linked by app_local_setting but not exercised here. */
#include "fault_standby_under_test.inc"

/* Page fixtures keep their existing sink; the separately linked recovery
 * service must send only the latch handshake, never CLEAR or START. */
static bool recovery_send_failed;
static unsigned recovery_command_calls;
int protocol_send(uint8_t cmd,const uint8_t *data,uint16_t size)
{
    assert(cmd==0x3D&&size==1&&data&&*data==1);
    ++recovery_command_calls;
    int sent=host_protocol_send(cmd,data,size);
    return recovery_send_failed?-1:sent;
}

/* Offset separates the recovery deadline from animation time in ordering tests. */
bool counting_action_start_pending(void){return false;}
uint64_t protocol_request_result_time_ms(void){return lv_tick_get();}
bool currency_service_take_switch_rejection_detail(uint8_t command,uint8_t code)
{(void)command;(void)code;return false;}

static uint32_t recovery_clock_offset;
uint32_t __wrap_app_clock_uptime_ms(void) {return lv_tick_get()+recovery_clock_offset;}
static void fault_test_progress(const char *stage)
{
    const char *enabled=getenv("UN260_FAULT_TEST_PROGRESS");
    if(enabled&&!strcmp(enabled,"1"))
        fprintf(stderr,"[fault-test] %s tick=%u\n",stage,lv_tick_get());
}

static void fault(uint8_t code)
{
    app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_START,2,code});
    fault_popup_report_start_fault(2,code);
}
static void test_setting_rejections(void)
{
    const uint8_t commands[]={3,4,0x3A,6};
    for(unsigned top=0;top<2;++top)for(unsigned automatic=0;automatic<2;++automatic)
    for(unsigned i=0;i<sizeof(commands);++i) {
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        ui_notice_deinit();ui_notice_set_enabled(top!=0);fault_popup_set_auto_enabled(automatic!=0);
        unsigned tx=protocol_calls;
        assert(app_fault_recovery_report_setting_rejection(commands[i]));
        const machine_fault_key_t key={MACHINE_FAULT_SETTING,0x80,commands[i]};
        assert(!machine_fault_find(key,NULL) && !machine_fault_count() && !machine_fault_first_unread(NULL));
        assert(!fault_popup_get_pending_fault(NULL,NULL,NULL) && fault_popup_is_showing()==(automatic!=0));
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING && g_si_ctx.warning.fault.valid);
        assert(g_si_ctx.warning.fault.source==MACHINE_FAULT_SETTING && g_si_ctx.warning.fault.code==commands[i]);
        assert(!strcmp(g_si_ctx.warning.text,ui_tr("Stacker full")));
        assert(!ui_notice_is_visible() && !notice.state.has_active && protocol_calls==tx);
        if(automatic) {
            assert(popup.guide.step_count==1 && popup.guide.steps[0].zone==MF_STACKER);
            assert(strstr(lv_label_get_text(popup.step_body),"Notes remain in the stacker pocket"));
            confirm(NULL);assert(!fault_popup_is_showing() && protocol_calls==tx);
        }
        tick(18000);app_fault_recovery_poll();
        /* Ordinary faults use the same existing length-aware island cue. A
         * settings refusal does not dispatch 3D at the animation deadline. */
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE && protocol_calls==tx);
        assert(!setting_notice_active && !machine_fault_count() && !fault_popup_get_pending_fault(NULL,NULL,NULL));
        fault_popup_set_auto_enabled(!automatic);fault_popup_set_auto_enabled(automatic!=0);
        assert(!fault_popup_is_showing());
        assert(app_fault_recovery_report_setting_rejection(commands[i]));
        assert(fault_popup_is_showing()==(automatic!=0));
        fault_popup_stacker_cleared();
        assert(!machine_fault_find(key,NULL) && !fault_popup_is_showing() && protocol_calls==tx);
    }
    /* An operation event never replaces a genuine jam's live guide or owns
     * its existing recovery deadline/ACK. */
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    fault_popup_set_auto_enabled(true);
    const machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};
    app_fault_recovery_report(jam);fault_popup_report_runtime_fault(2);
    lv_obj_t *overlay=popup.overlay;tick(500);uint32_t elapsed=popup.model.elapsed_ms;
    assert(app_fault_recovery_report_setting_rejection(4));
    assert(popup.overlay==overlay && popup.model.elapsed_ms==elapsed && machine_fault_find(jam,NULL));
    unsigned tx=protocol_calls;confirm(NULL);assert(protocol_calls==tx+1);
    app_fault_recovery_handle_clear_result(1);assert(!machine_fault_find(jam,NULL));
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    ui_notice_deinit();ui_notice_set_enabled(true);fault_popup_set_auto_enabled(false);
}


static void test_sort_feedback_language_and_navigation(void)
{
    const char *body="Notes remain in the stacker pocket. Cannot change sorting (SOT).";
    for(unsigned language=0;language<2;++language)for(unsigned automatic=0;automatic<2;++automatic) {
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        ui_notice_deinit();ui_notice_set_enabled(true);fault_popup_set_auto_enabled(automatic!=0);
        ui_lang_set(language?LANGUAGE_CN:LANGUAGE_EN);
        ui_page_t origin=host_current_page;
        unsigned tx=protocol_calls;
        uint8_t previous=machine_state_fo_mode();
        assert(app_fault_recovery_report_setting_rejection(0x3A));
        assert(host_current_page==origin&&machine_state_fo_mode()==previous&&protocol_calls==tx);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING&&g_si_ctx.warning.fault.code==0x3A);
        assert(!strcmp(g_si_ctx.warning.text,ui_tr("Stacker full")));
        if(language)assert(!strcmp(g_si_ctx.warning.text,"接钞口满"));
        assert(fault_popup_is_showing()==(automatic!=0));
        if(automatic) {
            assert(!strcmp(lv_label_get_text(popup.title),ui_tr("Sorting change blocked")));
            assert(!strcmp(lv_label_get_text(popup.step_body),ui_tr(body)));
            if(language)assert(!strcmp(lv_label_get_text(popup.step_body),"接钞口有钞票，无法切换分钞方式（SOT）。"));
            lv_obj_update_layout(popup.overlay);
            assert(popup.model.playing&&popup.model.timer&&!popup.model.timer->paused);
            confirm(NULL);assert(!fault_popup_is_showing()&&protocol_calls==tx);
        }
        tick(18000);app_fault_recovery_poll();
        assert(host_current_page==origin&&machine_state_fo_mode()==previous&&protocol_calls==tx);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE&&!setting_notice_active&&!machine_fault_count());
    }
    ui_lang_set(LANGUAGE_EN);fault_popup_set_auto_enabled(false);
}

static bool layout_can_edit_anywhere(void)
{
    for(int y=20;y<390;y+=40)for(int x=20;x<1260;x+=80) {
        editor.point=(lv_point_t){x,y};
        if(allowed())return true;
    }
    return false;
}
static void operation_consumers_released(void)
{
    assert(!machine_fault_count() && !machine_fault_first_unread(NULL));
    assert(!fault_popup_get_pending_fault(NULL,NULL,NULL));
    assert(qc_guide_safe() && layout_can_edit_anywhere());
    assert(!standby_operation_blocker());
}
static void test_operation_notice_lifecycle_and_consumers(void)
{
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    ui_notice_deinit();ui_notice_set_enabled(false);host_current_page=UI_PAGE_MAIN;
    edit_feature_enabled=true;editor.enabled=true;
    fault_popup_set_auto_enabled(false);
    operation_consumers_released();
    assert(app_fault_recovery_report_setting_rejection(4));
    operation_consumers_released(); /* OFF is display-only even before END. */
    smart_island_faults_changed();assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING);
    tick(18000);operation_consumers_released();assert(!setting_notice_active);
    for(unsigned outcome=0;outcome<4;++outcome) {
        assert(app_fault_recovery_report_setting_rejection(6));
        smart_island_set_suspended(true);
        assert(!setting_notice_active && !g_si_ctx.warning.fault.valid &&
               !g_si_ctx.warning.resume_animation_pending);
        if(outcome==0)app_fault_recovery_resolve_setting_rejection(6);
        if(outcome==1)app_fault_recovery_stacker_cleared();
        if(outcome==2)app_fault_recovery_cancel_setting_presentation();
        if(outcome==3){assert(observed_begin);observed_begin(3);}
        smart_island_set_suspended(false);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
        operation_consumers_released();
    }
    assert(app_fault_recovery_report_setting_rejection(3));
    lv_obj_t *parent=lv_obj_get_parent(g_si_ctx.objects.root);
    smart_island_destroy();assert(!setting_notice_active);
    smart_island_create(parent);tick(200);
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);operation_consumers_released();
    fault_popup_set_auto_enabled(true);
    assert(app_fault_recovery_report_setting_rejection(4));assert(popup.overlay);
    lv_obj_del(popup.overlay);
    assert(!popup.overlay&&!setting_notice_active&&g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    operation_consumers_released();
    assert(app_fault_recovery_report_setting_rejection(6));
    tick(18000);
    assert(popup.overlay&&!setting_notice_active); /* Reading modal retains Confirm ownership. */
    confirm(NULL);operation_consumers_released();
    assert(app_fault_recovery_report_setting_rejection(3));
    fault_popup_set_auto_enabled(false);
    fault_popup_report_sensor_mask(1);
    const machine_fault_key_t sensor={MACHINE_FAULT_SENSOR,0,0};
    assert(!popup.overlay && machine_fault_find(sensor,NULL));
    assert(!setting_notice_active && g_si_ctx.warning.fault.source==MACHINE_FAULT_SENSOR);
    assert(fault_popup_get_pending_fault(NULL,NULL,NULL)&&machine_fault_first_unread(NULL));
    assert(!qc_guide_safe()&&!layout_can_edit_anywhere()&&standby_operation_blocker());
    fault_popup_report_sensor_mask(0);operation_consumers_released();
    assert(app_fault_recovery_report_setting_rejection(3));
    const machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};
    machine_fault_report(jam);
    smart_island_notify_fault("Notes remain in the stacker pocket. Cannot change the currency.",jam);
    smart_island_cancel_fault_notice((machine_fault_key_t){MACHINE_FAULT_SETTING,0x80,3});
    app_fault_recovery_resolve_setting_rejection(3);
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING&&g_si_ctx.warning.fault.source==MACHINE_FAULT_RUNTIME);
    assert(fault_popup_get_pending_fault(NULL,NULL,NULL)&&!qc_guide_safe()&&!layout_can_edit_anywhere()&&standby_operation_blocker());
    tick(18000);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    assert(machine_fault_find(jam,NULL));
    assert(app_fault_recovery_report_setting_rejection(0x3A));
    assert(g_si_ctx.warning.fault.source==MACHINE_FAULT_SETTING && !strcmp(g_si_ctx.warning.text,ui_tr("Stacker full")));
    assert(machine_fault_find(jam,NULL) && fault_popup_get_pending_fault(NULL,NULL,NULL));
    app_fault_recovery_resolve_setting_rejection(0x3A);
    fault_popup_resolve_key(jam);operation_consumers_released();
    start_busy=true;assert(app_fault_recovery_report_setting_rejection(4));
    assert(!setting_notice_active&&!popup.overlay);
    app_fault_recovery_poll();assert(!setting_notice_active);
    start_busy=false;app_fault_recovery_poll();
    if(!setting_notice_active || g_si_ctx.warning.fault.source!=MACHINE_FAULT_SETTING) fprintf(stderr,"defer state active=%d scene=%d valid=%d source=%d modal=%d busy=%d physical=%zu\n",setting_notice_active,g_si_ctx.view.scene,g_si_ctx.warning.fault.valid,g_si_ctx.warning.fault.source,popup.overlay!=NULL,start_busy,machine_fault_count());
    assert(setting_notice_active && g_si_ctx.warning.fault.source==MACHINE_FAULT_SETTING);
    app_fault_recovery_cancel_setting_presentation();operation_consumers_released();
    start_busy=true;assert(app_fault_recovery_report_setting_rejection(6));
    app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,0});
    start_busy=false;app_fault_recovery_poll();
    assert(!setting_notice_active&&!popup.overlay);operation_consumers_released();
    fault_popup_set_auto_enabled(false);ui_notice_set_enabled(true);
}

static void test_independent_switches(void)
{
    const machine_fault_key_t door={MACHINE_FAULT_START,2,9};
    for(unsigned top=0;top<2;++top)for(unsigned automatic=0;automatic<2;++automatic) {
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        ui_notice_deinit();ui_notice_set_enabled(top!=0);fault_popup_set_auto_enabled(automatic!=0);
        unsigned tx=protocol_calls;
        ui_notice_post_text(UI_NOTICE_SUCCESS,"menu.settings","Confirm","Saved");
        assert(!ui_notice_is_visible() && notice.pending==(top!=0));
        tick(NOTICE_DELAY_MS); /* Existing notice policy settles the first frame before entry. */
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
        assert(!ui_notice_is_visible() && notice.pending==(top!=0));
        tick(NOTICE_DELAY_MS); /* Existing notice policy settles the first frame before entry. */
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
        unsigned prior_scene=g_si_ctx.view.scene;
        app_fault_recovery_handle_clear_result(2);
        assert(machine_fault_find(jam,NULL) && !fault_popup_is_showing());
        assert(g_si_ctx.view.scene==(automatic?prior_scene:SMART_ISLAND_SCENE_WARNING));
        same_overlay=popup.overlay;
        fault_popup_set_auto_enabled(!automatic);fault_popup_set_auto_enabled(automatic!=0);
        assert(popup.overlay==same_overlay && protocol_calls==tx+1 && machine_fault_find(jam,NULL));
        ui_notice_set_enabled(top!=0);
        tick(18000);assert(protocol_calls==tx+1); /* Replaying the guide never re-arms its END command. */
        if(automatic){assert(fault_popup_show_key(jam));confirm(NULL);}
        else assert(app_fault_recovery_request_clear(jam));
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
        tick(18000);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE&&protocol_calls==tx+1);
        if(outcome==0)app_fault_recovery_handle_clear_result(1);
        else if(outcome==1)app_fault_recovery_handle_clear_result(2);
        else {tick(3000);app_fault_recovery_poll();}
        assert(protocol_calls==tx+1);
        if(outcome==0) {
            assert(!machine_fault_find(jam,NULL) && !fault_popup_is_showing());
        } else {
            /* A real FAIL may repeat only an unread report. Confirmed guides
             * and transport timeouts do not create a new controller event. */
            assert(machine_fault_find(jam,NULL) && !fault_popup_is_showing());
            assert(g_si_ctx.view.scene==(outcome==1&&!automatic?SMART_ISLAND_SCENE_WARNING:SMART_ISLAND_SCENE_IDLE));
            if(outcome==1&&!automatic)assert(g_si_ctx.warning.marquee_running);
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
static void test_pocket_recovery_modes(void)
{
    const uint8_t codes[]={5,7,8};
    for(unsigned i=0;i<sizeof(codes);++i)for(unsigned automatic=0;automatic<2;++automatic) {
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        fault_popup_set_auto_enabled(automatic!=0);
        machine_fault_key_t key={MACHINE_FAULT_START,2,codes[i]};
        unsigned tx=protocol_calls;
        fault(codes[i]);
        assert(protocol_calls==tx+1&&fault_popup_is_showing()==(automatic!=0));
        assert(machine_fault_find(key,NULL)&&g_si_ctx.warning.marquee_running);
        /* No Confirm action: the automatic request already reached UART. */
        app_fault_recovery_handle_clear_result(1);
        assert(machine_fault_find(key,NULL)&&fault_popup_is_showing()==(automatic!=0));
        tick(18000);
        assert(protocol_calls==tx+2&&g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
        assert(machine_fault_find(key,NULL)&&fault_popup_is_showing()==(automatic!=0));
        app_fault_recovery_stacker_cleared();
        assert(machine_fault_find(key,NULL)==(codes[i]!=7));
        assert(fault_popup_is_showing()==(automatic&&codes[i]!=7));
        app_fault_recovery_handle_clear_result(1);
        assert(protocol_calls==tx+2&&machine_fault_find(key,NULL)==(codes[i]!=7));
        /* Taking cash before the hidden island begins cannot swallow recovery. */
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        page_01_main_suspend();tx=protocol_calls;
        fault(codes[i]);assert(protocol_calls==tx+1&&machine_fault_find(key,NULL));
        app_fault_recovery_stacker_cleared();
        assert(machine_fault_find(key,NULL)==(codes[i]!=7));
        app_fault_recovery_handle_clear_result(1);assert(protocol_calls==tx+1);
        app_fault_recovery_clear();fault_popup_clear_runtime();
        assert(page_01_main_resume());tick(18000);assert(protocol_calls==tx+1);
    }
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    fault_popup_set_auto_enabled(false);
}
static void test_pocket_recovery_failures(void)
{
    for(unsigned automatic=0;automatic<2;++automatic)for(unsigned outcome=0;outcome<4;++outcome) {
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        fault_popup_set_auto_enabled(automatic!=0);recovery_send_failed=outcome==0;
        recovery_clock_offset=0;
        unsigned tx=protocol_calls;machine_fault_key_t key={MACHINE_FAULT_START,2,7};
        fault(7);assert(protocol_calls==tx+1&&machine_fault_find(key,NULL));
        if(outcome==1)app_fault_recovery_handle_clear_result(2);
        else if(outcome==2){
            /* LVGL repeat_count=1 plays 1000ms forward + 1000ms playback plus
             * 260ms collapse, so the short notice ends before the 3s deadline. */
            tick(3100);
            assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE&&protocol_calls==tx+1);
            app_fault_recovery_poll();assert(protocol_calls==tx+1);
            /* END already coalesced the first request before its deadline. */
        } else if(outcome==3){
            tick(100);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING&&protocol_calls==tx+1);
            recovery_clock_offset=3100;app_fault_recovery_poll();
            assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING&&protocol_calls==tx+1);
            /* The service timed out while the original animation still owns END. */
        }
        unsigned expected=outcome==2?1U:2U;
        tick(18000);
        if(protocol_calls!=tx+expected||!machine_fault_find(key,NULL))
            fprintf(stderr,"pocket failure automatic=%u outcome=%u before=%u after=%u scene=%u showing=%u tick=%u\n",
                    automatic,outcome,tx,protocol_calls,g_si_ctx.view.scene,fault_popup_is_showing(),lv_tick_get());
        assert(protocol_calls==tx+expected&&machine_fault_find(key,NULL));
        if(outcome==1)app_fault_recovery_handle_clear_result(2);
        else if(outcome==2||outcome==3)app_fault_recovery_poll();
        tick(40000);app_fault_recovery_poll();
        assert(protocol_calls==tx+expected&&machine_fault_find(key,NULL));
        assert(fault_popup_is_showing()==(automatic!=0));
        /* Display replay after failure/timeout must not create an unbounded request loop. */
        recovery_send_failed=false;recovery_clock_offset=0;
    }
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    fault_popup_set_auto_enabled(false);
}
static void test_unresolved_pocket_lifecycle(void)
{
    const uint8_t codes[]={5,7,8};
    for(unsigned i=0;i<sizeof(codes);++i)for(unsigned automatic=0;automatic<2;++automatic) {
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        fault_popup_set_auto_enabled(automatic!=0);
        machine_fault_key_t key={MACHINE_FAULT_START,2,codes[i]};
        unsigned sent=recovery_command_calls;
        page_01_main_suspend();fault(codes[i]);
        assert(recovery_command_calls==sent+1&&machine_fault_find(key,NULL));
        app_fault_recovery_handle_clear_result(1);
        assert(page_01_main_resume());
        assert(recovery_command_calls==sent+1&&machine_fault_find(key,NULL));
        tick(18000);assert(recovery_command_calls==sent+2&&machine_fault_find(key,NULL));
        app_fault_recovery_handle_clear_result(1);
        assert(fault_popup_is_showing()==(automatic!=0));
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        sent=recovery_command_calls;fault(codes[i]);app_fault_recovery_handle_clear_result(1);
        assert(recovery_command_calls==sent+1&&machine_fault_find(key,NULL));
        ui_main_destroy();assert(machine_fault_find(key,NULL));
        ui_main_create(lv_scr_act());tick(400);
        assert(recovery_command_calls==sent+1&&machine_fault_find(key,NULL)); /* Creation cannot re-arm UART work. */
        /* Destroy clears the notice objects; restoring their presentation is
         * a display action, not another controller report or clear request. */
        smart_island_notify_fault(machine_start_error_desc(codes[i]),key);
        assert(recovery_command_calls==sent+1);
        tick(18000);assert(recovery_command_calls==sent+2&&machine_fault_find(key,NULL));
        app_fault_recovery_handle_clear_result(1);
        smart_island_notify_fault(machine_start_error_desc(codes[i]),key);tick(18000);
        assert(recovery_command_calls==sent+2&&machine_fault_find(key,NULL)); /* The completion budget was consumed. */
    }
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    fault_popup_set_auto_enabled(false);
}
static void test_confirmed_pockets_without_new_start(void)
{
    const uint8_t codes[]={5,7,8};
    for(unsigned i=0;i<sizeof(codes);++i)for(unsigned automatic=0;automatic<2;++automatic)
    for(unsigned outcome=0;outcome<2;++outcome) {
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        recovery_send_failed=false;recovery_clock_offset=0;
        fault_popup_set_auto_enabled(automatic!=0);
        machine_fault_key_t key={MACHINE_FAULT_START,2,codes[i]};
        unsigned tx=protocol_calls;fault(codes[i]);
        assert(protocol_calls==tx+1&&fault_popup_is_showing()==(automatic!=0));
        /* OFF still permits explicit inspection. Taking cash has no general
         * sensor-clear frame for 05/08; Confirm must not wait for another START. */
        if(!automatic)assert(fault_popup_show_key(key));
        confirm(NULL);
        machine_fault_record_t record;
        assert(!fault_popup_is_showing()&&protocol_calls==tx+1);
        assert(machine_fault_find(key,&record)&&record.acknowledged);
        tick(3100);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
        if(outcome)app_fault_recovery_handle_clear_result(2);else app_fault_recovery_poll();
        if(fault_popup_is_showing()||g_si_ctx.view.scene!=SMART_ISLAND_SCENE_IDLE)
            fprintf(stderr,"Confirm replay code=%u Popup=%u result=%s scene=%u modal=%u\n",
                codes[i],automatic,outcome?"FAIL":"timeout",g_si_ctx.view.scene,fault_popup_is_showing());
        assert(!fault_popup_is_showing()&&g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
        assert(machine_fault_find(key,&record)&&record.acknowledged); /* Presentation acknowledgement is not physical recovery. */
        tick(40000);app_fault_recovery_poll();
        assert(protocol_calls==tx+1&&!fault_popup_is_showing()&&g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
        /* A genuinely new wire report still uses the selected presentation. */
        fault(codes[i]);
        assert(protocol_calls==tx+2&&fault_popup_is_showing()==(automatic!=0));
        assert(machine_fault_find(key,&record)&&!record.acknowledged);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING);
    }
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    recovery_send_failed=false;recovery_clock_offset=0;fault_popup_set_auto_enabled(false);
}
static void test_old_failure_keeps_new_pocket_presentation(void)
{
    for(unsigned automatic=0;automatic<2;++automatic)for(unsigned same_key=0;same_key<2;++same_key) {
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        recovery_send_failed=false;recovery_clock_offset=0;fault_popup_set_auto_enabled(automatic!=0);
        machine_fault_key_t old={MACHINE_FAULT_START,2,5};
        unsigned tx=protocol_calls;fault(5);
        if(!automatic)assert(fault_popup_show_key(old));
        confirm(NULL);assert(!popup.overlay&&protocol_calls==tx+1);
        uint8_t next=same_key?5:7;machine_fault_key_t current={MACHINE_FAULT_START,2,next};
        fault(next);assert(protocol_calls==tx+1&&fault_popup_is_showing()==(automatic!=0));
        tick(600);
        lv_obj_t *overlay=popup.overlay;
        uint32_t elapsed=popup.model.elapsed_ms;
        lv_coord_t x=lv_obj_get_x(g_si_ctx.objects.title);
        app_fault_recovery_handle_clear_result(2); /* Untagged result belongs to the old transaction. */
        assert(protocol_calls==tx+2&&machine_fault_find(current,NULL));
        assert(popup.overlay==overlay&&popup.model.elapsed_ms==elapsed);
        assert(g_si_ctx.warning.fault.valid&&g_si_ctx.warning.fault.code==next);
        assert(lv_obj_get_x(g_si_ctx.objects.title)==x);
        if(automatic)assert(machine_fault_key_equal(popup.key,current));
        app_fault_recovery_handle_clear_result(1);
    }
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    fault_popup_set_auto_enabled(false);
}
static void test_old_runtime_failure_keeps_new_start_guide(void)
{
    const machine_fault_key_t old={MACHINE_FAULT_RUNTIME,0,2};
    const machine_fault_key_t current={MACHINE_FAULT_START,2,7};
    for(unsigned automatic=0;automatic<2;++automatic)for(unsigned queued_send_failure=0;queued_send_failure<2;++queued_send_failure) {
        hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
        recovery_send_failed=false;recovery_clock_offset=0;fault_popup_set_auto_enabled(automatic!=0);
        unsigned tx=protocol_calls;
        app_fault_recovery_report(old);fault_popup_report_runtime_fault(2);
        assert(app_fault_recovery_request_clear(old)&&protocol_calls==tx+1);
        hide_fault_popup(); /* Keep the old physical report unread and unresolved. */
        fault(7);assert(protocol_calls==tx+1&&machine_fault_find(old,NULL)&&machine_fault_find(current,NULL));
        tick(600);
        lv_obj_t *overlay=popup.overlay;
        uint32_t elapsed=popup.model.elapsed_ms;
        lv_coord_t x=lv_obj_get_x(g_si_ctx.objects.title);
        assert(g_si_ctx.warning.fault.valid&&g_si_ctx.warning.fault.source==current.source&&g_si_ctx.warning.fault.code==current.code);
        recovery_send_failed=queued_send_failure!=0;
        app_fault_recovery_handle_clear_result(2);
        assert(protocol_calls==tx+2&&machine_fault_find(old,NULL)&&machine_fault_find(current,NULL));
        assert(popup.overlay==overlay&&popup.model.elapsed_ms==elapsed);
        assert(g_si_ctx.warning.fault.valid&&g_si_ctx.warning.fault.source==current.source&&g_si_ctx.warning.fault.code==current.code);
        assert(lv_obj_get_x(g_si_ctx.objects.title)==x);
        if(automatic)assert(machine_fault_key_equal(popup.key,current));
        recovery_send_failed=false;
        if(!queued_send_failure)app_fault_recovery_handle_clear_result(1);
    }
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    recovery_send_failed=false;fault_popup_set_auto_enabled(false);
}
static void test_confirm_send_failure_retry(void)
{
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    fault_popup_set_auto_enabled(true);recovery_send_failed=true;
    machine_fault_key_t key={MACHINE_FAULT_START,2,5};
    unsigned tx=protocol_calls;fault(5);assert(protocol_calls==tx+1&&popup.overlay);
    lv_obj_t *overlay=popup.overlay;confirm(NULL);
    machine_fault_record_t record;
    assert(protocol_calls==tx+2&&popup.overlay==overlay&&machine_fault_find(key,&record)&&!record.acknowledged);
    assert(!strcmp(lv_label_get_text(popup.safety),ui_tr("Clear request not sent. Try Confirm again shortly.")));
    recovery_send_failed=false;confirm(NULL);
    assert(protocol_calls==tx+3&&!popup.overlay&&machine_fault_find(key,&record)&&record.acknowledged);
    tick(3100);app_fault_recovery_poll();
    assert(!popup.overlay&&g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    fault_popup_set_auto_enabled(false);
}

static const machine_fault_key_t batch_full_key={MACHINE_FAULT_PRESET,0,4};
static void batch_test_reset(bool automatic)
{
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    recovery_send_failed=false;recovery_clock_offset=0;fault_popup_set_auto_enabled(automatic);
}
static void batch_test_report(void)
{
    app_fault_recovery_report(batch_full_key);fault_popup_report_preset_full();
}
static void batch_test_tick(uint32_t milliseconds)
{
    while(milliseconds){uint32_t step=milliseconds>100?100:milliseconds;tick(step);app_fault_recovery_poll();milliseconds-=step;}
}
static void test_batch_real_popup_and_island(void)
{
    for(unsigned automatic=0;automatic<2;++automatic){
        batch_test_reset(automatic!=0);unsigned tx=protocol_calls;
        batch_test_report();assert(fault_popup_is_showing()==(automatic!=0)&&protocol_calls==tx);
        assert(g_si_ctx.warning.fault.valid&&g_si_ctx.warning.fault.source==MACHINE_FAULT_PRESET);
        assert(!strcmp(g_si_ctx.warning.text,ui_tr("Batch full")));
        tick(600);lv_obj_t *overlay=popup.overlay;uint32_t elapsed=popup.model.elapsed_ms;
        lv_coord_t island_x=lv_obj_get_x(g_si_ctx.objects.title);
        batch_test_report(); /* Duplicate unread report preserves the current animation and modal. */
        assert(popup.overlay==overlay&&popup.model.elapsed_ms==elapsed);
        assert(lv_obj_get_x(g_si_ctx.objects.title)==island_x&&protocol_calls==tx);
        if(automatic){
            batch_test_tick(4000);assert(protocol_calls==tx); /* END is not authorization in Popup ON. */
            confirm(NULL);assert(!popup.overlay&&protocol_calls==tx+1);
            machine_fault_record_t record;
            assert(machine_fault_find(batch_full_key,&record)&&record.acknowledged);
            batch_test_report(); /* Main controller answered Confirm with 06/04, not a 3D ACK. */
            assert(popup.overlay&&machine_fault_find(batch_full_key,&record)&&!record.acknowledged);
            overlay=popup.overlay;batch_test_report();assert(popup.overlay==overlay&&protocol_calls==tx+1);
            confirm(NULL);assert(!popup.overlay&&protocol_calls==tx+1); /* The pending wire request is shared. */
            app_fault_recovery_handle_clear_result(1);assert(machine_fault_find(batch_full_key,NULL));
        }else{
            batch_test_tick(5000);assert(protocol_calls>=tx+1);
            batch_test_report(); /* The same report must recover beyond the previous two island notices. */
            assert(!popup.overlay&&g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING);
        }
        unsigned attempts=protocol_calls;batch_test_tick(3000);
        assert(protocol_calls==attempts+1&&machine_fault_find(batch_full_key,NULL));
        attempts=protocol_calls;batch_test_tick(3000);assert(protocol_calls==attempts+1);
        app_fault_recovery_stacker_cleared();app_fault_recovery_stacker_cleared();attempts=protocol_calls;
        assert(!machine_fault_find(batch_full_key,NULL)&&!popup.overlay);
        assert(!g_si_ctx.warning.fault.valid||g_si_ctx.warning.fault.source!=MACHINE_FAULT_PRESET);
        batch_test_tick(20000);assert(protocol_calls==attempts); /* No Clear or START is sent by this harness. */
    }
    /* First notice + quick cash removal preserves the previously working case. */
    batch_test_reset(false);unsigned tx=protocol_calls;batch_test_report();tick(600);
    app_fault_recovery_stacker_cleared();batch_test_tick(10000);
    assert(protocol_calls==tx&&!machine_fault_find(batch_full_key,NULL));
    /* Hidden CANCEL cannot start an unrequested Batch retry. */
    batch_test_reset(false);tx=protocol_calls;page_01_main_suspend();batch_test_report();
    batch_test_tick(20000);assert(protocol_calls==tx&&machine_fault_find(batch_full_key,NULL));
    app_fault_recovery_stacker_cleared();assert(page_01_main_resume());batch_test_tick(10000);
    assert(protocol_calls==tx&&!machine_fault_find(batch_full_key,NULL));
    /* Once END authorises recovery, hiding Main cannot strand a controller latch. */
    batch_test_reset(false);tx=protocol_calls;batch_test_report();batch_test_tick(5000);
    assert(protocol_calls>=tx+1);tx=protocol_calls;page_01_main_suspend();batch_test_tick(3000);
    assert(protocol_calls==tx+1);app_fault_recovery_stacker_cleared();tx=protocol_calls;
    assert(page_01_main_resume());batch_test_tick(10000);assert(protocol_calls==tx);
    batch_test_reset(false);
}
static void test_batch_real_generation_and_failure(void)
{
    for(unsigned automatic=0;automatic<2;++automatic){
        batch_test_reset(automatic!=0);batch_test_report();unsigned tx=protocol_calls;
        assert(app_fault_recovery_request_clear(batch_full_key)&&protocol_calls==tx+1);
        app_fault_recovery_stacker_cleared();batch_test_report();
        assert(fault_popup_is_showing()==(automatic!=0));
        assert(app_fault_recovery_request_clear(batch_full_key)&&protocol_calls==tx+1);
        app_fault_recovery_handle_clear_result(1);assert(machine_fault_find(batch_full_key,NULL));
        batch_test_tick(3000);assert(protocol_calls==tx+2);
        app_fault_recovery_clear();fault_popup_clear_preset();tx=protocol_calls;
        batch_test_tick(10000);assert(protocol_calls==tx&&!machine_fault_find(batch_full_key,NULL));
    }
    batch_test_reset(true);batch_test_report();unsigned tx=protocol_calls;lv_obj_t *overlay=popup.overlay;
    recovery_send_failed=true;confirm(NULL);machine_fault_record_t record;
    assert(popup.overlay==overlay&&protocol_calls==tx+1&&machine_fault_find(batch_full_key,&record)&&!record.acknowledged);
    batch_test_tick(2980);assert(protocol_calls==tx+1);batch_test_tick(20);assert(protocol_calls==tx+2);
    recovery_send_failed=false;batch_test_tick(3000);assert(protocol_calls==tx+3);
    /* A newer jam keeps its modal/animation while the older Batch timer is cancelled. */
    const machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};
    app_fault_recovery_report(jam);fault_popup_report_runtime_fault(2);overlay=popup.overlay;
    batch_test_tick(4000);assert(protocol_calls==tx+3&&popup.overlay==overlay&&machine_fault_key_equal(popup.key,jam));
    batch_test_reset(false);
}


static void test_batch_real_visuals(void)
{
    for(unsigned language=0;language<2;++language){
        batch_test_reset(true);ui_lang_set(language?LANGUAGE_CN:LANGUAGE_EN);batch_test_report();tick(600);
        assert(popup.overlay&&popup.guide.step_count==1&&popup.guide.steps[0].zone==MF_STACKER);
        assert(!strcmp(lv_label_get_text(popup.title),ui_tr("Batch full")));
        assert(!strcmp(g_si_ctx.warning.text,ui_tr("Batch full")));
        write_bmp(language?"batch-full-popup-cn":"batch-full-popup-en");
    }
    batch_test_reset(false);ui_lang_set(LANGUAGE_EN);
}



/* Count transport keeps owning FINISHED/history. The real island is observed
 * separately so a Batch presentation cannot invent a completed session. */
static counting_info_reply_result_t batch_pause_info(counting_session_state_t *session,
    counting_sim_t *data,uint8_t flag)
{
    uint8_t frame[]={0xFD,0xDF,0x0D,0x0E,0,0,0,100,0,10,2,flag,0};
    counting_info_reply_result_t result=counting_info_reply_handle(session,data,frame,sizeof(frame),90);
    if(result.kind==COUNTING_INFO_REPLY_LIVE)smart_island_update_counting(data->total_pcs,data->total_amount);
    else if(result.kind==COUNTING_INFO_REPLY_FINISHED){
        smart_island_set_count_analysis(result.final_pcs,result.final_issue,0);
        smart_island_update_counting(result.final_pcs,result.final_amount);
        smart_island_notify_count_end(NULL);
    }
    return result;
}
static void batch_pause_restart(counting_session_state_t *session,counting_sim_t *data)
{
    /* Match real START's island-facing path; do not reset the island first,
     * since that could conceal a START that fails to clear the Batch pause. */
    app_fault_recovery_clear();fault_popup_clear_runtime();fault_popup_clear_preset();
    memset(session,0,sizeof(*session));memset(data,0,sizeof(*data));
    session->start_confirmed=true;smart_island_notify_count_start();
    assert(g_si_ctx.lifecycle.count_session_active&&!g_si_ctx.lifecycle.batch_counting_paused);
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_COUNTING);
    assert(batch_pause_info(session,data,1).kind==COUNTING_INFO_REPLY_LIVE);
}
static void batch_pause_begin(bool automatic,counting_session_state_t *session,counting_sim_t *data)
{
    batch_test_reset(automatic);smart_island_notify_count_reset();tick(400);
    batch_pause_restart(session,data);
}
static void batch_pause_transport_active(const counting_session_state_t *session,const counting_sim_t *data)
{
    assert(session->phase==COUNTING_SESSION_ACTIVE&&session->start_confirmed);
    assert(!session->last_result.valid&&!session->history_record.valid);
    assert(data->total_pcs==10&&data->total_amount==100&&session->expected_issue==2);
}
static void batch_pause_idle_ui(void)
{
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    assert(g_si_ctx.objects.counting_root&&lv_obj_has_flag(g_si_ctx.objects.counting_root,LV_OBJ_FLAG_HIDDEN));
    assert(!g_si_ctx.counting.gate_anim_running);
}
static void test_batch_pause_missing_finish(void)
{
    for(unsigned automatic=0;automatic<2;++automatic){
        counting_session_state_t session;counting_sim_t data;
        batch_pause_begin(automatic!=0,&session,&data);batch_test_report();tick(80);
        assert(batch_pause_info(&session,&data,1).kind==COUNTING_INFO_REPLY_LIVE);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING&&g_si_ctx.lifecycle.count_session_active);
        assert(g_si_ctx.lifecycle.batch_counting_paused);
        batch_pause_transport_active(&session,&data);batch_test_tick(5000);
        /* The user log has no FINISHED frame. Batch END must stop only the
         * running presentation, retaining transport and history eligibility. */
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE&&g_si_ctx.lifecycle.count_session_active);batch_pause_idle_ui();
        batch_pause_transport_active(&session,&data);
        if(automatic){
            assert(popup.overlay);confirm(NULL);assert(!popup.overlay);
            /* Confirm keeps its existing keyed island cue. The cue must end
             * in IDLE rather than reviving the paused running display. */
            assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING&&g_si_ctx.lifecycle.batch_counting_paused);
            batch_test_tick(5000);
        }
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);batch_pause_idle_ui();
        batch_test_report();assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING);
        batch_test_tick(5000);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE&&g_si_ctx.lifecycle.count_session_active);batch_pause_idle_ui();
        app_fault_recovery_stacker_cleared();assert(!machine_fault_find(batch_full_key,NULL));
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE&&g_si_ctx.lifecycle.count_session_active);batch_pause_idle_ui();
        assert(batch_pause_info(&session,&data,1).kind==COUNTING_INFO_REPLY_LIVE);tick(400);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);batch_pause_transport_active(&session,&data);
        batch_pause_restart(&session,&data); /* The next real START owns resumption. */
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_COUNTING);
    }
    batch_test_reset(false);smart_island_notify_count_reset();
}
static void test_batch_pause_quick_cash_and_new_start(void)
{
    for(unsigned automatic=0;automatic<2;++automatic){
        counting_session_state_t session;counting_sim_t data;
        batch_pause_begin(automatic!=0,&session,&data);batch_test_report();tick(80);
        assert(batch_pause_info(&session,&data,1).kind==COUNTING_INFO_REPLY_LIVE);
        app_fault_recovery_stacker_cleared();
        assert(!machine_fault_find(batch_full_key,NULL)&&!popup.overlay);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE&&g_si_ctx.lifecycle.count_session_active);batch_pause_idle_ui();
        assert(g_si_ctx.lifecycle.batch_counting_paused);batch_pause_transport_active(&session,&data);
        batch_pause_restart(&session,&data);tick(500);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_COUNTING&&!g_si_ctx.lifecycle.batch_counting_paused);
    }
    batch_test_reset(false);smart_island_notify_count_reset();
}
static void test_batch_pause_late_finish(void)
{
    for(unsigned automatic=0;automatic<2;++automatic)for(unsigned hidden=0;hidden<2;++hidden)
    for(uint8_t finished=2;finished<=5;++finished){
        counting_session_state_t session;counting_sim_t data;
        batch_pause_begin(automatic!=0,&session,&data);batch_test_report();
        assert(batch_pause_info(&session,&data,1).kind==COUNTING_INFO_REPLY_LIVE);
        if(hidden){page_01_main_suspend();batch_test_tick(5000);assert(page_01_main_resume());}
        batch_test_tick(5000);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE&&g_si_ctx.lifecycle.count_session_active);batch_pause_idle_ui();
        batch_pause_transport_active(&session,&data);
        counting_info_reply_result_t result=batch_pause_info(&session,&data,finished);
        assert(result.kind==COUNTING_INFO_REPLY_FINISHED&&result.final_pcs==10&&result.final_amount==100&&result.final_issue==2);
        assert(session.phase==COUNTING_SESSION_FINISHED_WAIT_START&&!session.start_confirmed);
        assert(session.last_result.valid&&session.last_result.pcs==10&&session.last_result.valid_pcs==10);
        assert(session.last_result.issue_pcs==2&&session.last_result.amount==100);
        assert(session.history_record.valid&&session.history_record.pcs==10&&session.history_record.total_after==100);
        assert(session.history_record.amount==100&&!g_si_ctx.lifecycle.count_session_active&&!g_si_ctx.lifecycle.batch_counting_paused);
        tick(400);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_RESULT);
        lv_timer_t *result_timer=g_si_ctx.lifecycle.result_timer;
        assert(batch_pause_info(&session,&data,finished).kind==COUNTING_INFO_REPLY_IGNORED);
        assert(g_si_ctx.lifecycle.result_timer==result_timer);batch_test_tick(5000);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE&&!g_si_ctx.lifecycle.count_session_active);
    }
    batch_test_reset(false);smart_island_notify_count_reset();
}
static void test_batch_pause_hidden_rebuild_and_reset(void)
{
    for(unsigned automatic=0;automatic<2;++automatic)for(unsigned before=0;before<2;++before){
        counting_session_state_t session;counting_sim_t data;
        batch_pause_begin(automatic!=0,&session,&data);
        if(before)page_01_main_suspend();
        batch_test_report();assert(batch_pause_info(&session,&data,1).kind==COUNTING_INFO_REPLY_LIVE);
        if(!before)page_01_main_suspend();
        batch_test_tick(10000);batch_pause_transport_active(&session,&data);
        assert(page_01_main_resume());batch_test_tick(5000);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE&&g_si_ctx.lifecycle.count_session_active);batch_pause_idle_ui();
        batch_pause_transport_active(&session,&data);
        /* Keep the established destroy/create contract; business transport
         * remains active, while rebuilding never revives old running UI. */
        lv_obj_t *parent=lv_obj_get_parent(g_si_ctx.objects.root);smart_island_destroy();smart_island_create(parent);
        assert(!g_si_ctx.lifecycle.batch_counting_paused);
        smart_island_faults_changed();batch_test_tick(5000);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE&&!g_si_ctx.lifecycle.count_session_active);
        batch_pause_transport_active(&session,&data);
        batch_pause_begin(automatic!=0,&session,&data);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_COUNTING);
        /* CLEAR/session reset cancels this display pause without finishing or
         * saving the transport. A fresh START must still enter COUNTING. */
        batch_test_report();app_fault_recovery_clear();fault_popup_clear_preset();smart_island_notify_count_reset();
        tick(5000);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE&&!g_si_ctx.lifecycle.count_session_active&&!g_si_ctx.lifecycle.batch_counting_paused);
        batch_pause_transport_active(&session,&data);
        batch_pause_begin(automatic!=0,&session,&data);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_COUNTING);
    }
    batch_test_reset(false);smart_island_notify_count_reset();
}
static void test_batch_pause_other_warning_resume(void)
{
    for(unsigned automatic=0;automatic<2;++automatic)for(unsigned kind=0;kind<7;++kind){
        counting_session_state_t session;counting_sim_t data;
        batch_pause_begin(automatic!=0,&session,&data);
        if(kind==0)smart_island_notify_warning("Ordinary counting warning");
        else if(kind==1){
            const machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};
            app_fault_recovery_report(jam);fault_popup_report_runtime_fault(2);
        }else if(kind==2)assert(app_fault_recovery_report_setting_rejection(4));
        else if(kind==3)smart_island_notify_warning("Batch full"); /* Text alone never owns the pause. */
        else if(kind==4)smart_island_notify_fault("Batch full",(machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,4});
        else if(kind==5)smart_island_notify_preset_full((machine_fault_key_t){MACHINE_FAULT_PRESET,1,4});
        else smart_island_notify_preset_full((machine_fault_key_t){MACHINE_FAULT_PRESET,0,3});
        assert(!g_si_ctx.lifecycle.batch_counting_paused);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING&&g_si_ctx.lifecycle.count_session_active);
        batch_test_tick(18000);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_COUNTING&&g_si_ctx.lifecycle.count_session_active);
        batch_pause_transport_active(&session,&data);
    }
    batch_test_reset(false);smart_island_notify_count_reset();
}

static void batch_test_report_foreign(unsigned boot)
{
    if(boot)fault_popup_report_boot_result(5,2);else fault_popup_report_sensor_mask(UINT32_C(1));
}
static void test_batch_real_sensor_boot_isolation(void)
{
    for(unsigned boot=0;boot<2;++boot)for(unsigned automatic=0;automatic<2;++automatic){
        const machine_fault_key_t foreign=boot?(machine_fault_key_t){MACHINE_FAULT_BOOT,2,5}:
            (machine_fault_key_t){MACHINE_FAULT_SENSOR,0,0};
        batch_test_reset(automatic!=0);batch_test_report();unsigned tx=protocol_calls;
        if(automatic){confirm(NULL);assert(protocol_calls==tx+1);}else{batch_test_tick(5000);assert(protocol_calls>=tx+1);}
        fault_popup_set_auto_enabled(true);batch_test_report_foreign(boot);tx=protocol_calls;
        assert(popup.overlay&&machine_fault_key_equal(popup.key,foreign)&&machine_fault_find(foreign,NULL));
        tick(600);lv_obj_t *overlay=popup.overlay;uint32_t elapsed=popup.model.elapsed_ms;
        lv_coord_t island_x=lv_obj_get_x(g_si_ctx.objects.title);
        app_fault_recovery_handle_clear_result(2); /* Delayed result belongs to the Batch transaction. */
        assert(popup.overlay==overlay&&popup.model.elapsed_ms==elapsed&&machine_fault_key_equal(popup.key,foreign));
        assert(lv_obj_get_x(g_si_ctx.objects.title)==island_x&&g_si_ctx.warning.fault.source==foreign.source);
        batch_test_tick(6000);assert(protocol_calls==tx&&popup.overlay==overlay&&machine_fault_key_equal(popup.key,foreign));
        app_fault_recovery_stacker_cleared();assert(!machine_fault_find(batch_full_key,NULL)&&machine_fault_find(foreign,NULL));
        assert(popup.overlay==overlay&&machine_fault_key_equal(popup.key,foreign));
        /* No clear ACK is required for the cancellation either: keep old transaction ownership through timeout. */
        batch_test_reset(automatic!=0);batch_test_report();
        if(automatic)confirm(NULL);else batch_test_tick(5000);
        fault_popup_set_auto_enabled(true);batch_test_report_foreign(boot);tx=protocol_calls;overlay=popup.overlay;
        batch_test_tick(9000);assert(protocol_calls==tx&&popup.overlay==overlay&&machine_fault_key_equal(popup.key,foreign));
        /* The same real UI adapters preserve a runtime owner's original clear-before-START policy. */
        batch_test_reset(false);const machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};
        app_fault_recovery_report(jam);fault_popup_report_runtime_fault(2);batch_test_report_foreign(boot);tx=protocol_calls;
        assert(app_fault_recovery_prepare_start()&&protocol_calls==tx+1);
        app_fault_recovery_handle_clear_result(1);assert(!machine_fault_find(jam,NULL)&&machine_fault_find(foreign,NULL));
    }
    batch_test_reset(false);
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
    fault_test_progress("Batch missing FINISHED pauses display and retains counting transport");
    test_batch_pause_missing_finish();test_batch_pause_quick_cash_and_new_start();test_batch_pause_late_finish();
    test_batch_pause_hidden_rebuild_and_reset();test_batch_pause_other_warning_resume();
    fault_test_progress("Batch isolation from fresh sensor / boot guides");
    test_batch_real_sensor_boot_isolation();
    fault_test_progress("Batch real Popup / island replay and bounded controller recovery");
    test_batch_real_visuals();test_batch_real_popup_and_island();test_batch_real_generation_and_failure();
    fault_test_progress("Confirm without new START or controller reply");
    test_confirmed_pockets_without_new_start();
    test_confirm_send_failure_retry();
    test_old_failure_keeps_new_pocket_presentation();
    test_old_runtime_failure_keeps_new_start_guide();
    fault_test_progress("existing preference and recovery modes");
    test_setting_rejections();
    test_sort_feedback_language_and_navigation();
    test_operation_notice_lifecycle_and_consumers();
    test_independent_switches();
    test_recovery_switch_independence();
    test_popup_preference_only();
    test_late_recovery_results();
    fault_test_progress("pocket modes and hidden clear");
    test_pocket_recovery_modes();
    fault_test_progress("unresolved resume and recreate");
    test_unresolved_pocket_lifecycle();
    fault_test_progress("bounded failure and timeout ordering");
    test_pocket_recovery_failures();
    fault_test_progress("final lifecycle and cleanup");
    unsigned tx=protocol_calls;
    fault(7);assert(protocol_calls==tx+1); /* Full-pocket report automatically releases the latch. */
    tick(18000);assert(protocol_calls==tx+1); /* END coalesces while the first ACK is outstanding. */
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    fault(7);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING && protocol_calls==tx+1);
    tick(1500);lv_coord_t x=lv_obj_get_x(g_si_ctx.objects.title);
    fault(7);assert(lv_obj_get_x(g_si_ctx.objects.title)==x && protocol_calls==tx+1);
    tick(18000);assert(protocol_calls==tx+1);
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
    tx=protocol_calls;confirm(NULL);assert(protocol_calls==tx); /* Confirm coalesces the pending automatic request. */
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
    puts("PASS real notice+island+popup+recovery: Batch pauses running display without FINISHED, preserves transport/history for late 02..05, keeps ordinary warning resume; Batch recurring 06/04 reopens Confirm, duplicate unread preserves live UI, bounded no-ACK recovery, quick/late cash removal, pending-generation isolation, failed-send retry and hidden authorization;  actual Confirm without START suppresses 05/07/08 timeout/FAIL replay in both Popup modes; superseded FAIL preserves the new live guide; failed Confirm retries; non-Batch END cannot self-retry; full pockets 05/07/08 automatically handshake in both Popup modes and while hidden without Confirm/Clear/START; ACK retains physical guide, 51 clears only 07; pending coalescing, preference ownership, runtime policy and suspend/destroy retained");
}
