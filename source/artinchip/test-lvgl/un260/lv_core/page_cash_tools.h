#ifndef PAGE_CASH_TOOLS_H
#define PAGE_CASH_TOOLS_H
#include "lvgl/lvgl.h"
void ui_calculator_create(lv_obj_t *);
void ui_calculator_destroy(void);
void ui_cash_journal_create(lv_obj_t *);
void ui_cash_journal_destroy(void);
void ui_operators_create(lv_obj_t *);
void ui_operators_destroy(void);
void ui_cash_journal_filter_operator(uint32_t);
#endif
