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

typedef struct {
    lv_obj_t* switch_container;
    lv_obj_t* switch_knob;
    lv_obj_t* label_on;
    lv_obj_t* label_off;
} batch_switch_t;

static batch_switch_t batch_switch = {
    .switch_container = NULL,
    .switch_knob = NULL,
    .label_on = NULL,
    .label_off = NULL,
};
static uint8_t g_batch_last_on_num = 100;

void set_batch_switch_state(bool enable);

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

static void batch_switch_center_knob_y(void)
{
    if (!batch_switch.switch_container || !batch_switch.switch_knob) return;
    lv_obj_update_layout(batch_switch.switch_container);
    lv_obj_update_layout(batch_switch.switch_knob);
    lv_obj_set_y(batch_switch.switch_knob,
        (lv_obj_get_height(batch_switch.switch_container) - lv_obj_get_height(batch_switch.switch_knob)) / 2);
}

static void update_switch_visual(bool enable, bool animate) {
    lv_coord_t cont_w = lv_obj_get_width(batch_switch.switch_container);
    lv_coord_t knob_w = lv_obj_get_width(batch_switch.switch_knob);
    batch_switch_center_knob_y();

    if (enable) {
        //  ON 标签
        lv_obj_set_style_bg_color(batch_switch.switch_container, lv_color_hex(0x0A66F6), 0);

        lv_label_set_text(batch_switch.label_on, "ON");
        lv_obj_set_style_text_color(batch_switch.label_on, lv_color_white(), 0);
        lv_obj_align(batch_switch.label_on, LV_ALIGN_LEFT_MID, 9, 0);
        lv_obj_clear_flag(batch_switch.label_on, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(batch_switch.label_off, LV_OBJ_FLAG_HIDDEN);

        // 滑块动画
        lv_coord_t target_x = cont_w - knob_w - 4;
        if (animate && lv_obj_get_x(batch_switch.switch_knob) != target_x) {
            // 执行动画
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, batch_switch.switch_knob);
            lv_anim_set_values(&a, lv_obj_get_x(batch_switch.switch_knob), target_x);
            lv_anim_set_time(&a, 250);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
            lv_anim_start(&a);
        }
        else {
            lv_obj_set_x(batch_switch.switch_knob, target_x);
        }

    }
    else {
        // OFF 标签
        lv_obj_set_style_bg_color(batch_switch.switch_container, lv_color_hex(0xE9EEF5), 0);

        lv_label_set_text(batch_switch.label_off, "OFF");
        lv_obj_set_style_text_color(batch_switch.label_off, lv_color_hex(0x697589), 0);
        lv_obj_align(batch_switch.label_off, LV_ALIGN_RIGHT_MID, -9, 0);
        lv_obj_clear_flag(batch_switch.label_off, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(batch_switch.label_on, LV_OBJ_FLAG_HIDDEN);

        lv_coord_t target_x = 4;
        if (animate && lv_obj_get_x(batch_switch.switch_knob) != target_x) {
            // 执行动画
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, batch_switch.switch_knob);
            lv_anim_set_values(&a, lv_obj_get_x(batch_switch.switch_knob), target_x);
            lv_anim_set_time(&a, 250);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
            lv_anim_start(&a);
        }
        else {
            // 直接设置位置，无动画
            lv_obj_set_x(batch_switch.switch_knob, target_x);
        }
    }
    page_03_menu_refresh_batch_mode();
#if LV_DEBUG
    printf("batch_switch_status: %s\n", machine_state_batch_enabled() ? "ON" : "OFF");
#endif // LV_DEBUG

}

// 点击切换状态
static void switch_event_cb(lv_event_t* e) {
    LV_UNUSED(e);
    bool previous_enable = machine_state_batch_enabled();
    bool target_enable = !previous_enable;

    /* batch 开关行为：
     * ON  -> 发送用户预设的 pcs batch
     * OFF -> 固定发送 200
     */
    uint8_t batch_cmd = 200;
    if (target_enable) {
        int preset = machine_state_batch_num();
        if (preset <= 0 || preset >= 200) {
            preset = g_batch_last_on_num;
        }
        if (preset < 5) preset = 5;
        if (preset > 199) preset = 199;
        batch_cmd = (uint8_t)preset;
    }
    if (!setting_service_request_batch_switch(target_enable, batch_cmd, previous_enable, machine_state_batch_num())) {
        update_switch_visual(machine_state_batch_enabled(), false);
    }
}

void batch_switch_on_0x06_result(bool success, const setting_batch_result_t *result)
{
    if (result == NULL) return;

    if (success) {
        machine_state_confirm_batch(result->target.enable, result->target.num);
        if (machine_state_batch_enabled()) {
            if (machine_state_batch_num() >= 5 && machine_state_batch_num() <= 199) {
                g_batch_last_on_num = machine_state_batch_num();
            }
        }
        update_switch_visual(machine_state_batch_enabled(), true);
        page_03_menu_refresh_batch_number();
        page_01_batch_refre();
        return;
    }

    machine_state_confirm_batch_enable(result->previous.enable);
}

