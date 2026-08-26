#ifndef PAGE_01_DETAIL_SCROLL_H
#define PAGE_01_DETAIL_SCROLL_H

#include <stdbool.h>
#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void page_01_detail_scroll_attach(lv_obj_t* page_parent, lv_obj_t* scroll_container);
void page_01_scroll_hint_on_enter(void);
void page_01_scroll_hint_force_hide(void);
bool page_01_is_small_denom_mode(void);
void page_01_detail_scroll_before_section_switch(void);
void page_01_detail_scroll_after_section_switch(void);
void page_01_detail_scroll_sync_current_section(void);
void page_01_detail_scroll_reset_all(void);
int page_01_detail_scroll_first_row_get(int section);
int page_01_detail_row_gap_get(int section);

#ifdef __cplusplus
}
#endif

#endif // PAGE_01_DETAIL_SCROLL_H
