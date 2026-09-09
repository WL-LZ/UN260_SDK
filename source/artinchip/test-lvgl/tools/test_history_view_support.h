#ifndef TEST_HISTORY_VIEW_SUPPORT_H
#define TEST_HISTORY_VIEW_SUPPORT_H
#include "lvgl/lvgl.h"
#include "un260/lv_core/page_19_history_search.h"
void history_test_tick(unsigned ms);
void history_test_render(void);
void history_test_click_at(lv_obj_t *object, const char *file, unsigned line);
#define history_test_click(object) history_test_click_at((object), __FILE__, __LINE__)
lv_obj_t *history_test_button(lv_obj_t *parent, const char *text);
unsigned history_test_timers(void);
void history_test_bmp(const char *name);
void history_test_search_module(void);
void history_test_search_apply(page_19_history_search_t *search,
                                const history_query_input_t *input);
#endif
