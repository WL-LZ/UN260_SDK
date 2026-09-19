#include "app_setting_reply.h"

#include <stdbool.h>

#include "un260/app_service/setting_service.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/lv_components/lv_components.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_core/lv_page_event.h"
#include "un260/lv_core/page_03_menu.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/machine_state/machine_state.h"
#include "un260/protocol/mode_codec.h"
#include "un260/lv_system/user_cfg.h"

app_setting_reply_action_t app_setting_reply_handle_basic(uint8_t cmd,
                                                          const uint8_t *buf,
                                                          uint8_t len)
{
    app_setting_reply_action_t actions = APP_SETTING_REPLY_ACTION_NONE;

    switch (cmd) {
    case 0x04:
    {
        if (len < 6) break;

        uint8_t status = buf[4];

        if (status == 0x01)
        {
            uint8_t requested_mode;
            if (!setting_service_take_mode_result(&requested_mode)) {
                uart_debug_printf("Set work mode success ignored: no pending request\n");
                break;
            }
            machine_state_confirm_mode(requested_mode);
            page_01_mode_switch_refre();
            page_01_bottom_a_refresh_mode(true);
            page_01_curr_img_refre();
            ui_manager_publish_data_changed(UI_DATA_TOPIC_MACHINE_SETTINGS);
            actions = (app_setting_reply_action_t)(actions |
                      APP_SETTING_REPLY_ACTION_SCHEDULE_MODE_CLEAR);
            uart_debug_printf("Set work mode success\n");
            smart_island_refresh_summary();
        }
        else if (status == 0x02)
        {
            uint8_t requested_mode;
            if (!setting_service_take_mode_result(&requested_mode)) {
                uart_debug_printf("Set work mode fail ignored: no pending request\n");
                break;
            }
            uart_debug_printf("Set work mode fail\n");
            show_start_fault_popup(0x02, 0x06);
        }
        else if (status == 0x03)
        {
            if (len < 7) break;
            uint8_t protocol_mode = buf[5];
            uint8_t machine_mode = MODE_NONE;

            if (!mode_codec_decode(protocol_mode, &machine_mode)) {
                uart_debug_printf("Boot work mode invalid: 0x%02X\n", protocol_mode);
                break;
            }
            machine_state_confirm_mode(machine_mode);
            setting_service_cancel_mode_request();
            page_01_mode_switch_refre();
            page_01_curr_img_refre();
            uart_debug_printf("Boot work mode: 0x%02X\n", protocol_mode);
            ui_manager_publish_data_changed(UI_DATA_TOPIC_MACHINE_SETTINGS);
            smart_island_refresh_summary();
        }
        break;
    }

    case 0x06:
    {
        if (len < 6) break;

        uint8_t status = buf[4];

        if (status == 0x01)
        {
            setting_batch_result_t result;
            if (!setting_service_batch_take_result(status, &result)) {
                uart_debug_printf("Set batch num success ignored: no pending request\n");
                break;
            }
            if (result.type == SETTING_BATCH_REQUEST_NUMBER) {
                page_03_batch_set_result(true, &result);
            } else if (result.type == SETTING_BATCH_REQUEST_SWITCH) {
                batch_switch_on_0x06_result(true, &result);
            }
            uart_debug_printf("Set batch num success\n");
            smart_island_refresh_summary();
        }
        else if (status == 0x02)
        {
            setting_batch_result_t result;
            if (!setting_service_batch_take_result(status, &result)) {
                uart_debug_printf("Set batch num fail ignored: no pending request\n");
                break;
            }
            if (result.type == SETTING_BATCH_REQUEST_NUMBER) {
                page_03_batch_set_result(false, &result);
            } else if (result.type == SETTING_BATCH_REQUEST_SWITCH) {
                batch_switch_on_0x06_result(false, &result);
            }
            show_batch_set_fail_popup();
            uart_debug_printf("Set batch num fail\n");
        }
        else if (status == 0x03)
        {
            if (len < 7) break;
            machine_state_confirm_batch(buf[5] != 200, buf[5]);
            if (page_03_menu_is_visible()) {
                set_batch_switch_state(machine_state_batch_enabled());
                page_03_menu_refresh_batch_number();
            }
            page_01_batch_refre();
            uart_debug_printf("Boot batch num: %d\n", machine_state_batch_num());
            smart_island_refresh_summary();
        }
        break;
    }

    case 0x39:
    {
        if (len < 6) break;
        uint8_t sub = buf[4];

        if (sub == 0x00) {
            bool target;
            if (!setting_service_take_add_result(&target)) {
                uart_debug_printf("ADD set success ignored: no pending request\n");
                break;
            }
            machine_state_confirm_add(target);
            page_01_bottom_a_refresh_add(true);
            page_03_update_menu_button_states_refresh();
            uart_debug_printf("ADD set success\n");
            smart_island_refresh_summary();
        } else if (sub == 0x01) {
            if (!setting_service_take_add_result(NULL)) {
                uart_debug_printf("ADD set failed ignored: no pending request\n");
                break;
            }
            uart_debug_printf("ADD set failed\n");
            show_start_fault_popup(0x02, 0x06);
            page_03_update_menu_button_states_refresh();
        } else if (sub == 0x02) {
            uint8_t v = buf[5];
            if (v == 0x00) {
                machine_state_confirm_add(false);
            } else if (v == 0x01) {
                machine_state_confirm_add(true);
            } else {
                uart_debug_printf("ADD boot status: unexpected raw=0x%02X, keep %s\n",
                            v, machine_state_add_enabled() ? "ON" : "OFF");
            }
            (void)setting_service_take_add_result(NULL);
            page_01_bottom_a_refresh_add(false);
            uart_debug_printf("ADD boot status: raw=0x%02X -> %s\n",
                        v, machine_state_add_enabled() ? "ON" : "OFF");
            smart_island_refresh_summary();
            page_03_update_menu_button_states_refresh();
        }
        break;
    }

    case 0x15:
    {
        if (len < 6) break;
        uint8_t sub = buf[4];

        if (sub == 0x01) {
            bool target;
            if (!setting_service_take_beep_result(&target)) {
                uart_debug_printf("BEEP set success ignored: no pending request\n");
                break;
            }
            machine_state_confirm_buzzer(target);
            uart_debug_printf("BEEP set success\n");
            page_03_update_menu_button_states_refresh();
        } else if (sub == 0x02) {
            if (!setting_service_take_beep_result(NULL)) {
                uart_debug_printf("BEEP set failed ignored: no pending request\n");
                break;
            }
            uart_debug_printf("BEEP set failed\n");
            show_start_fault_popup(0x02, 0x06);
            page_03_update_menu_button_states_refresh();
        } else if (sub == 0x03) {
            if (len < 7) break;
            uint8_t v = buf[5];
            machine_state_confirm_buzzer(v == 0x01);
            (void)setting_service_take_beep_result(NULL);
            uart_debug_printf("BEEP boot status: %s\n", machine_state_buzzer_enabled() ? "ON" : "OFF");
            page_03_update_menu_button_states_refresh();
        }
        break;
    }

    case 0x16:
    {
        if (len < 6) break;
        uint8_t type = buf[4];
        uint8_t res  = buf[5];

        if (type >= 0x01 && type <= 0x03) {
            if (res == 0x01) {
                uint8_t target_speed;
                if (!setting_service_take_speed_result(&target_speed)) {
                    uart_debug_printf("SPEED set SUCCESS ignored: no pending request\n");
                    break;
                }
                machine_state_confirm_speed(target_speed);
                page_03_update_menu_button_states_refresh();
                page_01_bottom_c_refresh_speed(true);
                page_01_speed_refre();
                uart_debug_printf("SPEED set SUCCESS: type=0x%02X -> ui=%u\n",
                            type, machine_state_speed());
                smart_island_refresh_summary();
            } else if (res == 0x02) {
                if (!setting_service_take_speed_result(NULL)) {
                    uart_debug_printf("SPEED set FAIL ignored: no pending request\n");
                    break;
                }
                page_03_update_menu_button_states_refresh();
                uart_debug_printf("SPEED set FAIL: type=0x%02X\n", type);
                show_start_fault_popup(0x02, 0x06);
            } else {
                uart_debug_printf("SPEED set UNKNOWN result: type=0x%02X, res=0x%02X\n", type, res);
            }
        } else if (type == 0x04) {
            if (res <= 0x02) {
                machine_state_confirm_speed((uint8_t)(0x02 - res));
                page_03_update_menu_button_states_refresh();
                uart_debug_printf("SPEED boot sync: mode=0x%02X -> ui=%u\n", res, machine_state_speed());
                smart_island_refresh_summary();
            } else {
                uart_debug_printf("SPEED boot sync: invalid mode=0x%02X\n", res);
            }
        } else {
            uart_debug_printf("0x16: unknown type=0x%02X, res=0x%02X\n", type, res);
        }
        break;
    }

    case 0x3A:
    {
        if (len < 6) {
            uart_debug_printf("0x3A: frame too short (%d)\n", len);
            break;
        }
        uint8_t type = buf[4];
        uint8_t val  = buf[5];
        if (type == 0x05) {
            if (val <= 0x03) {
                machine_state_confirm_fo_mode(val);
                uart_debug_printf("FO boot sync: mode=0x%02X -> ui=%u\n", val, machine_state_fo_mode());
            } else {
                uart_debug_printf("FO boot sync: invalid mode=0x%02X\n", val);
            }
            (void)setting_service_take_fo_mode_result(NULL);
            page_01_bottom_a_refresh_fo(false);
            smart_island_refresh_summary();
            break;
        }
        if (type <= 0x03) {
            if (val == 0x01) {
                uint8_t target_mode = 0;
                if (!setting_service_take_fo_mode_result(&target_mode)) {
                    uart_debug_printf("FO set SUCCESS ignored: no pending request\n");
                    break;
                }
                machine_state_confirm_fo_mode(target_mode);
                page_01_bottom_a_refresh_fo(true);
                page_03_update_menu_button_states_refresh();
                uart_debug_printf("FO set SUCCESS: type=0x%02X -> ui=%u\n", type, machine_state_fo_mode());
                smart_island_refresh_summary();
            } else if (val == 0x02) {
                if (!setting_service_take_fo_mode_result(NULL)) {
                    uart_debug_printf("FO set FAIL ignored: no pending request\n");
                    break;
                }
                uart_debug_printf("FO set FAIL: type=0x%02X\n", type);
                show_start_fault_popup(0x02, 0x06);
                page_03_update_menu_button_states_refresh();
            } else {
                uart_debug_printf("FO set UNKNOWN result: type=0x%02X, res=0x%02X\n", type, val);
            }
        } else {
            uart_debug_printf("0x3A: unknown type=0x%02X, val=0x%02X\n", type, val);
        }
        break;
    }

    case 0x38:
        if (work_mode_service_handle_reply(buf, len)) {
            page_01_bottom_a_refresh_work(false);
            page_03_update_menu_button_states_refresh();
            smart_island_refresh_summary();
        }
        break;

    default:
        break;
    }

    return actions;
}
