#include "lvgl/lvgl.h"
#include "un260/lv_core/lv_page_manager.h"
#include "lv_page_event.h"

#include <stdlib.h>

#include "un260/counting/counting_data_store.h"
#include "lvgl/src/misc/lv_timer.h"
#include "un260/lv_system/machine_time.h"
#include "un260/lv_core/page_03_menu.h"
#include "un260/lv_core/page_01_detail_scroll.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_components/lv_qr_popup.h"
#include "un260/lv_components/lv_components.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_qr_data.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/lv_core/page_02_list.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/app_service/setting_service.h"
#include "un260/machine_state/machine_state.h"
#include "un260/currency/currency_state.h"

#define PAGE_01_DETAIL_TAP_THRESHOLD     10
typedef struct {
    lv_obj_t* beep[2];
    lv_obj_t* speed[3];
    lv_obj_t* add[2];
    lv_obj_t* fo[4];
    lv_obj_t* work[2];
    bool ready;
} page_03_function_button_cache_t;

static page_03_function_button_cache_t g_page_03_button_cache;

static const char* const g_page_03_beep_names[] = {
    "03_beep_on_btn", "03_beep_off_btn"
};
static const char* const g_page_03_speed_names[] = {
    "03_speed_800_btn", "03_speed_1000_btn", "03_speed_1200_btn"
};
static const char* const g_page_03_add_names[] = {
    "03_add_on_btn", "03_add_off_btn"
};
static const char* const g_page_03_fo_names[] = {
    "03_fo_OFF_btn", "03_fo_F_btn", "03_fo_O_btn", "03_fo_FO_btn"
};
static const char* const g_page_03_work_names[] = {
    "03_work_auto_btn", "03_work_manaul_btn"
};

void page_03_function_button_cache_reset(void)
{
    g_page_03_button_cache = (page_03_function_button_cache_t){ 0 };
}

static bool page_03_function_button_cache_prepare(void)
{
    if (g_page_03_button_cache.ready) {
        return true;
    }

    for (size_t i = 0; i < 2; i++) {
        g_page_03_button_cache.beep[i] =
            page_03_menu_find_obj(g_page_03_beep_names[i]);
        g_page_03_button_cache.add[i] =
            page_03_menu_find_obj(g_page_03_add_names[i]);
        g_page_03_button_cache.work[i] =
            page_03_menu_find_obj(g_page_03_work_names[i]);
        if (!g_page_03_button_cache.beep[i] ||
            !g_page_03_button_cache.add[i] ||
            !g_page_03_button_cache.work[i]) {
            page_03_function_button_cache_reset();
            return false;
        }
    }
    for (size_t i = 0; i < 3; i++) {
        g_page_03_button_cache.speed[i] =
            page_03_menu_find_obj(g_page_03_speed_names[i]);
        if (!g_page_03_button_cache.speed[i]) {
            page_03_function_button_cache_reset();
            return false;
        }
    }
    for (size_t i = 0; i < 4; i++) {
        g_page_03_button_cache.fo[i] =
            page_03_menu_find_obj(g_page_03_fo_names[i]);
        if (!g_page_03_button_cache.fo[i]) {
            page_03_function_button_cache_reset();
            return false;
        }
    }

    g_page_03_button_cache.ready = true;
    return true;
}

static void page_01_qr_show_toast(ui_text_id_t text_id) //显示二维码相关提示框
{
    lv_print_toast_config_t toast_cfg = lv_print_toast_get_default_config();

    toast_cfg.w = 320;
    toast_cfg.h = 101;
    toast_cfg.text = ui_text_get(text_id);
    toast_cfg.show_loader = false;
    toast_cfg.align_center = true;
    toast_cfg.use_text_area = false;
    toast_cfg.auto_hide_ms = 1200;

    lv_print_toast_show_with_config(&toast_cfg);
}

static void page_01_qr_show_popup(void) //显示当前点钞结果二维码
{
    char qr_text[3072];

    if (!ui_qr_data_build(qr_text, sizeof(qr_text))) {
        page_01_qr_show_toast(UI_TEXT_WIDGET_QR_POPUP_DATA_TOO_LARGE);
        return;
    }

    if (!lv_qr_popup_show(qr_text)) {
        page_01_qr_show_toast(UI_TEXT_WIDGET_QR_POPUP_DATA_TOO_LARGE);
    }
}