// 创建批次开关组件
void create_batch_num_switch(lv_obj_t* parent) {
    // 创建容器
    batch_switch.switch_container = lv_obj_create(parent);
    lv_obj_set_size(batch_switch.switch_container, 78, 36);
    lv_obj_set_style_radius(batch_switch.switch_container, 18, 0);
    lv_obj_set_style_pad_all(batch_switch.switch_container, 0, 0);
    lv_obj_set_style_bg_color(batch_switch.switch_container,
        machine_state_batch_enabled() ?
        lv_color_hex(0x0A66F6) :
        lv_color_hex(0xE9EEF5), 0);
    lv_obj_set_style_bg_opa(batch_switch.switch_container, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(batch_switch.switch_container, 1, 0);
    lv_obj_set_style_border_color(batch_switch.switch_container, lv_color_hex(0xDDE5EF), 0);
    lv_obj_set_style_shadow_color(batch_switch.switch_container, lv_color_hex(0x8795A8), 0);
    lv_obj_set_style_shadow_opa(batch_switch.switch_container, LV_OPA_10, 0);
    lv_obj_set_style_shadow_width(batch_switch.switch_container, 8, 0);
    lv_obj_set_style_shadow_ofs_y(batch_switch.switch_container, 2, 0);
    lv_obj_set_style_bg_color(batch_switch.switch_container, lv_color_hex(0xDDE8F8), LV_STATE_PRESSED);

    lv_obj_add_flag(batch_switch.switch_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(batch_switch.switch_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(batch_switch.switch_container, switch_event_cb, LV_EVENT_CLICKED, NULL);

    // 创建 OFF 标签
    batch_switch.label_off = lv_label_create(batch_switch.switch_container);
    lv_label_set_text(batch_switch.label_off, "OFF");
    lv_obj_set_style_text_font(batch_switch.label_off, &lv_font_instrument_sans_semibold_12, 0);
    lv_obj_set_style_text_color(batch_switch.label_off, lv_color_hex(0x697589), 0);

    // 创建 ON 标签
    batch_switch.label_on = lv_label_create(batch_switch.switch_container);
    lv_label_set_text(batch_switch.label_on, "ON");
    lv_obj_set_style_text_font(batch_switch.label_on, &lv_font_instrument_sans_semibold_12, 0);
    lv_obj_set_style_text_color(batch_switch.label_on, lv_color_white(), 0);

    // 创建滑块
    batch_switch.switch_knob = lv_obj_create(batch_switch.switch_container);
    lv_obj_set_size(batch_switch.switch_knob, 28, 28);
    lv_obj_set_style_radius(batch_switch.switch_knob, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(batch_switch.switch_knob, lv_color_white(), 0);
    lv_obj_set_style_border_width(batch_switch.switch_knob, 1, 0);
    lv_obj_set_style_border_color(batch_switch.switch_knob, lv_color_hex(0xEFF3F8), 0);
    lv_obj_set_style_shadow_width(batch_switch.switch_knob, 6, 0);
    lv_obj_set_style_shadow_color(batch_switch.switch_knob, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(batch_switch.switch_knob, LV_OPA_20, 0);
    lv_obj_clear_flag(batch_switch.switch_knob, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(batch_switch.switch_knob, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_update_layout(batch_switch.switch_container);
    lv_obj_update_layout(batch_switch.switch_knob);

    // 居中Y
    batch_switch_center_knob_y();

    // 设置初始状态，无动画
    update_switch_visual(machine_state_batch_enabled(), false);
}

// 外部调用：获取容器
lv_obj_t* get_batch_switch_container(void) {
    return batch_switch.switch_container;
}

//设置开关状态（自动更新UI，无动画）
void set_batch_switch_state(bool enable) {
    if (machine_state_batch_num() >= 5 && machine_state_batch_num() <= 199) {
        g_batch_last_on_num = machine_state_batch_num();
    }

    if (batch_switch.switch_container == NULL ||
        batch_switch.switch_knob == NULL ||
        batch_switch.label_on == NULL ||
        batch_switch.label_off == NULL) {
        return;
    }

    if (!lv_obj_is_valid(batch_switch.switch_container) ||
        !lv_obj_is_valid(batch_switch.switch_knob) ||
        !lv_obj_is_valid(batch_switch.label_on) ||
        !lv_obj_is_valid(batch_switch.label_off)) {
        return;
    }

    // 外部调用时不执行动画
    update_switch_visual(enable, false);
}

// 记录最近一次可恢复的 batch 数值
void batch_switch_set_last_on_num(uint8_t num)
{
    if (num >= 5 && num <= 199) {
        g_batch_last_on_num = num;
    }
}


const char* get_system_error_desc(uint8_t code)
{
    switch (code) {
    case 0x00: return "No Error";
    case 0x01: return "Feeder Jam";
    case 0x02: return "Upper passage Jam";
    case 0x03: return "Lower passage Jam";
    case 0x04: return "Reject Exit Jam";
    case 0x05: return "Stacker Exit Jam";
    case 0x06: return "Diverter Solenoid Fault";
    case 0x07: return "Stacker Pocket Residual Note";
    default:   return "Unknown Fault";
    }
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
