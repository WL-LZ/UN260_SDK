#ifndef UI_STATE_RUNTIME_H
#define UI_STATE_RUNTIME_H

#include <stdbool.h>

#include "un260/lv_system/ui_state_store.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_state_apply_common_runtime(void);
void ui_state_save_popup_auto_state(void);
bool ui_state_popup_auto_set_enabled(bool enabled);
bool ui_state_top_popup_set_enabled(bool enabled);
void ui_state_quick_order_get(uint8_t order[UI_STATE_QUICK_CONTROL_COUNT]);
/* Presentation only: hiding never disables an action or changes its order. */
uint16_t ui_state_quick_visible_get(void);
bool ui_state_quick_visible_set(uint8_t action,bool visible);
bool ui_state_quick_order_save(const uint8_t order[UI_STATE_QUICK_CONTROL_COUNT]);
void ui_state_save_pure_count_state(void);
bool ui_state_pure_count_is_enabled(void);
int ui_state_page01_detail_section_get(void);
void ui_state_save_page01_detail_section(void);
ui_main_batch_action_t ui_state_main_batch_action_get(void);
bool ui_state_main_batch_action_set(ui_main_batch_action_t action);
void ui_state_main_layout_get(ui_main_layout_t *layout);
bool ui_state_main_layout_save(const ui_main_layout_t *layout);
void ui_state_page07_get(ui_state_page07_t* state);
void ui_state_save_page07(const ui_state_page07_t* state);

#ifdef __cplusplus
}
#endif

#endif // UI_STATE_RUNTIME_H
