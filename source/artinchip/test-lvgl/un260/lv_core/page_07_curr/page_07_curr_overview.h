#ifndef PAGE07_CURR_OVERVIEW_H
#define PAGE07_CURR_OVERVIEW_H

#include "un260/lv_core/page_07_curr/page_07_curr_internal.h"

/* Currency-private view boundary. The page retains navigation, persistence
 * and command/ACK ownership; this module only builds and projects the view. */
typedef struct {
    lv_event_cb_t select;
    lv_event_cb_t favorite;
    lv_event_cb_t favorite_feedback;
    lv_event_cb_t filter;
    lv_event_cb_t card;
    lv_event_cb_t back;
} page07_curr_overview_actions_t;

void page07_curr_overview_build(page07_curr_context_t *ctx,
                               const page07_curr_overview_actions_t *actions);
void page07_curr_overview_selection(page07_curr_context_t *ctx);

#endif
