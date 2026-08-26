#ifndef LV_PAGE_EVENT_H
#define LV_PAGE_EVENT_H
//管理事件
#include "lvgl/lvgl.h"
#include "un260/app_service/setting_service.h"

void page_01_print_btn_event_cb(lv_event_t* e);
void page_01_qr_btn_event_cb(lv_event_t* e);

/*****************事件函数********************/


void page_switch_btn_event_cb(lv_event_t* e);
void page_01_back_btn_event_cb(lv_event_t* e);
void page_01_list_btn_event_cb(lv_event_t* e);
void page_02_history_btn_event_cb(lv_event_t* e);
void page_01_menu_btn_event_cb(lv_event_t* e);
 void page_06_back_btn_event_cb(lv_event_t* e) ;
// 跑钞仿真相关函数
void page_01_start_btn_event_cb(lv_event_t* e);
void page_01_esc_btn_event_cb(lv_event_t* e);
void page_01_mode_btn_event_cb(lv_event_t* e);
void page_01_bottom_mode_btn_event_cb(lv_event_t* e);
void page_01_add_btn_event_cb(lv_event_t* e);
void page_01_work_btn_event_cb(lv_event_t* e);
void page_01_fo_btn_event_cb(lv_event_t* e);
void page_01_bottom_batch_btn_event_cb(lv_event_t* e);
void page_01_bottom_speed_btn_event_cb(lv_event_t* e);

void page_01_set_btn_event_cb(lv_event_t* e);
void page_01_curr_btn_event_cb(lv_event_t* e);
// 密码键盘相关函数

void page_03_batch_label_input_event_cb(lv_event_t* e);
void page_03_void_batch_label_gesture_event_cb(lv_event_t* e);
void page_01_detail_area_event_cb(lv_event_t* e);

//page 03 batch按键
void page_03_batch_num_keypad_event_cb(lv_event_t* e);
void page_03_batch_num_keypad_clear_event_cb(lv_event_t* e);
void page_03_batch_num_keypad_enter_event_cb(lv_event_t* e);
void page_03_batch_set_result(bool success, const setting_batch_result_t *result);

//page 03 功能页面
void page_03_cfd_mode_event_cb(lv_event_t* e);
void page_03_speed_mode_event_cb(lv_event_t* e);
void page_03_add_mode_event_cb(lv_event_t* e);
void page_03_fo_mode_event_cb(lv_event_t* e);
void page_03_work_mode_event_cb(lv_event_t* e);
void page_03_update_menu_button_states_refresh(void);
void page_03_function_button_cache_reset(void);
//PAGE03 翻页
void page_03_a_up_event_cb(lv_event_t* e);
void page_03_a_down_event_cb(lv_event_t* e);
void page_03_b_up_event_cb(lv_event_t* e);
void page_03_b_down_event_cb(lv_event_t* e);
void page_03_c_up_event_cb(lv_event_t* e);
void page_03_c_down_event_cb(lv_event_t* e);
//PAGE 07 货币切换


#endif // !LV_PAGE_EVENT_H