//跳转页面
void page_switch_btn_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_page_t target_page = (ui_page_t)(uintptr_t)lv_event_get_user_data(e);
        ui_manager_switch(target_page);
    }
}


void page_01_list_btn_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        page_01_main_icon_feedback("page_01_list_icon.png");
        /* 0x0C/0x0D 已在 0x0B 面额明细结束时提前发送，此处直接进 list */
        ui_manager_push_page(UI_PAGE_LIST);
    }
}

void page_02_history_btn_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ui_manager_push_page(UI_PAGE_HISTORY);
}

void page_01_detail_area_event_cb(lv_event_t* e)
{
    typedef struct {
        bool pressed;
        bool dragging;
        lv_point_t start_pt;
        lv_point_t last_pt;
    } detail_touch_state_t;

    static detail_touch_state_t s_touch = {0};
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = lv_indev_get_act();
    lv_obj_t* cont = lv_event_get_target(e);
    lv_point_t pt;

    if (cont == NULL || indev == NULL) return;

    lv_indev_get_point(indev, &pt);

    switch (code) {
    case LV_EVENT_PRESSED:
        s_touch.pressed = true;
        s_touch.dragging = false;
        s_touch.start_pt = pt;
        s_touch.last_pt = pt;
        break;

    case LV_EVENT_PRESSING:
        if (!s_touch.pressed) break;

        if (!s_touch.dragging) {
            if (LV_ABS(pt.y - s_touch.start_pt.y) > PAGE_01_DETAIL_TAP_THRESHOLD ||
                LV_ABS(pt.x - s_touch.start_pt.x) > PAGE_01_DETAIL_TAP_THRESHOLD) {
                s_touch.dragging = true;
            }
        }

        s_touch.last_pt = pt;
        break;

    case LV_EVENT_RELEASED:
        if (!s_touch.pressed) break;

        if (!s_touch.dragging &&
            LV_ABS(pt.y - s_touch.start_pt.y) < PAGE_01_DETAIL_TAP_THRESHOLD &&
            LV_ABS(pt.x - s_touch.start_pt.x) < PAGE_01_DETAIL_TAP_THRESHOLD) {
            ui_manager_push_page(UI_PAGE_LIST);
        }

        if (page_01_is_small_denom_mode() && cont && lv_obj_is_valid(cont)) {
            // 小面额币种支持拖动回弹，松手后自动回到顶部
            lv_obj_scroll_to_y(cont, 0, LV_ANIM_ON);
        }

        s_touch.pressed = false;
        s_touch.dragging = false;
        break;

    case LV_EVENT_PRESS_LOST:
        s_touch.pressed = false;
        s_touch.dragging = false;
        break;

    default:
        break;
    }
}

void page_01_menu_btn_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        page_01_main_icon_feedback("page_01_menu_icon.png");

        ui_manager_push_page(UI_PAGE_MENU);
    }
}

 void page_01_back_btn_event_cb(lv_event_t* e) {

     if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_switch(UI_PAGE_MAIN);
     }
 }

 void page_06_back_btn_event_cb(lv_event_t* e) {

     if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_pop_page();
     }
 }

void page_01_start_btn_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (page_01_main_is_created()) {
        page_01_main_icon_feedback("page_01_start_icon.png");
    }
    app_command_runtime_request_count_start();
}

void page_01_esc_btn_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (page_01_main_is_created()) {
        page_01_main_icon_feedback("page_01_esc_icon.png");
    }
    app_command_runtime_clear_counting_data("user clear");
    page_01_scroll_hint_force_hide();
}

// static Machine_Mode_t mode_next(int temp_mode)
// {
//     switch(temp_mode) {
//         case MODE_MDC: return Machine_MODE_SDC;
//         case MODE_SDC: return Machine_MODE_CNT;
//         case MODE_CNT: return Machine_MODE_MDC;
//         default:               return Machine_MODE_MDC; // 循环回到起点
//     }
// }


