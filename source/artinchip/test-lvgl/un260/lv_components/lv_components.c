#include "lv_components.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_03_menu.h"
#include "un260/lv_system/ui_object_utils.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/protocol/protocol_send.h"
#include "un260/app_service/setting_service.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_components/lv_modal_dialog.h"

#include <string.h>

typedef enum {
    SIMPLE_DIALOG_NONE = 0,
    SIMPLE_DIALOG_BOOT_SELFTEST,
    SIMPLE_DIALOG_COMMUNICATION,
    SIMPLE_DIALOG_BATCH_SET,
    SIMPLE_DIALOG_CURRENCY_SET,
    SIMPLE_DIALOG_SYSTEM,
    SIMPLE_DIALOG_COUNTING,
} simple_dialog_kind_t;

static lv_modal_dialog_t g_simple_dialog;
static simple_dialog_kind_t g_simple_dialog_kind;

static bool simple_dialog_matches(simple_dialog_kind_t kind,
                                  const char *title,
                                  const char *body)
{
    const char *current_title;
    const char *current_body;

    if (g_simple_dialog_kind != kind ||
        !lv_modal_dialog_is_visible(&g_simple_dialog) ||
        g_simple_dialog.title == NULL || g_simple_dialog.body == NULL) {
        return false;
    }
    current_title = lv_label_get_text(g_simple_dialog.title);
    current_body = lv_label_get_text(g_simple_dialog.body);
    return current_title != NULL && current_body != NULL &&
           strcmp(current_title, title != NULL ? title : "") == 0 &&
           strcmp(current_body, body != NULL ? body : "") == 0;
}

static void simple_dialog_hide(simple_dialog_kind_t kind)
{
    if (g_simple_dialog_kind != kind) return;
    lv_modal_dialog_hide(&g_simple_dialog);
    g_simple_dialog_kind = SIMPLE_DIALOG_NONE;
}

static void simple_dialog_show(simple_dialog_kind_t kind,
                               const char *title,
                               const char *body,
                               lv_coord_t panel_width,
                               lv_coord_t panel_height,
                               uint32_t accent_color,
                               lv_modal_dialog_action_cb_t action)
{
    lv_modal_dialog_config_t config = {
        .title = title,
        .body = body,
        .primary_text = ui_text_get(UI_TEXT_SETTINGS_DIALOG_CONFIRM),
        .title_font = &lv_font_instrument_sans_semibold_28,
        .body_font = &lv_font_instrument_sans_medium_16,
        .button_font = &lv_font_instrument_sans_semibold_16,
        .panel_width = panel_width,
        .panel_height = panel_height,
        .primary_width = 180,
        .accent_color = accent_color,
        .primary_color = 0x1462CC,
        .secondary_color = 0x72808B,
        .primary_action = action,
        .center_single_button = false,
    };

    g_simple_dialog_kind = kind;
    lv_modal_dialog_show(&g_simple_dialog, lv_scr_act(), &config);
}

void hide_boot_selftest_error_popup(void)
{
    simple_dialog_hide(SIMPLE_DIALOG_BOOT_SELFTEST);
}

static void boot_selftest_error_confirm_action(void *user_data)
{
    (void)user_data;
    hide_boot_selftest_error_popup();
    ui_manager_switch(UI_PAGE_SENSOR);
}

void show_boot_selftest_error_popup(const char* msg)
{
    if (simple_dialog_matches(SIMPLE_DIALOG_BOOT_SELFTEST,
                              "SELF-TEST ERROR", msg)) return;
    simple_dialog_show(SIMPLE_DIALOG_BOOT_SELFTEST,
                       "SELF-TEST ERROR", msg, 700, 260,
                       0xE45454, boot_selftest_error_confirm_action);
}

void hide_batch_set_fail_popup(void)
{
    simple_dialog_hide(SIMPLE_DIALOG_BATCH_SET);
}

void hide_currency_set_fail_popup(void)
{
    simple_dialog_hide(SIMPLE_DIALOG_CURRENCY_SET);
}

void hide_communication_error_popup(void)
{
    simple_dialog_hide(SIMPLE_DIALOG_COMMUNICATION);
}

static void currency_set_fail_confirm_action(void *user_data)
{
    (void)user_data;
    hide_currency_set_fail_popup();
    ui_manager_switch(UI_PAGE_MAIN);
}

static void batch_set_fail_confirm_action(void *user_data)
{
    (void)user_data;
    hide_batch_set_fail_popup();
}

static void communication_error_confirm_action(void *user_data)
{
    (void)user_data;
    hide_communication_error_popup();
}

void show_communication_error_popup(void)
{
    if (simple_dialog_matches(SIMPLE_DIALOG_COMMUNICATION,
        "COMMUNICATION ERROR",
        "Communication may be abnormal. Please check the UI and controller connection.")) return;
    simple_dialog_show(SIMPLE_DIALOG_COMMUNICATION,
        "COMMUNICATION ERROR",
        "Communication may be abnormal. Please check the UI and controller connection.",
        740, 260, 0xE45454, communication_error_confirm_action);
}

void show_batch_set_fail_popup(void)
{
    if (simple_dialog_matches(SIMPLE_DIALOG_BATCH_SET,
        "BATCH SET FAILED",
        "Please remove banknotes from the feeder or reject pocket first.")) return;
    simple_dialog_show(SIMPLE_DIALOG_BATCH_SET,
        "BATCH SET FAILED",
        "Please remove banknotes from the feeder or reject pocket first.",
        700, 260, 0xE45454, batch_set_fail_confirm_action);
}

