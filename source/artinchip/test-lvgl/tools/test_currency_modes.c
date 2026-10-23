#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "un260/currency/currency_state.h"
#include "un260/currency/currency_service.h"
#include "un260/currency/currency_reply.h"
#include "un260/app_service/setting_service.h"
#include "un260/app_service/count_mode_service.h"
#include "un260/boot/boot_service.h"
#include "un260/app_service/app_setting_reply.h"
#include "un260/machine_state/machine_state.h"
#include "un260/protocol/mode_codec.h"
#include "un260/lv_system/ui_state_store.h"
#include "test_notice_sink.h"

#define PAGE07_CURR_MAX_ITEMS MAX_CURRENCIES
#define UI_PAGE_MAIN 1
#define UI_DATA_TOPIC_MACHINE_SETTINGS 1U
#define LV_EVENT_CLICKED 1
#include "currency_mode_types.inc"

static struct { page07_curr_model_state_t model; } g_page07_curr;
static void *curr_page = (void *)1;
static ui_state_page07_t saved;
static uint64_t tick;
static unsigned sends, saves, navigations;
static uint8_t last_cmd, last_payload[8];
static uint16_t last_length;
static bool send_fails, mode_clear, running, applying;
bool app_add_runtime_busy(void) { return false; }
bool app_command_runtime_count_start_busy(void) { return running; }
bool workspace_service_applying(void) { return applying; }
boot_stage_t boot_service_get_stage(void) { return BOOT_STAGE_DONE; }
typedef struct { int code; intptr_t data; } lv_event_t;
static int lv_event_get_code(lv_event_t *e) { return e->code; }
static void *lv_event_get_user_data(lv_event_t *e) { return (void *)e->data; }

void page07_curr_model_save(void);
int page07_curr_model_find_abs_idx(const char *code);
int page07_curr_model_find_visible_pos(int index);
bool page07_curr_model_is_fixed(int index);
void page_07_curr_apply_switch_result(const currency_switch_result_t *result);

uint64_t app_clock_monotonic_ms(void) { return tick; }
void uart_debug_printf(const char *format, ...) { (void)format; }
/* Work-mode switching is outside this Currency request fixture. Fail if a
 * currency path accidentally starts issuing that separate operation. */
bool work_mode_service_request(uint8_t mode)
{
    (void)mode;
    assert(!"Currency selection must not request a work-mode change");
    return false;
}
int protocol_send(uint8_t cmd, const uint8_t *payload, uint16_t length)
{
    assert(length <= sizeof(last_payload));
    sends++; last_cmd = cmd; last_length = length;
    memcpy(last_payload, payload, length);
    return send_fails ? -1 : (int)length;
}
void ui_state_page07_get(ui_state_page07_t *state) { *state = saved; }
void ui_state_save_page07(const ui_state_page07_t *state) { saved = *state; saves++; }
static void ui_manager_switch(int page) { assert(page == UI_PAGE_MAIN); navigations++; }
static void ui_manager_publish_data_changed(uint32_t topics) { (void)topics; }
static void page_01_mode_switch_refre(void) {}
static void page_01_bottom_a_refresh_mode(bool animate) { (void)animate; }
static void page_01_curr_img_refre(void) {}
static void smart_island_refresh_summary(void) {}
static void page_01_main_scroll_reset(void) {}
static void app_setting_notice_result(const char *key,const char *title,bool success)
{ui_notice_post(success?UI_NOTICE_SUCCESS:UI_NOTICE_ERROR,key,title,NULL);}
static void curr_set_left_info_by_abs(int index) { (void)index; }
static void curr_apply_selected_style(void) {}
static void curr_scroll_to_visible_idx(int index, bool animate) { (void)index; (void)animate; }
static void curr_refresh_right_views(void) {}

#include "currency_mode_under_test.inc"