// void page_01_mode_btn_event_cb(lv_event_t* e)
// {
//     if (lv_event_get_code(e) == LV_EVENT_CLICKED)
//     {   
//         // 用当前模式来循环，而不是每次都从 MDC 开始
//         Machine_Mode_t Temp_Mode = mode_next(machine_state_mode());
//         machine_state_confirm_mode(Temp_Mode);  // 更新当前模式

//         uart_debug_printf("mode:%02X\n", Temp_Mode);
//         page_01_main_icon_feedback("page_01_mode_icon.png");

//         uint8_t send_data = 0;

//         if (strcmp(active_currency_code, "AUT") == 0)
//         {
//             send_data = Machine_AUT_MODE_MDC;
//         }
//         else if (strcmp(active_currency_code, "MUL") == 0)
//         {
//             send_data = Machine_MUL_MODE_MDC;
//         }
//         else
//         {
//             // 普通循环模式
//             send_data = Temp_Mode;
//         }

//         protocol_send(0x04, &send_data, 1);  // 发送字节
//         Machine_work_code.mode_code = send_data; // 保存当前发送的模式代码
//         uart_debug_printf("send_data:%02X\n", send_data);
        
//     }
// }

static bool page_01_mode_req_busy(void) //判断模式切换是否仍在等待回包
{
    return setting_service_mode_is_pending();
}

static void page_01_mode_send_next(bool show_icon_feedback) //发送主界面模式切换命令
{
    uint8_t next_mode = MODE_MDC;

    (void)show_icon_feedback;

    if (page_01_mode_req_busy()) {
        return;
    }

    if (machine_state_mode() == MODE_MDC)
        next_mode = MODE_SDC;
    else if (machine_state_mode() == MODE_SDC)
        next_mode = MODE_CNT;
    else if (machine_state_mode() == MODE_CNT)
        next_mode = MODE_MDC;
    else
        next_mode = MODE_MDC;

    setting_service_request_mode(next_mode);
}

void page_01_mode_btn_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    page_01_mode_send_next(true);
}

void page_01_bottom_mode_btn_event_cb(lv_event_t* e) //切换主界面底部A区点钞模式
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    page_01_mode_send_next(false);
}

void page_01_add_btn_event_cb(lv_event_t* e) //切换主界面底部ADD开关
{
    bool target;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    target = !machine_state_add_enabled();
    if (!setting_service_request_add(target)) return;
}

void page_01_work_btn_event_cb(lv_event_t* e) //切换主界面底部工作模式
{
    uint8_t target_mode;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    target_mode = machine_state_work_mode() ? 0 : 1;
    if (!setting_service_request_work_mode(target_mode)) return;
}

void page_01_fo_btn_event_cb(lv_event_t* e) //切换主界面底部F/O开关
{
    uint8_t target_mode;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    target_mode = (uint8_t)((machine_state_fo_mode() + 1) % 4);
    /* 协议第31条：0x00=OFF, 0x01=Face, 0x02=ORT, 0x03=Face&ORT */
    if (!setting_service_request_fo_mode(target_mode)) return;
}

void page_01_bottom_batch_btn_event_cb(lv_event_t* e) //进入主界面底部C区Batch设置页
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    ui_manager_push_page(UI_PAGE_MENU);
}

void page_01_bottom_speed_btn_event_cb(lv_event_t* e) //切换主界面底部C区速度
{
    uint8_t target_speed;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    target_speed = (uint8_t)((machine_state_speed() + 1) % 3);
    if (!setting_service_request_speed(target_speed)) return;
}


void page_01_set_btn_event_cb(lv_event_t* e){
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        page_01_main_icon_feedback("page_01_set_icon.png");
        uint8_t version_cmd = 0x01;
        protocol_send(0x17, &version_cmd, 1);
        ui_manager_switch(UI_PAGE_SET_PASSAGE);
        }
 }

