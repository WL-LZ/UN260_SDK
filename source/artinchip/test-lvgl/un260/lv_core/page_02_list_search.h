#ifndef PAGE_02_LIST_SEARCH_H
#define PAGE_02_LIST_SEARCH_H
#include "lvgl/lvgl.h"
#include <stdint.h>

/* A private workspace in the LIST session, not a new protocol/navigation page.
 * UINT16_MAX means close without positioning an original record. */
typedef struct page_02_list_search page_02_list_search_t;
typedef void (*page_02_list_search_close_fn)(uint16_t source_slot, void *context);
page_02_list_search_t *page_02_list_search_create(lv_obj_t *parent,
    page_02_list_search_close_fn close, void *context);
void page_02_list_search_data_changed(page_02_list_search_t *search);
void page_02_list_search_destroy(page_02_list_search_t *search);
#endif
