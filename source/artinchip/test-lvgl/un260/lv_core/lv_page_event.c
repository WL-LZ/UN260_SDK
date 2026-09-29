#include "un260/lv_components/ui_notice.h"
#include "lvgl/lvgl.h"
#include "un260/lv_core/lv_page_manager.h"
#include "lv_page_event.h"

#include <stdlib.h>

#include "un260/counting/counting_data_store.h"
#include "lvgl/src/misc/lv_timer.h"
#include "un260/lv_system/machine_time.h"
#include "un260/lv_core/page_03_menu.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_components/lv_qr_popup.h"
#include "un260/lv_components/lv_components.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_qr_data.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/lv_core/page_02_list.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/app_service/setting_service.h"
#include "un260/app_service/workspace_service.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/machine_state/machine_state.h"
#include "un260/currency/currency_state.h"

static void page_01_qr_show_toast(ui_text_id_t text_id)
{ui_notice_post(UI_NOTICE_WARNING,"export.qr","QR export",ui_text_get(text_id));}

static void page_01_qr_show_popup(void) //显示当前点钞结果二维码
{
    char qr_text[3072];

    if (app_command_runtime_count_start_busy()) {
        page_01_qr_show_toast(UI_TEXT_WIDGET_QR_POPUP_NO_DATA);
        return;
    }
    if (counting_data_current()->multi_currency_result) {
        if (!ui_qr_data_build_summary(qr_text, sizeof(qr_text))) {
            page_01_qr_show_toast(UI_TEXT_WIDGET_QR_POPUP_NO_DATA);
            return;
        }
    } else if (!counting_data_monetary_result_supported(counting_data_current())) {
        page_01_qr_show_toast(UI_TEXT_WIDGET_MULTI_RESULT_UNSUPPORTED);
        return;
    } else if (!ui_qr_data_build(qr_text, sizeof(qr_text))) {
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



void page_01_menu_btn_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {

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
    app_command_runtime_request_count_start();
}

void page_01_esc_btn_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    app_command_runtime_clear_counting_data("user clear");
}

static bool page_01_mode_req_busy(void) //判断模式切换是否仍在等待回包
{
    return setting_service_mode_is_pending();
}

static void page_01_mode_send_next(void) //发送主界面模式切换命令
{
    uint8_t next_mode = MODE_MDC;

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
    page_01_mode_send_next();
}

void page_01_bottom_mode_btn_event_cb(lv_event_t* e) //切换主界面底部A区点钞模式
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    page_01_mode_send_next();
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

void page_01_bottom_batch_btn_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!workspace_service_batch_next())
        ui_notice_post(UI_NOTICE_WARNING,"settings.batch","Batch unchanged","Finish the current operation, then try again.");
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
        uint8_t version_cmd = 0x01;
        protocol_send(0x17, &version_cmd, 1);
        ui_manager_switch(UI_PAGE_SET_PASSAGE);
        }
 }

void page_01_print_btn_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (currency_state_multi_selected() ||
        !counting_data_monetary_result_supported(counting_data_current())) {
        ui_notice_post(UI_NOTICE_WARNING,"print.request","Printing",ui_text_get(UI_TEXT_WIDGET_MULTI_RESULT_UNSUPPORTED));
        return;
    }

    // 只有金额和张数都为 0 时，才提示先点钞
    if (counting_data_current()->total_amount <= 0.0f && counting_data_current()->total_pcs <= 0) {
        ui_notice_post(UI_NOTICE_WARNING,"print.request","Printing",ui_text_get(UI_TEXT_WIDGET_PRINT_TOAST_COUNT_FIRST));
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

    if (protocol_send(0x3C, payload, 9) < 0) {
        ui_notice_post(UI_NOTICE_ERROR,"print.request","Print request not sent","Check controller connection.");
        return;
    }
    ui_notice_post(UI_NOTICE_INFO,"print.request","Print request sent",NULL);
}

void page_01_qr_btn_event_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (!counting_data_current()->multi_currency_result && !ui_qr_data_is_ready()) {
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

void page_03_batch_set_result(bool success, const setting_batch_result_t *result)
{
    if (result == NULL) return;
    page_03_menu_clear_batch_tip();

    if (success) {
        if (result->target.num > 0) {
            machine_state_confirm_batch(result->target.enable, result->target.num);
            page_01_bottom_c_refresh_batch(true);
            if (page_03_menu_is_visible()) {
                page_03_menu_refresh_batch_number();
                page_03_menu_show_batch_saved_tip();
            }
        }
    }

}
void page_03_update_menu_button_states_refresh(void)
{
    ui_page_03_menu_refresh_data(UI_DATA_TOPIC_MACHINE_SETTINGS);
}