void page_01_print_btn_event_cb(lv_event_t* e)
{
    lv_print_toast_config_t toast_cfg;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    // 只有金额和张数都为 0 时，才提示先点钞
    if (counting_data_current()->total_amount <= 0.0f && counting_data_current()->total_pcs <= 0) {
        toast_cfg = lv_print_toast_get_default_config();
        toast_cfg.w = 320;
        toast_cfg.h = 101;
        toast_cfg.text = ui_text_get(UI_TEXT_WIDGET_PRINT_TOAST_COUNT_FIRST);
        toast_cfg.show_loader = true;
        toast_cfg.align_center = true;
        toast_cfg.use_text_area = false;
        toast_cfg.loader_color = lv_color_hex(0xC0392B);
        toast_cfg.auto_hide_ms = 2000;

        lv_print_toast_show_with_config(&toast_cfg);
        return;
    }

    machine_time_value_t now;
    uint8_t payload[9];
    char curr_code[4];

    machine_time_get(&now);
    currency_state_get_active_code(curr_code);
    payload[0] = (uint8_t)curr_code[0];
    payload[1] = (uint8_t)curr_code[1];
    payload[2] = (uint8_t)curr_code[2];
    payload[3] = (uint8_t)(now.year >= 2000 ? (now.year - 2000) : now.year);
    payload[4] = now.month;
    payload[5] = now.day;
    payload[6] = now.hour;
    payload[7] = now.minute;
    payload[8] = now.second;

    lv_print_toast_show(ui_text_get(UI_TEXT_WIDGET_PRINT_TOAST_PRINTING));
    protocol_send(0x3C, payload, 9);
}

void page_01_qr_btn_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (!ui_qr_data_is_ready()) {
        page_01_qr_show_toast(UI_TEXT_WIDGET_QR_POPUP_NO_DATA);
        return;
    }

    page_01_qr_show_popup();
}

void page_01_curr_btn_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {

        ui_manager_switch(UI_PAGE_CURR);
    }

}

// 输入事件回调函数（处理触摸事件）

void page_03_batch_label_input_event_cb(lv_event_t* e)
{
    /* Amount batch 暂未启用：先屏蔽该区域手势切换逻辑，后续可恢复 */
    LV_UNUSED(e);
    return;
#if 0
    if (!machine_state_batch_enabled()) return;

    static struct {
        bool is_dragging;      // 是否正在拖拽
        lv_coord_t start_y;    // 开始Y坐标
        lv_coord_t current_y;  // 当前Y坐标
        bool has_switched;     // 是否已经切换过
    } drag_state = { false, 0, 0, false };
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = lv_indev_get_act();
    lv_point_t point;
    lv_indev_get_point(indev, &point);

    switch (code) {
    case LV_EVENT_PRESSED:
        drag_state.is_dragging = true;
        drag_state.start_y = point.y;
        drag_state.current_y = point.y;
        drag_state.has_switched = false;
        break;

    case LV_EVENT_PRESSING:
        if (drag_state.is_dragging) {
            drag_state.current_y = point.y;
            lv_coord_t diff = drag_state.current_y - drag_state.start_y;

            if (!drag_state.has_switched && abs(diff) > 20) {
                // 进行一次模式切换
                toggle_batch_mode();
                drag_state.has_switched = true;
            }
        }
        break;

    case LV_EVENT_RELEASED:
        drag_state.is_dragging = false;
        drag_state.has_switched = false;
        break;

    case LV_EVENT_GESTURE:
    {
        // 切换模式
        lv_dir_t dir = lv_indev_get_gesture_dir(indev);
        if (dir == LV_DIR_TOP || dir == LV_DIR_BOTTOM) {
            toggle_batch_mode();
        }
    }
    break;

    default:
        break;
    }
    page_03_menu_refresh_batch_mode();
#endif
}


// 手势事件回调函数
void page_03_void_batch_label_gesture_event_cb(lv_event_t* e)
{
    /* Amount batch 暂未启用：先屏蔽手势切换逻辑，后续可恢复 */
    LV_UNUSED(e);
    return;
#if 0
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

        if (dir == LV_DIR_BOTTOM) {
            switch_to_pcs_batch();
        }
        else if (dir == LV_DIR_TOP) {
            switch_to_amount_batch();
        }
    }
#endif
}



void page_03_batch_num_keypad_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const char* password_get_txt = lv_event_get_user_data(e);
    if (!password_get_txt) return;
    page_03_batch_num_edit_input(password_get_txt[0]);
}

