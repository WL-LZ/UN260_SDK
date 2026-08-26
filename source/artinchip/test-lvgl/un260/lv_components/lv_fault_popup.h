#ifndef LV_FAULT_POPUP_H
#define LV_FAULT_POPUP_H

#include "lvgl/lvgl.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FAULT_SRC_BOOT = 0,
    FAULT_SRC_START_COUNT,
    FAULT_SRC_RUNTIME,
} fault_source_t;

typedef enum {
    FAULT_CONFIRM_CLOSE = 0,
    FAULT_CONFIRM_GOTO_SENSOR,
} fault_confirm_action_t;

typedef struct {
    fault_source_t source;
    uint8_t code;

    const char* diagnostics_title;   // 红框1
    const char* fault_type_title;    // 红框2
    const char* fault_main_desc;     // 红框3

    const char* reason_text;
    const char* solution_text;

    const char* machine_img_path;
    const char* err_img_path;

    lv_coord_t radar_x;
    lv_coord_t radar_y;

    fault_confirm_action_t confirm_action;
} fault_popup_data_t;

void show_fault_popup_ex(const fault_popup_data_t* data);
void hide_fault_popup(void);
bool fault_popup_is_showing(void);
void fault_popup_set_auto_enabled(bool enabled);
bool fault_popup_get_auto_enabled(void);
void fault_popup_report_start_fault(uint8_t type, uint8_t code);
void fault_popup_report_start_no_note(void);
void fault_popup_report_runtime_fault(uint8_t code);
bool fault_popup_show_pending_now(void);
bool fault_popup_get_pending_fault(fault_source_t* source, uint8_t* fault_type, uint8_t* code);
void fault_popup_clear_pending(void);
bool fault_popup_has_pending_start_issue(void);
void fault_popup_auto_confirm_pending_if_needed(void);
void fault_popup_schedule_auto_confirm(void);
void fault_popup_cancel_auto_confirm(void);
void fault_popup_reset_auto_retry(void);

/* 协议适配入口 */
void show_boot_fault_popup(uint8_t selftest_type, uint8_t result);
void show_start_fault_popup(uint8_t type, uint8_t code);
void show_runtime_fault_popup(uint8_t code);

#endif