static void selected(const char *expected)
{
    char code[4];
    currency_state_get_selected_code(code);
    assert(strcmp(code, expected) == 0);
}
static void choose(const char *code)
{
    uint8_t index;
    assert(currency_state_find_code(code, &index));
    curr_select_and_exit_abs(index);
}
static void reply_mode(uint8_t status, uint8_t value)
{
    uint8_t reply[7] = {0xFD, 0xDF, status == 3 ? 7 : 6, 0x04, status, value, 0};
    app_setting_reply_action_t action = app_setting_reply_handle_basic(
        0x04, reply, status == 3 ? 7 : 6);
    if (action & APP_SETTING_REPLY_ACTION_SCHEDULE_MODE_CLEAR) mode_clear = true;
}
static void reset(void)
{
    currency_state_reset(); setting_service_cancel_mode_request();
    currency_service_cancel_switch();
    memset(&saved, 0, sizeof(saved)); memset(&g_page07_curr, 0, sizeof(g_page07_curr));
    machine_state_confirm_mode(MODE_MDC);
    count_mode_service_currency_sync();count_mode_service_mode_sync();
    page07_curr_model_load(); page07_curr_model_refresh_visible();
    sends = saves = navigations = 0;
    tick += 1000; send_fails = mode_clear = running = applying = false;
}
static void test_catalog_and_model(void)
{
    reset(); char code[4];
    assert(currency_state_get_code(0, code) && strcmp(code, "AUT") == 0);
    assert(currency_state_get_code(1, code) && strcmp(code, "MUL") == 0);
    assert(currency_state_get_code(2, code) && strcmp(code, "USD") == 0);
    assert(strcmp(currency_state_display_code("MUL"), "MULTI") == 0);
    assert(currency_state_count() == 16);
    assert(currency_service_request_switch(0, "AUT"));
    currency_service_cancel_switch();
    assert(currency_service_request_switch(1, "MUL"));
    currency_service_cancel_switch();
    assert(!currency_state_confirm_active_code("MUL"));
    assert(!currency_state_confirm_active_selection(2, "MUL"));
    assert(currency_state_code_to_item("MUL") == CURR_COUNT);

    saved.fav_only = 1; saved.fav_count = 4; saved.selected_abs_idx = 1;
    memcpy(saved.fav_codes[0], "AUT", 4); memcpy(saved.fav_codes[1], "MUL", 4);
    memcpy(saved.fav_codes[2], "EUR", 4); memcpy(saved.fav_codes[3], "USD", 4);
    page07_curr_model_load(); page07_curr_model_refresh_visible();
    assert(g_page07_curr.model.favorite_count == 2);
    assert(g_page07_curr.model.selected_abs_idx == 3); /* old saved index cannot override CNY */
    const int order[] = {0, 1, 2, 4};
    assert(g_page07_curr.model.visible_count == 4);
    assert(memcmp(g_page07_curr.model.visible_indices, order, sizeof(order)) == 0);
    page07_curr_model_toggle_favorite(0); page07_curr_model_toggle_favorite(1);
    assert(saves == 0 && g_page07_curr.model.favorite_count == 2);
    g_page07_curr.model.favorite_count = 0; page07_curr_model_refresh_visible();
    assert(g_page07_curr.model.visible_count == 2);
    assert(g_page07_curr.model.visible_indices[0] == 0 && g_page07_curr.model.visible_indices[1] == 1);

    assert(currency_state_confirm_multi_selection());
    currency_state_begin_list_sync();
    assert(!currency_state_append_list_code(1, "AUT"));
    assert(!currency_state_append_list_code(1, "MUL"));
    for (uint8_t i = 0; i < CONTROLLER_MAX_CURRENCIES; i++) {
        char real[4] = {'X', (char)('A' + i / 26), (char)('A' + i % 26), 0};
        assert(currency_state_append_list_code((uint8_t)(i + 1), real));
    }
    assert(currency_state_finish_list_sync()); assert(currency_state_count() == MAX_CURRENCIES);
    selected("MUL"); assert(currency_state_active_index() == 1);
    assert(currency_state_get_code(2, code) && strcmp(code, "XAA") == 0);
    assert(currency_state_get_code(33, code) && strcmp(code, "XBF") == 0);
    currency_state_begin_list_sync(); assert(currency_state_append_list_code(2, "EUR"));
    assert(!currency_state_finish_list_sync()); assert(currency_state_count() == MAX_CURRENCIES);
    selected("MUL");
    assert(currency_state_leave_special_selection()); selected("XAA");
    currency_state_confirm_active_index(31); assert(currency_state_active_index() == 33);
    currency_state_confirm_active_index(UINT8_MAX); assert(currency_state_active_index() == 33);
}