void page_03_batch_num_keypad_clear_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    page_03_batch_num_edit_reset();
}




void page_03_batch_num_keypad_enter_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    page_03_menu_clear_batch_tip();
    int num = 0;
    if (!page_03_batch_num_edit_value(&num)) {
        num = machine_state_batch_num();
    }
    if (num <= 0) num = 200;
    if (num < 5) num = 5;
    if (num >= 200) {
        num = 200;
    }

    /* ================== 0x06 设置清分机预置数量 ==================
     * 开关 ON：发送用户预设值
     * 开关 OFF：固定发送 200
     */
    if (!setting_service_request_batch_number((uint8_t)num, machine_state_batch_enabled(), machine_state_batch_num())) {
        return;
    }
    // 确认后清空输入缓存，回到 0，等待下一次重新输入
    page_03_batch_num_edit_reset();
    printf("batch pending:%d\n", num);


}
void page_03_batch_set_result(bool success, const setting_batch_result_t *result)
{
    if (result == NULL) return;
    page_03_menu_clear_batch_tip();

    if (success) {
        if (result->target.num > 0) {
            machine_state_confirm_batch(result->target.enable, result->target.num);
            batch_switch_set_last_on_num(machine_state_batch_num());
            page_01_batch_refre();
            if (page_03_menu_is_visible()) {
                set_batch_switch_state(machine_state_batch_enabled());
                page_03_menu_refresh_batch_number();
                page_03_batch_num_edit_reset();
                page_03_menu_show_batch_saved_tip();
            }
        }
    }

}
void page_03_update_menu_button_states_refresh(void)
{
    /* boot 阶段会收到参数同步帧(0x38/0x39/0x3A/0x15等)，
       但菜单页对象可能尚未创建。这里必须做到“无对象就直接跳过”，
       否则会对 NULL 调用 lv_obj_set_style_* 导致卡死/崩溃。 */
    if (!page_03_menu_is_visible()) {
        return;
    }

    if (!page_03_function_button_cache_prepare()) {
        return;
    }
    lv_color_t selected_blue_color = lv_color_hex(0x0B69FF);
    lv_color_t selected_off_color = lv_color_hex(0x9AA6B2);
    lv_color_t unselected_color = lv_color_hex(0xEEF2F7);
    lv_color_t selected_text_color = lv_color_make(255, 255, 255);
    lv_color_t unselected_text_color = lv_color_hex(0x747B84);

    #define PAGE_03_APPLY_FUNCTION_BTN(_obj, _sel, _off_selected) do {                  \
        bool _selected = (_sel);                                                        \
        bool _off = (_off_selected);                                                    \
        lv_obj_t* _btn = (_obj);                                                        \
        lv_color_t _bg = _selected ? (_off ? selected_off_color : selected_blue_color)   \
                                   : unselected_color;                                  \
        if (_btn) {                                                                     \
            lv_damped_button_set_palette(_btn, _bg,                                     \
                                         lv_color_darken(_bg, LV_OPA_20));               \
            lv_obj_t* _label = lv_obj_get_child(_btn, 0);                               \
            if (_label) {                                                               \
                lv_obj_set_style_text_color(_label,                                    \
                    _selected ? selected_text_color : unselected_text_color, 0);        \
            }                                                                           \
        }                                                                               \
    } while (0)

    // BEEP 处理（配色与 ADD 一致）
    lv_obj_t* tmp_beep_on_obj = g_page_03_button_cache.beep[0];
    lv_obj_t* tmp_beep_off_obj = g_page_03_button_cache.beep[1];
    bool beep_on = machine_state_buzzer_enabled();
    if (tmp_beep_on_obj && tmp_beep_off_obj) {
        PAGE_03_APPLY_FUNCTION_BTN(tmp_beep_on_obj, beep_on, false);
        PAGE_03_APPLY_FUNCTION_BTN(tmp_beep_off_obj, !beep_on, true);
    }
    //speed 处理
    for (int i = 0; i < SPEED_MODE; i++)
    {
        lv_obj_t* tmp_speed_obj = g_page_03_button_cache.speed[i];
        bool sel = (i == machine_state_speed());
        if (!tmp_speed_obj) continue;
        PAGE_03_APPLY_FUNCTION_BTN(tmp_speed_obj, sel, false);
    }
    //FO处理
    for (int i = 0; i < FO_MODE; i++)
    {
        lv_obj_t* tmp_fo_obj = g_page_03_button_cache.fo[i];
        bool sel = (i == machine_state_fo_mode());
        if (!tmp_fo_obj) continue;
        PAGE_03_APPLY_FUNCTION_BTN(tmp_fo_obj, sel, i == 0);
    }
    //work处理
    for (int i = 0; i < WORK_MODE; i++)
    {
        lv_obj_t* tmp_work_obj = g_page_03_button_cache.work[i];
        bool sel = (i == machine_state_work_mode());
        if (!tmp_work_obj) continue;
        PAGE_03_APPLY_FUNCTION_BTN(tmp_work_obj, sel, false);
    }
    //ADD 处理
        lv_obj_t* tmp_add_on_obj = g_page_03_button_cache.add[0];
        lv_obj_t* tmp_add_off_obj = g_page_03_button_cache.add[1];
        bool sel = machine_state_add_enabled();
        if (!tmp_add_on_obj || !tmp_add_off_obj) return;
        PAGE_03_APPLY_FUNCTION_BTN(tmp_add_on_obj, sel, false);
        PAGE_03_APPLY_FUNCTION_BTN(tmp_add_off_obj, !sel, true);

    #undef PAGE_03_APPLY_FUNCTION_BTN

    page_03_menu_sync_button_skins();
}

