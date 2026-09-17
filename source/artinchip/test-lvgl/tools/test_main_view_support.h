#ifndef TEST_MAIN_VIEW_SUPPORT_H
#define TEST_MAIN_VIEW_SUPPORT_H
#include "un260/gesture/touch_feedback.h"
#include "un260/lv_system/ui_qr_data.h"
#include "un260/lv_core/page_18_pure.h"

/* Surrounding workflows are captured, never sent to hardware/storage. Main,
 * detail projection, styling, hit tests, fonts and Smart Island remain real. */
static unsigned callbacks[16], pushes, protocol_calls, persisted_tabs;
static bool start_busy, prewarming, touch_enabled, fault_auto;
static ui_page_t destination;
static int saved_tab;
enum { CB_MODE, CB_SETTING, CB_LIST, CB_PRINT, CB_MENU, CB_START, CB_CLEAR,
       CB_CURRENCY, CB_BOTTOM_MODE, CB_ADD, CB_WORK, CB_FO, CB_BATCH, CB_SPEED };
#define CAPTURE(name, id) void name(lv_event_t *e) { if(lv_event_get_code(e)==LV_EVENT_CLICKED) ++callbacks[id]; }
CAPTURE(page_01_mode_btn_event_cb, CB_MODE)
CAPTURE(page_01_set_btn_event_cb, CB_SETTING)
CAPTURE(page_01_list_btn_event_cb, CB_LIST)
CAPTURE(page_01_print_btn_event_cb, CB_PRINT)
CAPTURE(page_01_menu_btn_event_cb, CB_MENU)
CAPTURE(page_01_start_btn_event_cb, CB_START)
CAPTURE(page_01_esc_btn_event_cb, CB_CLEAR)
CAPTURE(page_01_curr_btn_event_cb, CB_CURRENCY)
CAPTURE(page_01_bottom_mode_btn_event_cb, CB_BOTTOM_MODE)
CAPTURE(page_01_add_btn_event_cb, CB_ADD)
CAPTURE(page_01_work_btn_event_cb, CB_WORK)
CAPTURE(page_01_fo_btn_event_cb, CB_FO)
CAPTURE(page_01_bottom_batch_btn_event_cb, CB_BATCH)
CAPTURE(page_01_bottom_speed_btn_event_cb, CB_SPEED)
#undef CAPTURE

void ui_manager_push_page(ui_page_t page) { ++pushes; destination=page; }
void ui_manager_switch(ui_page_t page) { destination=page; }
ui_page_t ui_manager_get_current_page(void) { return UI_PAGE_MAIN; }
bool ui_manager_is_prewarming_page(ui_page_t page) { (void)page; return prewarming; }
bool ui_manager_is_transitioning(void) { return false; }
bool app_command_runtime_count_start_busy(void) { return start_busy; }
bool protocol_send_is_ready(void) { return true; }
int protocol_send(uint8_t cmd,const uint8_t *data,uint16_t count)
{ (void)cmd;(void)data;(void)count;++protocol_calls;return 0; }
void pause_counting_sim(void) {}
void resume_counting_sim(void) {}
void start_counting_sim(void) {}
void stop_counting_sim(void) {}
void ui_count_end_anim_cancel(void) {}
void ui_state_apply_common_runtime(void) {}
int ui_state_page01_detail_section_get(void) { return saved_tab; }
void ui_state_save_page01_detail_section(void) { saved_tab=page_01_detail_section_get();++persisted_tabs; }
void ui_state_save_popup_auto_state(void) {}
void ui_state_save_pure_count_state(void) {}
bool ui_state_pure_count_is_enabled(void) { return false; }
bool ui_export_data_request(void) { return true; }
void machine_time_init(void) {}
void machine_time_get(machine_time_value_t *value)
{ *value=(machine_time_value_t){2026,9,9,18,23,12}; }
void machine_time_format(char *out,uint32_t size) { snprintf(out,size,"18:23:12"); }
bool perf_profile_is_enabled(void) { return false; }
void perf_profile_report_event_us(const char *group,const char *name,uint32_t us)
{ (void)group;(void)name;(void)us; }
void perf_profile_watch_invalidation(const void *object,const char *name) { (void)object;(void)name; }
void perf_profile_unwatch_invalidation(const void *object) { (void)object; }
bool lv_dma_static_skin_attach(lv_dma_static_skin_t *skin,lv_obj_t *object,const char *key)
{ (void)skin;(void)object;(void)key;return false; }
void lv_dma_static_skin_release(lv_dma_static_skin_t *skin) { (void)skin; }
void lv_print_toast_create(void) {}
void lv_print_toast_show_with_config(const lv_print_toast_config_t *config) { (void)config; }
lv_print_toast_config_t lv_print_toast_get_default_config(void)
{ lv_print_toast_config_t config={0};return config; }
bool fault_popup_get_auto_enabled(void) { return fault_auto; }
void fault_popup_set_auto_enabled(bool enabled) { fault_auto=enabled; }
bool fault_popup_get_pending_fault(fault_source_t *source,uint8_t *type,uint8_t *code)
{ (void)source;(void)type;(void)code;return false; }
bool fault_popup_show_pending_now(void) { return false; }
bool fault_popup_is_showing(void) { return false; }
void fault_popup_schedule_auto_confirm(void) {}
void fault_popup_clear_pending(void) {}
void fault_popup_reset_auto_retry(void) {}
void show_start_fault_popup(uint8_t type,uint8_t code) { (void)type;(void)code; }
void show_runtime_fault_popup(uint8_t code) { (void)code; }
bool touch_feedback_enabled(void) { return touch_enabled; }
bool touch_feedback_set_enabled(bool enabled) { touch_enabled=enabled;return true; }
bool ui_qr_data_is_ready(void) { return false; }
bool ui_qr_data_build(char *buffer,size_t size) { (void)buffer;(void)size;return false; }
bool lv_qr_popup_show(const char *text) { (void)text;return true; }
void ui_page_18_pure_request_exit(void) {}
#endif