void show_currency_set_fail_popup(void)
{
    if (simple_dialog_matches(SIMPLE_DIALOG_CURRENCY_SET,
        "CURRENCY SET FAILED",
        "There are banknotes still inside the machine or the sensor is abnormal.\n"
        "Currency change was rejected.")) return;
    simple_dialog_show(SIMPLE_DIALOG_CURRENCY_SET,
        "CURRENCY SET FAILED",
        "There are banknotes still inside the machine or the sensor is abnormal.\n"
        "Currency change was rejected.",
        760, 280, 0xE45454, currency_set_fail_confirm_action);
}

const char* get_system_error_desc(uint8_t code)
{
    const char *known = machine_runtime_error_desc(code);
    if (known) return known;
    static char description[40];
    snprintf(description, sizeof(description), "Controller fault 0x%02X", code);
    return description;
}

static uint8_t g_sys_err_last_code = 0x00;

void system_error_state_reset(void)
{
    g_sys_err_last_code = 0x00;
}

void hide_system_error_popup(void)
{
    simple_dialog_hide(SIMPLE_DIALOG_SYSTEM);
}

static void system_error_confirm_action(void *user_data)
{
    (void)user_data;
    uint8_t clear_cmd = 0x01;
    protocol_send(0x3D, &clear_cmd, 1); /* FD DF 06 3D 01 0A */
    hide_system_error_popup();
    system_error_state_reset();
}

void system_error_confirm_cb(lv_event_t* e)
{
    if ((lv_event_code_t)lv_event_get_code(e) == LV_EVENT_CLICKED) {
        system_error_confirm_action(NULL);
    }
}

void show_system_error_popup(uint8_t code)
{
    if (code == 0x00) return;
    if (g_simple_dialog_kind == SIMPLE_DIALOG_SYSTEM &&
        g_sys_err_last_code == code &&
        lv_modal_dialog_is_visible(&g_simple_dialog)) return;
    simple_dialog_show(SIMPLE_DIALOG_SYSTEM, "SYSTEM ERROR",
                       get_system_error_desc(code), 620, 250,
                       0xE45454, system_error_confirm_action);
    g_sys_err_last_code = code;
}

/* 启动点钞(0x0A) type=0x01: 异常原因表 */
static const char* g_counting_start_error_desc[0x100] = {
    [0x02] = "Please Place Banknotes in the Hopper",
};

/* 启动点钞(0x0A) type=0x02: 设备故障码 */
static const char* g_counting_fault_error_desc[0x100] = {
    [0x01] = "Upper Channel Sensor Blocked",
    [0x02] = "Lower Channel Sensor Blocked",
    [0x03] = "Reject Exit Sensor Blocked",
    [0x04] = "Reject Pocket Sensor Blocked",
    [0x05] = "Reject Pocket Full",
    [0x06] = "Stacker Pocket Sensor Blocked",
    [0x07] = "Stacker Pocket Full",
    [0x08] = "Stacker & Reject Pockets Full",
    [0x09] = "Upper & Lower Channels Open",
    [0x0A] = "Genuine Exit Sensor Blocked",
    [0x0B] = "Dust Cover / Baffle Not Closed",
    [0x0C] = "Flipper Position Fault",
    [0x0D] = "Encoder Fault",
};

const char* get_counting_error_desc(uint8_t type, uint8_t code)
{
    if (type == 0x01) {
        if (g_counting_start_error_desc[code] != NULL) {
            return g_counting_start_error_desc[code];
        }
        return "Start Counting Failed";
    }

    if (type == 0x02) {
        if (g_counting_fault_error_desc[code] != NULL) {
            return g_counting_fault_error_desc[code];
        }
        return "Counting Fault";
    }

    return "Unknown Counting Fault";
}

static const char* get_counting_ui_error_desc(uint8_t type, uint8_t code)
{
    const char *description;

    if (type == 0x01 && (code == 0x00 || code == 0x02)) {
        return ui_text_get(UI_TEXT_WIDGET_FAULT_NO_NOTE_MAIN);
    }

    description = machine_start_error_desc(code);
    if (description != NULL) {
        return description;
    }

    return ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_COUNT_ERROR);
}

static uint8_t g_count_err_last_code = 0x00;
static uint8_t g_count_err_last_type = 0x00;

void hide_counting_error_popup(void)
{
    simple_dialog_hide(SIMPLE_DIALOG_COUNTING);
}

static void counting_error_confirm_action(void *user_data)
{
    (void)user_data;
    /* 与系统报错确认一致：发送清除命令 */
    uint8_t clear_cmd = 0x01;
    protocol_send(0x3D, &clear_cmd, 1); /* FD DF 06 3D 01 0A */
    hide_counting_error_popup();
    g_count_err_last_code = 0x00;
    g_count_err_last_type = 0x00;
}

void counting_error_confirm_cb(lv_event_t* e)
{
    if ((lv_event_code_t)lv_event_get_code(e) == LV_EVENT_CLICKED) {
        counting_error_confirm_action(NULL);
    }
}

void show_counting_error_popup(uint8_t type, uint8_t code)
{
    if (code == 0x00) return;
    if (g_simple_dialog_kind == SIMPLE_DIALOG_COUNTING &&
        g_count_err_last_type == type && g_count_err_last_code == code &&
        lv_modal_dialog_is_visible(&g_simple_dialog)) return;
    simple_dialog_show(SIMPLE_DIALOG_COUNTING, "COUNTING ERROR",
                       get_counting_ui_error_desc(type, code), 620, 250,
                       0xE45454, counting_error_confirm_action);
    g_count_err_last_type = type;
    g_count_err_last_code = code;
}
