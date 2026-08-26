#include "app_setting_reply.h"

#include "un260/app_service/setting_service.h"
#include "un260/lv_core/page_20_set_print.h"
#include "un260/lv_core/page_22_set_double_note.h"
#include "un260/lv_core/page_23_set_flap.h"
#include "un260/lv_core/page_24_set_reject_pocket.h"
#include "un260/lv_core/page_25_set_serial_number.h"
#include "un260/lv_core/page_26_set_aging.h"
#include "un260/lv_core/page_27_set_cfd_level.h"
#include "un260/lv_core/page_30_set_factory.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/machine_state/machine_state.h"
#include "un260/print/print_config.h"
#include "un260/serial_number/serial_number.h"
#include "un260/lv_system/user_cfg.h"


static void setting_reply_handle_double_note(const uint8_t *buf, uint8_t len)
{
    setting_value_result_t result;
    bool result_taken;
    uint8_t normalized_level;

    if (len < 6) {
        uart_debug_printf("0x31 invalid len=%d\n", len);
        return;
    }

    if (len == 6) {
        normalized_level = buf[4];
        if (normalized_level < DOUBLE_NOTE_LEVEL_MIN || normalized_level > DOUBLE_NOTE_LEVEL_MAX) {
            normalized_level = DOUBLE_NOTE_LEVEL_MIN;
        }
        machine_state_confirm_double_note_level(normalized_level);
        ui_page_22_set_double_note_on_boot_setting();
        uart_debug_printf("0x31 boot double note level: level=0x%02X\n", buf[4]);
        return;
    }

    if (buf[4] == 0x31) {
        normalized_level = buf[5];
        if (normalized_level < DOUBLE_NOTE_LEVEL_MIN || normalized_level > DOUBLE_NOTE_LEVEL_MAX) {
            normalized_level = DOUBLE_NOTE_LEVEL_MIN;
        }
        machine_state_confirm_double_note_level(normalized_level);
        ui_page_22_set_double_note_on_boot_setting();
        uart_debug_printf("0x31 boot double note level: level=0x%02X\n", buf[5]);
        return;
    }

    normalized_level = buf[4];
    if (normalized_level < DOUBLE_NOTE_LEVEL_MIN || normalized_level > DOUBLE_NOTE_LEVEL_MAX) {
        normalized_level = DOUBLE_NOTE_LEVEL_MIN;
    }

    result_taken = setting_service_take_double_note_level_result(normalized_level,
                                                                 buf[5],
                                                                 &result);
    if (result_taken && result.success) {
        machine_state_confirm_double_note_level(normalized_level);
    } else if (result_taken && result.target == normalized_level) {
        machine_state_confirm_double_note_level(result.previous);
    }

    uart_debug_printf("0x31 double note level ack: level=0x%02X res=0x%02X\n", buf[4], buf[5]);
    if (result_taken) ui_page_22_set_double_note_on_reply(&result);
}

static void setting_reply_handle_serial_number(const uint8_t *buf, uint8_t len)
{
    serial_number_setting_result_t result;
    bool result_taken;
    uint8_t normalized_level;

    if (len < 6) {
        uart_debug_printf("0x32 invalid len=%d\n", len);
        return;
    }

    if (len == 6) {
        normalized_level = serial_number_state_normalize_level(buf[4]);
        serial_number_service_cancel_request();
        serial_number_state_confirm(normalized_level != SERIAL_NUMBER_LEVEL_OFF, normalized_level);
        ui_page_25_set_serial_number_on_boot_setting(buf[4]);
        uart_debug_printf("0x32 boot serial number level: level=0x%02X\n", buf[4]);
        return;
    }

    normalized_level = serial_number_state_normalize_level(buf[4]);
    result_taken = serial_number_service_take_reply(normalized_level, buf[5], &result);
    if (!result_taken) {
        uart_debug_printf("0x32 serial number level ack ignored: no matching request\n");
        return;
    }
    if (result.success) {
        serial_number_state_confirm(normalized_level != SERIAL_NUMBER_LEVEL_OFF, normalized_level);
    } else {
        serial_number_state_confirm(result.previous_enabled, result.previous_level);
    }

    uart_debug_printf("0x32 serial number level ack: level=0x%02X res=0x%02X\n", buf[4], buf[5]);
    ui_page_25_set_serial_number_on_reply(buf[4], buf[5]);
}

static void setting_reply_handle_flap(const uint8_t *buf, uint8_t len)
{
    setting_value_result_t result;

    if (len < 6) {
        uart_debug_printf("0x42 invalid len=%d\n", len);
        return;
    }

    uart_debug_printf("0x42 flap setting ack: res=0x%02X\n", buf[4]);
    if (setting_service_take_flap_position_result(buf[4], &result)) {
        machine_state_confirm_flap_position(result.success ? result.target : result.previous);
        ui_page_23_set_flap_on_reply(&result);
    }
}

