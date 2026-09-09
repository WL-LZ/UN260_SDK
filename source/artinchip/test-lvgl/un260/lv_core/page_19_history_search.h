#ifndef PAGE_19_HISTORY_SEARCH_H
#define PAGE_19_HISTORY_SEARCH_H

#include "lvgl/lvgl.h"
#include "un260/history/history_query.h"

typedef struct page_19_history_search page_19_history_search_t;
typedef void (*page_19_history_search_close_cb_t)(
    const history_query_input_t *applied, void *context);

/* The caller owns the modal and keeps records/details immutable until destroy.
 * Closing hides the modal before callback; callback may destroy the modal or
 * its parent. NULL applied means cancel; otherwise input is callback-local.
 * Deleting parent frees the modal automatically and invalidates its handle. */
page_19_history_search_t *page_19_history_search_create(lv_obj_t *parent,
    const history_query_input_t *initial, const history_query_record_t *records,
    size_t count, page_19_history_search_close_cb_t close, void *context);
void page_19_history_search_destroy(page_19_history_search_t *search);

#endif
