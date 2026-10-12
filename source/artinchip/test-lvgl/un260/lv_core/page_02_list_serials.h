#ifndef PAGE_02_LIST_SERIALS_H
#define PAGE_02_LIST_SERIALS_H
#include "lvgl/lvgl.h"
#include "page_02_list_data.h"

/* List-owned, fixed-page subview; projection is borrowed. No counting strings are retained. */
typedef struct page_02_list_serials page_02_list_serials_t;
typedef void (*page_02_list_serials_close_fn)(void *context);
page_02_list_serials_t *page_02_list_serials_create(lv_obj_t *parent,
    const page_02_list_data_t *projection,unsigned initial_rank,
    bool monetary_supported,page_02_list_serials_close_fn close,
    page_02_list_serials_close_fn main,void *context);
void page_02_list_serials_refresh(page_02_list_serials_t *view,bool reset,bool monetary_supported);
void page_02_list_serials_set_visible(page_02_list_serials_t *view,bool visible);
void page_02_list_serials_destroy(page_02_list_serials_t *view);
#endif
