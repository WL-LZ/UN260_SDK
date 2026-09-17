#ifndef PAGE_01_MAIN_H
#define PAGE_01_MAIN_H

#include "lvgl/lvgl.h"
#include <stdint.h>
#include "un260/lv_resources/lv_img_init.h" 

typedef enum {
    PAGE_01_DETAIL_SECTION_A = 0,
    PAGE_01_DETAIL_SECTION_B,
    PAGE_01_DETAIL_SECTION_C
} page_01_detail_section_t;

void ui_main_create(lv_obj_t* parent);
void ui_main_destroy(void);
bool page_01_main_is_created(void);
bool page_01_main_is_visible(void);
lv_obj_t *page_01_main_find_obj(const char *name);
lv_obj_t *page_01_main_scroll_obj(void);
void page_01_main_scroll_reset(void);
void page_01_main_refresh_start_state(void);
void page_01_main_refresh_totals(int total_pcs, const char *amount_text);

typedef enum {
    PAGE_01_MAIN_DIRTY_MODE = 1U << 0,
    PAGE_01_MAIN_DIRTY_ADD = 1U << 1,
    PAGE_01_MAIN_DIRTY_WORK = 1U << 2,
    PAGE_01_MAIN_DIRTY_BATCH = 1U << 3,
    PAGE_01_MAIN_DIRTY_FO = 1U << 4,
    PAGE_01_MAIN_DIRTY_CFD = 1U << 5,
    PAGE_01_MAIN_DIRTY_SPEED = 1U << 6,
    PAGE_01_MAIN_DIRTY_ERROR = 1U << 7,
    PAGE_01_MAIN_DIRTY_CURRENCY = 1U << 8,
    PAGE_01_MAIN_DIRTY_COUNTING = 1U << 9,
    PAGE_01_MAIN_DIRTY_LANGUAGE = 1U << 10,
    PAGE_01_MAIN_DIRTY_ALL = (1U << 11) - 1U,
} page_01_main_dirty_flag_t;

void page_01_main_mark_dirty(uint32_t flags);
bool page_01_main_defer_refresh(uint32_t flags);
void page_01_main_suspend(void);
bool page_01_main_resume(void);
void page_01_main_reveal_for_transition(void);
void page_01_update_language_texts(void);
void page_01_mode_switch_refre(void);
void page_01_add_refre(void);
void page_01_work_refre(void);
void page_01_batch_refre(void);
void page_01_face_refre(void);
void page_01_cfd_refre(void);
void page_01_speed_refre(void);
void page_01_err_num_refre(void);
void page_01_curr_img_refre(void);
void page_01_detail_section_set(page_01_detail_section_t section, bool refresh);
page_01_detail_section_t page_01_detail_section_get(void);
void page_01_bottom_a_refresh_mode(bool anim_en);
void page_01_bottom_a_refresh_mode_preview(uint8_t mode);
void page_01_bottom_a_refresh_add(bool anim_en);
void page_01_bottom_a_refresh_work(bool anim_en);
void page_01_bottom_a_refresh_fo(bool anim_en);
void page_01_bottom_c_refresh_batch(bool anim_en);
void page_01_bottom_c_refresh_speed(bool anim_en);
void page_01_bottom_c_refresh_cfd(void);

#endif // PAGE_01_MAIN_H
