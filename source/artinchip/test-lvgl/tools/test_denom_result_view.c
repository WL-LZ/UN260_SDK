/* Actual LVGL, Main and island reuse the existing host display/assets.
 * The extracted runtime consumes real 0B frames and keeps actual timer owners. */
#include "lvgl/lvgl.h"
#define HOST_ISLAND_ONLY
#define main main_raster_entry_unused
#include "test_main_view.c"
#undef main
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_denom_reply.h"
#include "un260/counting/counting_report_sync.h"
#include "un260/app_service/app_counting_runtime.h"

static unsigned animation_starts, end_notifications;
lv_anim_t *__real_lv_anim_start(const lv_anim_t *);
lv_anim_t *__wrap_lv_anim_start(const lv_anim_t *animation)
{ ++animation_starts; return __real_lv_anim_start(animation); }
void __real_smart_island_notify_count_end(const char *);
void __wrap_smart_island_notify_count_end(const char *text)
{ ++end_notifications; __real_smart_island_notify_count_end(text); }

static void denom_boundary(counting_detail_state_t *detail,
                           counting_session_state_t *session,
                           counting_sim_t *data, uint8_t fill)
{
    uint8_t frame[16] = {0xfd,0xdf,16,0x0b}; memset(frame+4,fill,11);
    app_counting_runtime_handle_denom(detail,session,data,frame,sizeof(frame));
}
static void denom_row(counting_detail_state_t *detail,
                      counting_session_state_t *session,
                      counting_sim_t *data, unsigned value, unsigned pcs)
{
    uint8_t frame[16] = {0xfd,0xdf,16,0x0b}; char text[16];
    snprintf(text,sizeof(text),"%8u%3u",value,pcs); memcpy(frame+4,text,11);
    app_counting_runtime_handle_denom(detail,session,data,frame,sizeof(frame));
}
static void late_add_final(void)
{
    counting_sim_t *d = counting_data_mutable();
    counting_session_state_t session = {0};
    counting_detail_state_t detail = {0};
    fixture(false); d->denom_number=3;
    d->denom[0]=(denom_t){10,0,0}; d->denom[1]=(denom_t){50,10,500};
    d->denom[2]=(denom_t){100,0,0};
    d->total_pcs=10; d->total_amount=500; d->last_total_pcs=10; d->last_total_amount=500;
    machine_state_confirm_add(true); ui_refresh_main_page(); counting_report_begin(true,d);
    counting_denom_reply_begin_count(&detail);
    session.start_confirmed=true; session.phase=COUNTING_SESSION_IDLE;
    page_01_detail_section_set(PAGE_01_DETAIL_SECTION_A,true);
    page_01_main_detail_refresh_rows_only(); smart_island_notify_count_start();
    denom_boundary(&detail,&session,d,0); ui_frame_commit_flush();
    assert_main_denom_row(1,"50","0","0");
    denom_row(&detail,&session,d,50,11); ui_frame_commit_flush();
    assert_main_denom_row(1,"50","11","550");
    assert(d->total_pcs==10 && d->total_amount==500);
    /* A data was visible before the first total; later 0E owns totals. */
    uint8_t live[13]={0xfd,0xdf,13,0x0e,0,0,2,38,0,11,0,1,0};
    assert(counting_info_reply_handle(&session,d,live,sizeof(live),0).kind==COUNTING_INFO_REPLY_LIVE);
    assert_main_denom_row(1,"50","11","550");
    uint8_t end[13]={0xfd,0xdf,13,0x0e,0,0,0,0,0,0,0,2,0};
    assert(counting_info_reply_handle(&session,d,end,sizeof(end),0).kind==COUNTING_INFO_REPLY_FINISHED);
    session.analysis_valid_pcs=1;
    smart_island_update_counting(1,550); smart_island_set_count_analysis(1,0,0);
    smart_island_notify_count_end(NULL); page_01_main_refresh_totals(11,"550");
    tick(400);
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_RESULT);
    assert(!g_si_ctx.lifecycle.result_transition_pending);
    assert(!strncmp(lv_label_get_text(g_si_ctx.objects.title),"1 PCS",5));
    lv_timer_t *result_timer=g_si_ctx.lifecycle.result_timer; assert(result_timer);
    uint32_t last_run=result_timer->last_run,period=result_timer->period;
    unsigned before_animations=animation_starts,before_ends=end_notifications;
    unsigned before_timers=timers();

    /* Delayed, repeated and decreasing A rows remain absolute. They do not
     * revise 0E, restart the completion animation, or extend its hold timer. */
    denom_row(&detail,&session,d,50,12); ui_frame_commit_flush();
    assert_main_denom_row(1,"50","12","600");
    denom_row(&detail,&session,d,50,12);
    denom_row(&detail,&session,d,50,13);
    denom_row(&detail,&session,d,50,12);
    denom_boundary(&detail,&session,d,255); ui_frame_commit_flush();
    assert_main_denom_row(1,"50","12","600");
    assert(d->total_pcs==11 && d->total_amount==550);
    assert(d->last_total_pcs==10 && session.analysis_valid_pcs==1);
    assert(session.last_result.pcs==11 && session.last_result.amount==550);
    assert(session.history_record.pcs==11 && session.history_record.amount==550);
    assert(g_si_ctx.counting.pcs==1 && g_si_ctx.counting.amount==550);
    assert(g_si_ctx.text.analysis_valid && g_si_ctx.text.analysis_valid_pcs==1);
    assert(!strncmp(lv_label_get_text(g_si_ctx.objects.title),"1 PCS",5));
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_RESULT);
    assert(!g_si_ctx.lifecycle.result_transition_pending);
    assert(g_si_ctx.lifecycle.result_timer==result_timer);
    assert(result_timer->last_run==last_run && result_timer->period==period);
    assert(timers()==before_timers);
    assert(animation_starts==before_animations && end_notifications==before_ends);
    write_bmp("denom-late-add-result");
    /* Orphan rows/END have no stream owner and cannot revise the screen. */
    denom_row(&detail,&session,d,100,9); denom_boundary(&detail,&session,d,255);
    ui_frame_commit_flush(); assert_main_denom_row(1,"50","12","600");
    assert(d->denom[2].pcs==0);
    assert(g_si_ctx.lifecycle.result_timer==result_timer && result_timer->last_run==last_run);
    assert(animation_starts==before_animations && end_notifications==before_ends);
    tick(1600); assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    assert(!g_si_ctx.lifecycle.result_timer);
    puts("PASS real Main A: live 0B before first 0E; absolute 11/12/12/13/12 rows; 0E retains 11/550 and ADD completion 1; history candidate, RESULT hold timer, animation and end event unchanged");
}

int main(void)
{
    lv_init();
    static lv_color_t pixels[1280*40]; static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer,pixels,NULL,1280*40);
    static lv_disp_drv_t display; lv_disp_drv_init(&display);
    display.hor_res=1280;display.ver_res=400;display.draw_buf=&buffer;display.flush_cb=flush;
    assert(lv_disp_drv_register(&display));
    lv_img_decoder_t *decoder=lv_img_decoder_create();assert(decoder);
    lv_img_decoder_set_info_cb(decoder,host_image_info);lv_img_decoder_set_open_cb(decoder,host_image_open);
    lv_img_decoder_set_close_cb(decoder,host_image_close);
    fixture(false);assert(currency_state_confirm_active_code("USD"));
    ui_lang_set(LANGUAGE_EN);ui_main_create(lv_scr_act());tick(400);
    late_add_final();
    counting_report_shutdown();ui_main_destroy();
    counting_data_clear_serials(counting_data_mutable());counting_data_clear_errors(counting_data_mutable());
    lv_img_decoder_delete(decoder);lv_deinit();host_external_assets_release();
    return 0;
}
