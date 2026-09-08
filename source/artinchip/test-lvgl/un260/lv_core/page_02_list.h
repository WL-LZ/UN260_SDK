#ifndef PAGE_02_LIST_H
#define PAGE_02_LIST_H

#include "lvgl/lvgl.h"

typedef enum {
    PAGE_02_SECTION_A = 0,
    PAGE_02_SECTION_B,
    PAGE_02_SECTION_C,
    PAGE_02_SECTION_COUNT
} page_02_section_id_t;

void ui_page_02_list_create(lv_obj_t* parent);
void ui_page_02_list_destroy(void);
bool ui_page_02_list_resume(void);
void ui_page_02_list_suspend(void);
void page_02_list_section_data_ready(page_02_section_id_t section_id);
void page_02_list_section_mark_dirty(page_02_section_id_t section_id);
void page_02_list_report_reset(void);

#endif // PAGE_02_LIST_H
