#ifndef PAGE_07_CURR_H
#define PAGE_07_CURR_H

#include "lvgl/lvgl.h"
#include "un260/currency/currency_service.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_page_07_curr_create(lv_obj_t* parent);
void ui_page_07_curr_destroy(void);
bool ui_page_07_curr_resume(void);
void ui_page_07_curr_suspend(void);
bool ui_page_07_curr_prewarm_ready(void);
bool ui_page_07_curr_prepare_static_step(void);
void page_07_curr_img_refre(void);
void page_07_curr_img_reset(void);
void page_07_curr_apply_switch_result(const currency_switch_result_t* result);
void page_07_curr_apply_mode_result(uint8_t requested_mode, bool success);
void page_07_curr_poll_selection(void);
void page_07_curr_cancel_pending_selection(void);

#ifdef __cplusplus
}
#endif

#endif // PAGE_07_CURR_H
