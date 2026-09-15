#include "app_setting_runtime.h"

#include <stddef.h>

#include "un260/lv_system/app_clock.h"
#include "un260/app_service/app_setting_reply.h"
#include "un260/app_service/setting_service.h"
#include "un260/cfd/cfd.h"
#include "un260/currency/currency_service.h"
#include "un260/counting/counting_data_store.h"
#include "un260/data_collection/data_collection.h"
#include "un260/lv_components/lv_components.h"
#include "un260/lv_core/lv_page_event.h"
#include "un260/lv_core/page_06_settings.h"
#include "un260/lv_core/page_20_set_print.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/lv_core/page_07_curr.h"
#include "un260/lv_core/page_22_set_double_note.h"
#include "un260/lv_core/page_23_set_flap.h"
#include "un260/lv_core/page_24_set_reject_pocket.h"
#include "un260/lv_core/page_25_set_serial_number.h"
#include "un260/lv_core/page_26_set_aging.h"
#include "un260/lv_core/page_27_set_cfd_level.h"
#include "un260/lv_core/page_30_set_factory.h"
#include "un260/machine_state/machine_state.h"
#include "un260/print/print_config.h"
#include "un260/serial_number/serial_number.h"

#define APP_SETTING_MODE_CLEAR_DELAY_MS 120

static bool g_mode_clear_scheduled;
static uint32_t g_mode_clear_tick;
static bool g_mode_clear_due;

static void app_setting_runtime_schedule_mode_clear(void)
{
    app_setting_runtime_cancel_mode_clear();

    if (counting_data_current()->total_pcs == 0 && counting_data_reject_pcs_count(counting_data_current()) == 0) {
        return;
    }

    g_mode_clear_tick = app_clock_uptime_ms();
    g_mode_clear_scheduled = true;
}

bool app_setting_runtime_take_mode_clear(void)
{
    if (!g_mode_clear_due) {
        return false;
    }

    g_mode_clear_due = false;
    return true;
}

void app_setting_runtime_cancel_mode_clear(void)
{
    g_mode_clear_scheduled = false;
    g_mode_clear_tick = 0;
    g_mode_clear_due = false;
}

static void app_setting_runtime_handle_basic_reply(uint8_t cmd,
                                                   uint8_t *buf,
                                                   uint8_t len)
{
    app_setting_reply_action_t actions =
        app_setting_reply_handle_basic(cmd, buf, len);

    if ((actions & APP_SETTING_REPLY_ACTION_SCHEDULE_MODE_CLEAR) != 0) {
        app_setting_runtime_schedule_mode_clear();
    }
}

bool app_setting_runtime_handle_reply(uint8_t cmd, uint8_t *buf, uint8_t len)
{
    switch (cmd) {
    case 0x04:
    case 0x06:
    case 0x15:
    case 0x16:
    case 0x38:
    case 0x39:
    case 0x3A:
        app_setting_runtime_handle_basic_reply(cmd, buf, len);
        return true;

    case 0x08:
    case 0x31:
    case 0x32:
    case 0x41:
    case 0x42:
    case 0x44:
    case 0x45:
    case 0x46:
        app_setting_reply_handle_detail(cmd, buf, len);
        return true;

    default:
        return false;
    }
}

static void app_setting_runtime_notify_timeout(void)
{
    page_03_update_menu_button_states_refresh();
    show_communication_error_popup();
}

void app_setting_runtime_poll(uint32_t now_ms)
{
    bool notify_timeout = false;
    uint32_t basic_timeouts;
    setting_batch_result_t batch_result;
    setting_value_result_t value_result;
    serial_number_setting_result_t serial_result;
    currency_switch_result_t currency_result;
    print_config_request_result_t print_result;
    setting_action_result_t action_result;

    if (g_mode_clear_scheduled &&
        (uint32_t)(now_ms - g_mode_clear_tick) >=
            APP_SETTING_MODE_CLEAR_DELAY_MS) {
        g_mode_clear_scheduled = false;
        g_mode_clear_tick = 0;
        g_mode_clear_due = true;
    }

    basic_timeouts = setting_service_take_basic_timeouts();
    if (basic_timeouts != SETTING_REQUEST_TIMEOUT_NONE) {
        uart_debug_printf("basic setting request timeout mask=0x%02X\n",
                    (unsigned int)basic_timeouts);
        notify_timeout = true;
    }

    if (setting_service_batch_take_timeout(&batch_result)) {
        if (batch_result.type == SETTING_BATCH_REQUEST_NUMBER) {
            page_03_batch_set_result(false, &batch_result);
        } else if (batch_result.type == SETTING_BATCH_REQUEST_SWITCH) {
            batch_switch_on_0x06_result(false, &batch_result);
        }
        notify_timeout = true;
    }

    if (setting_service_take_double_note_level_timeout(&value_result)) {
        machine_state_confirm_double_note_level(value_result.previous);
        ui_page_22_set_double_note_on_reply(&value_result);
        notify_timeout = true;
    }

    if (setting_service_take_flap_position_timeout(&value_result)) {
        machine_state_confirm_flap_position(value_result.previous);
        ui_page_23_set_flap_on_reply(&value_result);
        notify_timeout = true;
    }

    if (setting_service_take_reject_pocket_max_timeout(&value_result)) {
        machine_state_confirm_reject_pocket_max(value_result.previous);
        ui_page_24_set_reject_pocket_on_reply(&value_result);
        notify_timeout = true;
    }

    if (serial_number_service_take_timeout(&serial_result)) {
        serial_number_state_confirm(serial_result.previous_enabled,
                                    serial_result.previous_level);
        ui_page_25_set_serial_number_on_reply(serial_result.response_level, 0x02);
        notify_timeout = true;
    }

    if (currency_service_take_switch_timeout(&currency_result)) {
        page_07_curr_apply_switch_result(&currency_result);
        uart_debug_printf("currency switch request timeout\n");
        notify_timeout = true;
    }

    if (cfd_service_take_query_timeout()) {
        ui_page_27_set_cfd_level_on_request_failed();
        notify_timeout = true;
    }

    if (cfd_service_take_update_timeout()) {
        ui_page_27_set_cfd_level_on_request_failed();
        notify_timeout = true;
    }

    if (print_config_take_timeout(&print_result)) {
        ui_page_20_set_print_on_reply(&print_result);
        notify_timeout = true;
    }

    if (setting_service_take_aging_timeout(&action_result)) {
        uart_debug_printf("aging start request timeout\n");
        ui_page_26_set_aging_on_reply(0x01);
        notify_timeout = true;
    }

    if (setting_service_take_factory_timeout(&action_result)) {
        uart_debug_printf("factory reset request timeout\n");
        ui_page_30_set_factory_on_reply(0x02);
        notify_timeout = true;
    }

    if (data_collection_request_take_timeout(now_ms)) {
        uart_debug_printf("data collection mode request timeout\n");
        page_06_data_collection_refresh();
        notify_timeout = true;
    }

    if (notify_timeout) {
        app_setting_runtime_notify_timeout();
    }
}

void app_setting_runtime_stop(void)
{
    app_setting_runtime_cancel_mode_clear();
    setting_service_cancel_all();
    serial_number_service_cancel_request();
    currency_service_cancel_switch();
    cfd_service_cancel_query();
    cfd_service_cancel_update();
    print_config_cancel_request();
    data_collection_request_cancel();
}
