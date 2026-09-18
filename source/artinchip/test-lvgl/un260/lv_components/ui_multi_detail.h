#ifndef UI_MULTI_DETAIL_H
#define UI_MULTI_DETAIL_H
#include "lvgl/lvgl.h"
typedef struct ui_multi_detail ui_multi_detail_t;
typedef void (*ui_multi_open_fn)(int currency,int tab,void *context);
ui_multi_detail_t *ui_multi_detail_create(lv_obj_t *parent,bool expanded,ui_multi_open_fn open,void *context);
lv_obj_t *ui_multi_detail_object(ui_multi_detail_t *v);
lv_obj_t *ui_multi_detail_scroll(ui_multi_detail_t *v);
/* Page owner may attach its own currency-selection navigation to this target. */
lv_obj_t *ui_multi_detail_currency_target(ui_multi_detail_t *v);
void ui_multi_detail_visible(ui_multi_detail_t *v,bool visible);
void ui_multi_detail_select(ui_multi_detail_t *v,int currency,int tab);
void ui_multi_detail_refresh(ui_multi_detail_t *v);
bool ui_multi_detail_back(ui_multi_detail_t *v);
void ui_multi_detail_search(ui_multi_detail_t *v);
void ui_multi_detail_destroy(ui_multi_detail_t *v);
#endif