static void currency_ack(uint8_t status)
{
    uint8_t frame[] = {0xFD,0xDF,6,3,status,0};
    currency_reply_result_t r = currency_reply_handle(frame,sizeof(frame));
    if (r.kind == CURRENCY_REPLY_SWITCH_SUCCESS || r.kind == CURRENCY_REPLY_SWITCH_FAILURE)
        { count_mode_service_currency_result(&r.switch_result);page_07_curr_apply_switch_result(&r.switch_result); }
}
static void boot_currency(const char *code)
{
    uint8_t frame[] = {0xFD,0xDF,9,3,3,0,0,0,0};
    memcpy(frame+5,code,3);
    assert(currency_reply_handle(frame,sizeof(frame)).kind == CURRENCY_REPLY_BOOT_ACTIVE);
    count_mode_service_currency_sync();
}
static void test_requests_replies_and_boot(void)
{
    reset(); unsigned initial_notices=test_notice_count; choose("MUL");
    assert(test_notice_count==initial_notices);
    assert(sends==1 && last_cmd==3 && last_length==3 && !memcmp(last_payload,"MUL",3));
    selected("CNY"); assert(currency_service_switch_pending());
    choose("AUT"); assert(sends==1);
    unsigned previous_nav=navigations, previous_notices=test_notice_count;
    currency_ack(2); selected("CNY");
    assert(navigations==previous_nav+1 && test_notice_count==previous_notices && !test_notice_visible);
    choose("MUL"); currency_ack(1); selected("MUL");
    assert(machine_state_mode()==MODE_MDC);
    choose("AUT"); selected("MUL");
    tick+=801; currency_switch_result_t result;
    assert(currency_service_take_switch_timeout(&result));
    count_mode_service_currency_result(&result);
    page_07_curr_apply_switch_result(&result); selected("MUL");
    assert(result.timeout&&test_notice_kind==UI_NOTICE_WARNING);
    currency_ack(1); selected("MUL");
    send_fails=true; choose("AUT"); selected("MUL");
    assert(test_notice_kind==UI_NOTICE_ERROR&&!strcmp(test_notice_title,"Currency not sent"));
    assert(!currency_service_switch_pending());
    send_fails=false; choose("AUT");
    assert(last_cmd==3 && last_length==3 && !memcmp(last_payload,"AUT",3));
    currency_ack(1); selected("AUT");
    uint8_t detected[8]={0xfd,0xdf,8,0x49,'C','N','Y',0xa1};
    char decoded[4];
    assert(currency_reply_detected_code(detected,8,decoded));
    assert(!strcmp(decoded,"CNY"));
    assert(currency_state_confirm_detected_code(decoded));
    assert(!currency_reply_detected_code(detected,7,decoded));
    assert(!currency_reply_detected_code(detected,24,decoded));
    detected[4]='c';assert(!currency_reply_detected_code(detected,8,decoded));
    detected[4]='C';detected[3]=0x50;
    assert(currency_reply_detected_code(detected,8,decoded));
    assert(currency_state_confirm_detected_code("USD"));
    char code[4]; currency_state_get_effective_code(code); assert(!strcmp(code,"USD"));
    assert(!currency_state_confirm_detected_code("ZZZ"));
    currency_state_get_effective_code(code); assert(!strcmp(code,"USD"));
    assert(currency_state_confirm_detected_code("EUR"));
    currency_state_get_effective_code(code); assert(!strcmp(code,"EUR"));
    currency_state_clear_detected_result(); currency_state_get_effective_code(code); assert(!strcmp(code,"AUT"));
    /* Work mode is independent; it must never leave AUT/MUL implicitly. */
    assert(setting_service_request_mode(MODE_SDC)); reply_mode(1,0);
    selected("AUT"); assert(machine_state_mode()==MODE_SDC);
    reply_mode(3,3); selected("AUT"); assert(machine_state_mode()==MODE_MDC);
    reply_mode(3,1); selected("AUT"); /* obsolete special mode ignored */
    boot_currency("MUL"); selected("MUL");
    choose("EUR"); boot_currency("AUT"); selected("AUT");
    assert(!currency_service_switch_pending()); currency_ack(1); selected("AUT");
    boot_currency("USD"); selected("USD");
}
static void test_exit_special_and_grid(void)
{
    for (unsigned feature=0;feature<2;feature++) {
        reset(); machine_state_confirm_mode(MODE_SDC);
        choose(feature?"MUL":"AUT"); currency_ack(1); if(feature){count_mode_service_poll();reply_mode(1,0);}
        unsigned before=sends;
        choose("EUR");
        assert(sends==before+1 && last_cmd==3 && last_length==3 && !memcmp(last_payload,"EUR",3));
        selected(feature?"MUL":"AUT");
        currency_ack(2); selected(feature?"MUL":"AUT");
        choose("EUR"); currency_ack(1); selected("EUR");
        assert(machine_state_mode()==(feature?MODE_MDC:MODE_SDC));
    }
    reset(); g_page07_curr.model.view_mode=PAGE07_CURR_VIEW_GRID;
    lv_event_t event={LV_EVENT_CLICKED,1};curr_grid_item_click_cb(&event);
    selected("CNY");currency_ack(2);selected("CNY");
    assert(g_page07_curr.model.selected_abs_idx==3);
    /* Success after the view is destroyed updates model without navigation. */
    choose("AUT");curr_page=NULL;unsigned before=navigations;
    currency_ack(1);selected("AUT");assert(navigations==before);curr_page=(void*)1;
}
static void test_policy_transitions(void)
{
    const char *codes[]={"USD","EUR","AUT","MUL"};
    const uint8_t modes[]={MODE_MDC,MODE_SDC,MODE_CNT};
    for(unsigned a=0;a<4;a++)for(unsigned m=0;m<3;m++)for(unsigned b=0;b<4;b++) {
        if(!count_mode_allowed(codes[a],modes[m]))continue;
        reset();boot_currency(codes[a]);machine_state_confirm_mode(modes[m]);count_mode_service_mode_sync();
        unsigned before=sends;choose(codes[b]);
        if(a==b){assert(sends==before);continue;}
        bool change=!count_mode_allowed(codes[b],modes[m]);
        assert(sends==before+1 && last_cmd==3);
        assert(!memcmp(last_payload,codes[b],3));
        unsigned notices=test_notice_count;choose("EUR");assert(sends==before+1&&test_notice_count==notices);
        currency_ack(1);selected(codes[b]);
        if(change){assert(machine_state_mode()==modes[m]);count_mode_service_poll();
            assert(sends==before+2&&last_cmd==4&&last_payload[0]==3);
            reply_mode(1,0);assert(!mode_clear);}
        assert(!count_mode_service_busy());
        assert(machine_state_mode()==(change?MODE_MDC:modes[m]));
        assert(!count_mode_service_request(MODE_NONE));
        if(b==3)assert(!count_mode_service_request(MODE_SDC));
        if(b>=2)assert(!count_mode_service_request(MODE_CNT));
        before=sends;assert(count_mode_service_request(machine_state_mode()));assert(sends==before);
    }
    for(unsigned c=0;c<4;c++)for(unsigned m=0;m<3;m++){
        if(!count_mode_allowed(codes[c],modes[m]))continue;
        reset();boot_currency(codes[c]);machine_state_confirm_mode(modes[m]);count_mode_service_mode_sync();
        unsigned before=sends;uint8_t expected=c==3?MODE_MDC:m==0?MODE_SDC:(c<2&&m==1)?MODE_CNT:MODE_MDC;
        page_01_mode_cycle();
        assert(sends==before+(expected!=modes[m]));
        if(expected!=modes[m]){assert(last_cmd==4);reply_mode(1,0);assert(machine_state_mode()==expected);}
    }
    reset();boot_currency("AUT");assert(currency_state_confirm_detected_code("USD"));
    assert(!count_mode_service_request(MODE_CNT));
    assert(count_mode_next("AUT",MODE_SDC)==MODE_MDC);
    assert(count_mode_next("MUL",MODE_MDC)==MODE_MDC);
    assert(count_mode_next("USD",MODE_SDC)==MODE_CNT);
    assert(count_mode_next("USD",MODE_CNT)==MODE_MDC);
    reset();machine_state_confirm_mode(MODE_CNT);send_fails=true;choose("MUL");
    assert(last_cmd==3&&!count_mode_service_busy()&&!currency_service_switch_pending());
    assert(test_notice_kind==UI_NOTICE_ERROR&&!strcmp(test_notice_key,"settings.currency"));
    send_fails=false;
    /* Currency rejection/timeout must never send a mode command. */
    reset();machine_state_confirm_mode(MODE_CNT);choose("MUL");assert(last_cmd==3);
    unsigned before=sends;currency_ack(2);count_mode_service_poll();
    assert(sends==before&&!count_mode_service_busy());selected("CNY");assert(machine_state_mode()==MODE_CNT);
    choose("AUT");tick+=801;currency_switch_result_t result;assert(currency_service_take_switch_timeout(&result));
    count_mode_service_currency_result(&result);before=sends;count_mode_service_poll();assert(sends==before);
    choose("CNY");assert(sends==before+1&&last_cmd==3);currency_ack(1);
    /* Mode follow-on failure preserves the successful currency and allows retry. */
    choose("MUL");currency_ack(1);count_mode_service_poll();assert(last_cmd==4);
    reply_mode(2,0);selected("MUL");assert(machine_state_mode()==MODE_CNT&&!count_mode_service_busy());
    before=sends;choose("MUL");assert(last_cmd==3&&sends==before+1);currency_ack(1);count_mode_service_poll();
    tick+=801;assert(setting_service_take_basic_timeouts()&SETTING_REQUEST_TIMEOUT_MODE);
    count_mode_service_mode_result(false,true);assert(!count_mode_service_busy());selected("MUL");
    before=sends;reply_mode(1,0);assert(sends==before&&machine_state_mode()==MODE_CNT);
    choose("MUL");currency_ack(1);count_mode_service_poll();reply_mode(1,0);assert(machine_state_mode()==MODE_MDC);
    /* A subsequent direct/manual request is not mistaken for the follow-on. */
    choose("USD");currency_ack(1);assert(count_mode_service_request(MODE_SDC));mode_clear=false;
    reply_mode(1,0);assert(mode_clear&&count_mode_service_result_may_clear());
    reset();machine_state_confirm_mode(MODE_SDC);choose("MUL");currency_ack(1);send_fails=true;
    count_mode_service_poll();assert(!count_mode_service_busy());selected("MUL");assert(machine_state_mode()==MODE_SDC);
    send_fails=false;choose("MUL");currency_ack(1);count_mode_service_poll();reply_mode(1,0);
    /* Physical START cancels only unsent mode work; a sent result cannot clear the new run. */
    reset();machine_state_confirm_mode(MODE_CNT);choose("MUL");before=sends;
    count_mode_service_count_started();running=true;currency_ack(1);count_mode_service_poll();
    assert(sends==before&&!count_mode_service_busy()&&!mode_clear);running=false;
    choose("MUL");currency_ack(1);count_mode_service_poll();before=sends;
    count_mode_service_count_started();running=true;reply_mode(1,0);count_mode_service_poll();
    assert(sends==before&&!count_mode_service_busy()&&!mode_clear);running=false;
    reset();applying=true;before=sends;choose("MUL");assert(sends==before);applying=false;
    puts("PASS currency/mode matrix, same-value no-op, AUTO detection, failures, timeout retry, START cancellation and profile conflict");
}
static void test_boot_policy(bool mode_first)
{
    currency_state_reset();tick=100;
    if(mode_first){reply_mode(3,4);count_mode_service_poll();assert(sends==0);boot_currency("MUL");}
    else {boot_currency("MUL");count_mode_service_poll();assert(sends==0);reply_mode(3,4);}
    running=true;count_mode_service_poll();assert(sends==0);running=false;
    count_mode_service_poll();assert(sends==1&&last_cmd==4&&last_payload[0]==3);
    tick+=801;assert(setting_service_take_basic_timeouts()&SETTING_REQUEST_TIMEOUT_MODE);
    count_mode_service_mode_result(false,true);
    for(unsigned i=0;i<100;i++)count_mode_service_poll();assert(sends==1&&!count_mode_service_busy());
    assert(count_mode_service_request(MODE_MDC));reply_mode(1,0);assert(machine_state_mode()==MODE_MDC);
    assert(count_mode_service_confirmed());puts("PASS boot sync order, defer during run, one automatic correction, timeout and explicit recovery");
}
int main(int argc,char **argv)
{
    if(argc>1){test_boot_policy(!strcmp(argv[1],"mode-first"));return 0;}
    test_catalog_and_model();test_requests_replies_and_boot();test_exit_special_and_grid();test_policy_transitions();
    send_fails=false;unsigned before=sends;setting_batch_result_t batch;
    assert(!setting_service_request_batch_number(200,false,200));
    assert(!setting_service_request_batch_number(0,false,200));assert(sends==before);
    assert(setting_service_request_batch_number(199,false,200));assert(last_cmd==6&&last_payload[0]==199);
    assert(setting_service_batch_take_result(1,&batch)&&batch.target.enable&&batch.target.num==199);
    assert(setting_service_request_batch_switch(false,200,true,199));assert(last_payload[0]==200);
    assert(setting_service_batch_take_result(1,&batch)&&!batch.target.enable);
    puts("PASS Batch wire boundary: 200 reserved for OFF, invalid enabled values send nothing, 199 and OFF round trip");
    puts("PASS AUT/MUL ordinary 0x03, ACK/failure/timeout/send failure, boot sync, independent work mode, lifecycle");
    return 0;
}