static void setting_reply_handle_reject_pocket(const uint8_t *buf, uint8_t len)
{
    setting_value_result_t result;
    uint8_t type;
    uint8_t res;

    if (len < 7) return;
    type = buf[4];
    res = buf[5];

    if (type != 0x01) {
        uart_debug_printf("0x08 unknown type=0x%02X, res=0x%02X\n", type, res);
        return;
    }

    if (len >= 8) {
        uint8_t capacity = buf[6];
        if (capacity < REJECT_POCKET_MIN_CAPACITY) capacity = REJECT_POCKET_MIN_CAPACITY;
        if (capacity > REJECT_POCKET_MAX_CAPACITY) capacity = REJECT_POCKET_MAX_CAPACITY;
        machine_state_confirm_reject_pocket_max(capacity);
        ui_page_24_set_reject_pocket_on_boot_setting();
        uart_debug_printf("Boot reject pocket pcs: %u\n", machine_state_reject_pocket_max());
        return;
    }

    if (res == 0x01) {
        uart_debug_printf("Reject pocket pcs set success\n");
        if (setting_service_take_reject_pocket_max_result(res, &result)) {
            machine_state_confirm_reject_pocket_max(result.target);
            ui_page_24_set_reject_pocket_on_reply(&result);
        }
    } else if (res == 0x02) {
        uart_debug_printf("Reject pocket pcs set fail\n");
        if (setting_service_take_reject_pocket_max_result(res, &result)) {
            machine_state_confirm_reject_pocket_max(result.previous);
            ui_page_24_set_reject_pocket_on_reply(&result);
        }
    } else {
        uart_debug_printf("0x08 unknown res=0x%02X\n", res);
    }
}

static void setting_reply_handle_print(const uint8_t *buf, uint8_t len)
{
    print_config_request_result_t result;

    if (len < 7) {
        uart_debug_printf("0x41 invalid len=%d\n", len);
        return;
    }

    if ((buf[4] == 0x01 && buf[5] >= PRINT_SETTING_CONTENT_LIST &&
         buf[5] <= PRINT_SETTING_CONTENT_LIST_SN) ||
        (buf[4] == 0x02 && len >= 27 && (buf[5] == 0x01 || buf[5] == 0x02)) ||
        (buf[4] == 0x03 && len >= 8 && (buf[5] == 0x01 || buf[5] == 0x02))) {
        ui_page_20_set_print_on_boot_setting(&buf[4], (uint16_t)(len - 5));
        uart_debug_printf("0x41 boot print setting: sub=0x%02X\n", buf[4]);
        return;
    }

    uart_debug_printf("0x41 print setting ack: sub=0x%02X res=0x%02X\n", buf[4], buf[5]);
    if (!print_config_take_reply(buf[4], buf[5], &result)) {
        uart_debug_printf("0x41 print setting ack ignored: no matching request\n");
        return;
    }
    ui_page_20_set_print_on_reply(&result);
}

bool app_setting_reply_handle_detail(uint8_t cmd,
                                     const uint8_t *buf,
                                     uint8_t len)
{
    if (!buf) return false;

    switch (cmd) {
    case 0x08:
        setting_reply_handle_reject_pocket(buf, len);
        return true;
    case 0x31:
        setting_reply_handle_double_note(buf, len);
        return true;
    case 0x32:
        setting_reply_handle_serial_number(buf, len);
        return true;
    case 0x41:
        setting_reply_handle_print(buf, len);
        return true;
    case 0x42:
        setting_reply_handle_flap(buf, len);
        return true;
    case 0x44:
        if (len < 6) uart_debug_printf("0x44 invalid len=%d\n", len);
        else {
            setting_action_result_t result;

            uart_debug_printf("0x44 factory setting reply: res=0x%02X\n", buf[4]);
            if (setting_service_take_factory_result(buf[4], &result)) {
                ui_page_30_set_factory_on_reply(result.success ? 0x01 : 0x02);
            } else {
                uart_debug_printf("0x44 factory setting reply ignored: no matching request\n");
            }
        }
        return true;
    case 0x45:
        if (len < 21) uart_debug_printf("0x45 invalid len=%d\n", len);
        else {
            uart_debug_printf("0x45 cfd level info: currency=%c%c%c\n", buf[4], buf[5], buf[6]);
            ui_page_27_set_cfd_level_on_info(&buf[4], (uint16_t)(len - 4));
        }
        return true;
    case 0x46:
        if (len < 6) uart_debug_printf("0x46 invalid len=%d\n", len);
        else {
            uart_debug_printf("0x46 aging setting reply: sx=0x%02X\n", buf[4]);
            if (buf[4] == 0x02) {
                ui_page_26_set_aging_on_reply(buf[4]);
            } else {
                setting_action_result_t result;

                if (setting_service_take_aging_result(buf[4], &result)) {
                    ui_page_26_set_aging_on_reply(result.success ? 0x00 : 0x01);
                } else {
                    uart_debug_printf("0x46 aging setting reply ignored: no matching request\n");
                }
            }
        }
        return true;
    default:
        return false;
    }
}
