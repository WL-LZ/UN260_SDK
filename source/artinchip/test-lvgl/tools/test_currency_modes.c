#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "un260/currency/currency_state.h"
#include "un260/currency/currency_service.h"
#include "un260/currency/currency_reply.h"
#include "un260/app_service/setting_service.h"
#include "un260/app_service/app_setting_reply.h"
#include "un260/machine_state/machine_state.h"
#include "un260/protocol/mode_codec.h"
#include "un260/lv_system/ui_state_store.h"

#define PAGE07_CURR_MAX_ITEMS MAX_CURRENCIES
#define UI_PAGE_MAIN 1
#define UI_DATA_TOPIC_MACHINE_SETTINGS 1U
#define LV_EVENT_CLICKED 1
#include "currency_mode_types.inc"

static struct { page07_curr_model_state_t model; } g_page07_curr;
static void *curr_page = (void *)1;
static ui_state_page07_t saved;
static uint64_t tick;
static unsigned sends, saves, navigations, errors;
static uint8_t last_cmd, last_payload[8];
static uint16_t last_length;
static bool send_fails, mode_clear, clear_pending;
typedef struct { int code; intptr_t data; } lv_event_t;
static int lv_event_get_code(lv_event_t *e) { return e->code; }
static void *lv_event_get_user_data(lv_event_t *e) { return (void *)e->data; }

void page07_curr_model_save(void);
int page07_curr_model_find_abs_idx(const char *code);
int page07_curr_model_find_visible_pos(int index);
bool page07_curr_model_is_fixed(int index);
void page_07_curr_apply_switch_result(const currency_switch_result_t *result);
void page_07_curr_apply_mode_result(uint8_t requested_mode, bool success);
void page_07_curr_reset_pending_selection(void);
void page_07_curr_cancel_pending_selection(void);