// BEEP 模式（复用原 CFD 回调）
void page_03_cfd_mode_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const char* beep_str = lv_event_get_user_data(e);
    uint8_t beep_code = atoi(beep_str);
    bool target = (beep_code > 0) ? true : false;

    if (target == machine_state_buzzer_enabled()) {
        return;
    }

    if (!setting_service_request_beep(target)) return;
#if LV_DEBUG
    printf("BEEP mode request -> %s\n", target ? "ON" : "OFF");
#endif
}

//speed模式
void page_03_speed_mode_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const char* speed_str = lv_event_get_user_data(e);
    uint8_t speed_code = atoi(speed_str);
    if (speed_code >= SPEED_MODE || speed_code == machine_state_speed()) return;
    /* ================== 0x16 设置清分机点钞速度 ================== */
    /* 协议定义：0x01=1000张/分钟, 0x02=800张/分钟, 0x03=600张/分钟 */
    if (!setting_service_request_speed(speed_code)) return;
#if LV_DEBUG
    printf("速度模式请求切换到： %u\n", speed_code);
#endif // LV_DEBUG

}

//ADD模式
void page_03_add_mode_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const char* add_str = lv_event_get_user_data(e);
    uint8_t add_code = atoi(add_str);
    bool target = (add_code > 0) ? true : false;

    if (target == machine_state_add_enabled()) return;
    if (!setting_service_request_add(target)) return;
#if LV_DEBUG
    printf("ADD模式请求切换为：%s\n", target ? "ON" : "OFF");
#endif // LV_DEBUG
}

//fo模式
void page_03_fo_mode_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const char* fo_str = lv_event_get_user_data(e);
    uint8_t fo_code = atoi(fo_str);
    if (fo_code >= FO_MODE || fo_code == machine_state_fo_mode()) return;
    if (fo_code <= 3) {
        /* 协议第31条：菜单页直接发送 0~3 编码 */
        if (!setting_service_request_fo_mode(fo_code)) return;
    }
#if LV_DEBUG
    char* fo[] = {"OFF","F","O","F/O"};
    printf("F/O 模式请求切换为：%s\n", fo[fo_code]);
#endif // LV_DEBUG


}


//work模式
void page_03_work_mode_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)return;
    const char* word_str = lv_event_get_user_data(e);
    uint8_t word_code = atoi(word_str);
    if (word_code >= WORK_MODE || word_code == machine_state_work_mode()) return;
    if (!setting_service_request_work_mode(word_code)) return;
#if LV_DEBUG
    printf("工作模式请求切换为：%s\n", (word_code > 0) ? "MANUAL" : "AUTO");
#endif // LV_DEBUG
}
