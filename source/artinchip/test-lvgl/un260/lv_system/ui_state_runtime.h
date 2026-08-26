#ifndef UI_STATE_RUNTIME_H
#define UI_STATE_RUNTIME_H

#include <stdbool.h>

#include "un260/lv_system/ui_state_store.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_state_apply_common_runtime(void);
void ui_state_save_popup_auto_state(void);
void ui_state_save_pure_count_state(void);
bool ui_state_pure_count_is_enabled(void);
int ui_state_page01_detail_section_get(void);
void ui_state_save_page01_detail_section(void);
void ui_state_page07_get(ui_state_page07_t* state);
void ui_state_save_page07(const ui_state_page07_t* state);

#ifdef __cplusplus
}
#endif

#endif // UI_STATE_RUNTIME_H
