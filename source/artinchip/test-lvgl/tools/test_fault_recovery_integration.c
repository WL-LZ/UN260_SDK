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
    unsigned tx=protocol_calls;
    fault(7);assert(protocol_calls==tx+1); /* legacy pocket begin handshake */
    tick(18000);assert(protocol_calls==tx+2);
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    fault(7);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING && protocol_calls==tx+3);
    tick(1500);lv_coord_t x=lv_obj_get_x(g_si_ctx.objects.title);
    fault(7);assert(lv_obj_get_x(g_si_ctx.objects.title)==x && protocol_calls==tx+3);
    tick(18000);assert(protocol_calls==tx+4);
    fault(7);app_fault_recovery_stacker_cleared();tx=protocol_calls;
    tick(18000);assert(protocol_calls==tx && g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    /* Door fault repeats after a completed notice, without restarting a live one. */
    fault(9);tick(18000);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    fault(9);assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_WARNING);
    /* No-note cannot capture that door identity or acknowledge it. */
    app_fault_recovery_clear();fault_popup_report_start_no_note();tx=protocol_calls;
    assert(!g_si_ctx.warning.fault.valid);tick(18000);app_fault_recovery_poll();assert(protocol_calls==tx);
    /* The displayed identity survives unrelated unread boot faults. */
    fault_popup_record_boot_result(5,2);fault(7);
    assert(g_si_ctx.warning.fault.source==MACHINE_FAULT_START && g_si_ctx.warning.fault.code==7);
    lv_event_send(g_si_ctx.objects.root,LV_EVENT_CLICKED,NULL);
    assert(popup.key.source==MACHINE_FAULT_START && popup.key.code==7);
    hide_fault_popup();
    fault_popup_set_auto_enabled(true);hide_fault_popup();
    assert(fault_popup_show_key((machine_fault_key_t){MACHINE_FAULT_START,2,7}));
    tx=protocol_calls;confirm(NULL);assert(protocol_calls==tx); /* local read acknowledgement */
    hide_fault_popup();machine_fault_clear();app_fault_recovery_clear();smart_island_faults_changed();
    fault_popup_set_auto_enabled(false);fault(7);tx=protocol_calls;
    page_01_main_suspend();tick(18000);app_fault_recovery_poll();assert(protocol_calls==tx);
    smart_island_notify_warning("A settings warning while Main is hidden");
    assert(g_si_ctx.warning.fault.valid && g_si_ctx.warning.fault.code==7);
    app_fault_recovery_clear();fault_popup_clear_runtime();assert(page_01_main_resume());tick(18000);
    assert(protocol_calls==tx && g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    fault(7);tx=protocol_calls;ui_main_destroy();tick(18000);app_fault_recovery_poll();assert(protocol_calls==tx);
    counting_data_clear_serials(counting_data_mutable());counting_data_clear_errors(counting_data_mutable());
    lv_img_decoder_delete(decoder);lv_deinit();host_external_assets_release();
    puts("PASS real island+popup+recovery: repeated full/door, live duplicate, source isolation, no-note, Confirm no TX, recovery, suspend, destroy");
}
