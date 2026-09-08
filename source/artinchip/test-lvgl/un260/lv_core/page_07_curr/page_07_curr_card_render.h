#ifndef PAGE_07_CURR_CARD_RENDER_H
#define PAGE_07_CURR_CARD_RENDER_H

#include <stdbool.h>

/* Currency-private UI-thread operations. Indices refer to this page's visible
 * card array; ownership stays in g_page07_curr, never a second global cache. */
void page07_curr_card_render_apply(int index, int x, int y);
/* Lookup/release only: safe in motion projection, never allocates a snapshot. */
bool page07_curr_card_render_sync_snapshots(int index, int focus);
/* Capture at most one missing face. Caller must ensure an active, idle page. */
bool page07_curr_card_render_prewarm_step(void);
/* Call after all image consumers have been removed from the object tree. */
void page07_curr_card_render_release_snapshots(void);

#endif