uint64_t app_clock_monotonic_ms(void) { return tick; }
void uart_debug_printf(const char *format, ...) { (void)format; }
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
static void page_01_main_icon_feedback(const char *name) { (void)name; }
static void page_01_mode_switch_refre(void) {}
static void page_01_bottom_a_refresh_mode(bool animate) { (void)animate; }
static void page_01_curr_img_refre(void) {}
static void smart_island_refresh_summary(void) {}
static void page_01_main_scroll_reset(void) {}
static void show_currency_set_fail_popup(void) { errors++; }
static void show_start_fault_popup(uint8_t type, uint8_t code) { (void)type; (void)code; errors++; }
static void curr_set_left_info_by_abs(int index) { (void)index; }
static void curr_apply_selected_style(void) {}
static void curr_scroll_to_visible_idx(int index, bool animate) { (void)index; (void)animate; }
static void curr_refresh_right_views(void) {}
static bool app_setting_runtime_mode_clear_pending(void) { return mode_clear; }
static bool counting_action_clear_pending(void) { return clear_pending; }

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
    uint8_t reply[7] = {0, 0, 0, 0x04, status, value, 0};
    app_setting_reply_action_t action = app_setting_reply_handle_basic(
        0x04, reply, status == 3 ? 7 : 6);
    if (action & APP_SETTING_REPLY_ACTION_SCHEDULE_MODE_CLEAR) mode_clear = true;
}
static void reset(void)
{
    currency_state_reset(); setting_service_cancel_mode_request();
    currency_service_cancel_switch(); page_07_curr_reset_pending_selection();
    memset(&saved, 0, sizeof(saved)); memset(&g_page07_curr, 0, sizeof(g_page07_curr));
    machine_state_confirm_mode(MODE_MDC);
    page07_curr_model_load(); page07_curr_model_refresh_visible();
    sends = saves = navigations = errors = 0;
    tick = 0; send_fails = mode_clear = clear_pending = false;
}
static void test_catalog_and_model(void)
{
    reset(); char code[4];
    assert(currency_state_get_code(0, code) && strcmp(code, "AUT") == 0);
    assert(currency_state_get_code(1, code) && strcmp(code, "MUL") == 0);
    assert(currency_state_get_code(2, code) && strcmp(code, "USD") == 0);
    assert(strcmp(currency_state_display_code("MUL"), "MULTI") == 0);
    assert(currency_state_count() == 16);
    assert(!currency_service_request_switch(0, "AUT"));
    assert(!currency_service_request_switch(1, "MUL"));
    assert(!currency_state_confirm_active_code("MUL"));
    assert(!currency_state_confirm_active_selection(1, "MUL"));
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
static void test_requests_replies_and_boot(void)
{
    reset(); choose("MUL");
    assert(sends == 1 && last_cmd == 4 && last_length == 1 && last_payload[0] == 2);
    selected("CNY"); assert(setting_service_mode_is_pending());
    choose("AUT"); assert(sends == 1); /* shared request slot and transition block */
    reply_mode(2, 0); selected("CNY"); assert(!setting_service_mode_is_pending());
    assert(g_curr_mode_transition.kind == CURR_MODE_TRANSITION_NONE);
    choose("MUL"); reply_mode(1, 0); selected("MUL");
    assert(currency_state_multi_selected() && !currency_state_auto_selected());
    assert(machine_state_mode() == MODE_MDC && g_page07_curr.model.selected_abs_idx == 1);
    char code[4]; currency_state_get_effective_code(code); assert(strcmp(code, "MUL") == 0);
    currency_state_get_active_code(code); assert(strcmp(code, "CNY") == 0);
    assert(!currency_state_confirm_detected_code("USD"));

    choose("AUT"); assert(last_payload[0] == 1); selected("MUL");
    tick = 801; assert(setting_service_take_basic_timeouts() & SETTING_REQUEST_TIMEOUT_MODE);
    page_07_curr_cancel_pending_selection(); selected("MUL");
    reply_mode(1, 0); selected("MUL"); /* late unsolicited ACK does not change selection */
    send_fails = true; choose("AUT"); selected("MUL");
    assert(!setting_service_mode_is_pending() && g_curr_mode_transition.kind == CURR_MODE_TRANSITION_NONE);
    send_fails = false; choose("AUT"); reply_mode(1, 0); selected("AUT");
    assert(currency_state_confirm_detected_code("USD"));
    currency_state_get_effective_code(code); assert(strcmp(code, "USD") == 0);
    currency_state_begin_count_session(); currency_state_get_effective_code(code); assert(strcmp(code, "AUT") == 0);

    choose("MUL"); reply_mode(3, 1); selected("AUT");
    assert(!setting_service_mode_is_pending() && g_curr_mode_transition.kind == CURR_MODE_TRANSITION_NONE);
    reply_mode(3, 2); selected("MUL");
    assert(currency_state_confirm_active_code("USD")); selected("MUL"); /* boot real currency keeps feature */
    reply_mode(3, 4); selected("USD"); assert(machine_state_mode() == MODE_SDC);
    reply_mode(3, 0x7F); selected("USD"); assert(machine_state_mode() == MODE_SDC);
}
static void test_exit_special_and_grid(void)
{
    for (unsigned feature = 0; feature < 2; feature++) {
        reset(); machine_state_confirm_mode(MODE_SDC);
        choose(feature ? "MUL" : "AUT"); reply_mode(1, 0);
        mode_clear = false; unsigned before = sends;
        choose("EUR"); assert(sends == before + 1 && last_cmd == 4 && last_payload[0] == 4);
        selected(feature ? "MUL" : "AUT");
        reply_mode(2, 0); selected(feature ? "MUL" : "AUT");
        choose("EUR"); reply_mode(1, 0); selected("CNY");
        before = sends; page_07_curr_poll_selection(); assert(sends == before);
        mode_clear = false; clear_pending = true; page_07_curr_poll_selection(); assert(sends == before);
        clear_pending = false; page_07_curr_poll_selection();
        assert(sends == before + 1 && last_cmd == 3 && last_length == 3 && memcmp(last_payload, "EUR", 3) == 0);
        selected("CNY"); uint8_t frame[] = {0, 0, 0, 3, 1, 0};
        currency_reply_result_t result = currency_reply_handle(frame, sizeof(frame));
        assert(result.kind == CURRENCY_REPLY_SWITCH_SUCCESS);
        page_07_curr_apply_switch_result(&result.switch_result); selected("EUR");
    }
    reset(); g_page07_curr.model.view_mode = PAGE07_CURR_VIEW_GRID;
    lv_event_t event = {LV_EVENT_CLICKED, 1}; curr_grid_item_click_cb(&event);
    selected("CNY"); assert(g_page07_curr.model.selected_abs_idx == 3);
    reply_mode(2, 0); selected("CNY"); assert(g_page07_curr.model.selected_abs_idx == 3);
}
int main(void)
{
    test_catalog_and_model(); test_requests_replies_and_boot(); test_exit_special_and_grid();
    puts("PASS currency AUTO/MULTI ACK/boot/failure/timeout, manual exit ordering, fixed cards and legacy favorites");
    return 0;
}
