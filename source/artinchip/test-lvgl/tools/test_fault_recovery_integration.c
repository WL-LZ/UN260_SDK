/* Real LVGL, fault popup, island and recovery service; capture UART only. */
#include "lvgl/lvgl.h"
#define HOST_REAL_FAULT
#define HOST_ISLAND_ONLY
#define main main_raster_entry_unused
#include "test_main_view.c"
#undef main
#define box fault_test_box
#define label fault_test_label
#include "un260/lv_components/lv_fault_popup.c"
#undef box
#undef label
#include "un260/app_service/app_fault_recovery.h"
const lv_font_t *ui_message_font(const lv_font_t *base) { return base; }
void ui_notice_set_suspended(uint32_t reason,bool value) {(void)reason;(void)value;}

uint32_t __wrap_app_clock_uptime_ms(void){return lv_tick_get();}
uint64_t __wrap_app_clock_monotonic_ms(void){return lv_tick_get();}
static bool recovery_confirm(machine_fault_key_t key){app_fault_recovery_confirm(key);return false;}
void ui_notice_clear(const char *key){(void)key;}
static void ack(uint8_t result){uint8_t b[]={0xFD,0xDF,6,0x3D,result,0};app_fault_recovery_handle_reply(b,6);}
static void fault(uint8_t code)
{
    app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_START,2,code});
    fault_popup_report_start_fault(2,code);
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
    fault_popup_set_confirm_handler(recovery_confirm);
    /* No-note owns an inspectable yellow identity, never a stale door fault. */
    fault(9);app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_START,1,2});
    fault_popup_report_start_no_note();tick(1500);
    assert(g_si_ctx.warning.fault.valid&&g_si_ctx.warning.fault.fault_type==1&&g_si_ctx.warning.fault.code==2);
    assert(g_si_ctx.warning.level==SMART_ISLAND_WARNING_LEVEL_WARNING);
    lv_event_send(g_si_ctx.objects.root,LV_EVENT_CLICKED,NULL);
    assert(popup.key.type==1&&popup.key.code==2&&!strcmp(lv_label_get_text(popup.title),"No banknotes detected"));
    unsigned tx=protocol_calls;confirm(NULL);assert(protocol_calls==tx+1&&!fault_popup_is_showing());ack(1);
    /* Popup ON: Confirm clears once; removal before Confirm also clears.
       Confirmation never fabricates START or deletes a physical fault. */
    machine_fault_clear();app_fault_recovery_clear();fault_popup_set_auto_enabled(true);fault(7);
    tx=protocol_calls;tick(5000);app_fault_recovery_poll();assert(protocol_calls==tx);
    confirm(NULL);assert(protocol_calls==tx+1&&!fault_popup_is_showing());
    assert(machine_fault_find((machine_fault_key_t){MACHINE_FAULT_START,2,7},NULL));ack(1);
    app_fault_recovery_stacker_cleared();assert(protocol_calls==tx+1&&!machine_fault_count());
    fault(7);assert(fault_popup_is_showing());tx=protocol_calls;
    app_fault_recovery_stacker_cleared();assert(protocol_calls==tx+1&&!fault_popup_is_showing());
    app_fault_recovery_stacker_cleared();assert(protocol_calls==tx+1);ack(1);
    /* Repeated asserted fault after Confirm reopens; duplicate while open
       preserves the current step/animation instead of restarting it. */
    fault(9);confirm(NULL);ack(1);fault(9);assert(fault_popup_is_showing());
    tick(500);uint32_t elapsed=popup.model.elapsed_ms;fault(9);assert(popup.model.elapsed_ms==elapsed);
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();
    machine_fault_key_t batch={MACHINE_FAULT_BATCH,0,4};
    app_fault_recovery_report(batch);fault_popup_report_batch_full();
    assert(!strcmp(lv_label_get_text(popup.title),"Batch full"));
    assert(!strcmp(lv_label_get_text(popup.code),"0x06/0x04"));
    tx=protocol_calls;confirm(NULL);assert(protocol_calls==tx+1&&!fault_popup_is_showing());ack(1);
    app_fault_recovery_report(batch);fault_popup_report_batch_full();assert(fault_popup_is_showing());
    fault_popup_report_runtime_fault(0);assert(fault_popup_is_showing()&&popup.key.source==MACHINE_FAULT_BATCH);
    tx=protocol_calls;app_fault_recovery_stacker_cleared();assert(protocol_calls==tx+1&&!fault_popup_is_showing());ack(1);
    /* Popup OFF: unresolved conditions persist over many animation cycles
       without any unsolicited UART traffic. END before/after fault is equal. */
    fault_popup_set_auto_enabled(false);
    for(unsigned order=0;order<2;++order) {
        app_fault_recovery_count_started();smart_island_notify_count_start();
        if(order) {app_fault_recovery_count_finished();smart_island_notify_count_end(NULL);}
        app_fault_recovery_report(batch);fault_popup_report_batch_full();
        if(!order) {app_fault_recovery_count_finished();smart_island_notify_count_end(NULL);}
        tx=protocol_calls;
        for(unsigned cycle=0;cycle<20;++cycle) {
            tick(1000);app_fault_recovery_poll();
            assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING);
            assert(g_si_ctx.warning.fault.source==MACHINE_FAULT_BATCH);
            assert(!g_si_ctx.lifecycle.result_timer&&!g_si_ctx.lifecycle.count_session_active);
            assert(protocol_calls==tx);
            assert(lv_obj_get_style_transform_width(g_si_ctx.objects.root,0)==22);
        }
        smart_island_restore_idle();
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING);
        smart_island_notify_warning("Unrelated notice");
        assert(g_si_ctx.warning.fault.source==MACHINE_FAULT_BATCH);
        /* A separate runtime fault clears without clearing the batch latch. */
        app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,2});
        fault_popup_report_runtime_fault(2);
        app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,0});
        fault_popup_report_runtime_fault(0);
        assert(g_si_ctx.warning.fault.source==MACHINE_FAULT_BATCH);
        /* UI destruction/recreation does not fabricate recovery either. */
        smart_island_destroy();smart_island_create(lv_scr_act());tick(500);
        assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING&&g_si_ctx.warning.fault.source==MACHINE_FAULT_BATCH);
        app_fault_recovery_stacker_cleared();assert(protocol_calls==tx+1);ack(1);
        tick(5000);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    }
    /* Hidden pages pause the visual only; no timer-driven clear is allowed. */
    fault_popup_set_auto_enabled(false);fault(9);tx=protocol_calls;
    page_01_main_suspend();tick(2100);app_fault_recovery_poll();assert(protocol_calls==tx);
    assert(page_01_main_resume());tick(18000);app_fault_recovery_poll();assert(protocol_calls==tx);
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING&&g_si_ctx.warning.marquee_running);
    /* Explicit clear failure must leave the original condition inspectable. */
    assert(fault_popup_show_key((machine_fault_key_t){MACHINE_FAULT_START,2,9}));
    confirm(NULL);assert(protocol_calls==tx+1);tick(2100);app_fault_recovery_poll();
    tick(10000);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING);
    assert(g_si_ctx.warning.fault.code==9);
    app_fault_recovery_clear();fault_popup_clear_runtime();
    /* Generic messages still finish normally; persistent motion is fault-only. */
    smart_island_notify_warning("Ordinary message");tick(20000);
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    ui_main_destroy();
    counting_data_clear_serials(counting_data_mutable());counting_data_clear_errors(counting_data_mutable());
    lv_img_decoder_delete(decoder);lv_deinit();host_external_assets_release();
    puts("PASS real island+popup+recovery: yellow no-note click, Confirm/removal handshake, repeated reports, batch guide, runtime-clear isolation, persistent warning across cycles/end/order/recreation/suspension, no timed clear");
}
