#ifndef UN260_UI_FRAME_COMMIT_H
#define UN260_UI_FRAME_COMMIT_H
#include <stdbool.h>
#include <stdint.h>
/* UI-thread only. Queue visual projections, NEVER protocol actions/storage.
 * Context must outlive the request, or cancel it before destroying its owner. */
typedef void (*ui_frame_commit_fn)(void *context, uint32_t flags);
typedef struct { uint32_t requests, merged, callbacks, full; } ui_frame_commit_stats_t;
void ui_frame_commit_begin_batch(void);
void ui_frame_commit_end_batch(void);
/* Page construction/activation must finish its projection before snapshots,
 * suspend or first presentation. Nestable; preserves pending work and the
 * surrounding batch. End does not flush unrelated pages or run callbacks. */
void ui_frame_commit_begin_sync(void);
void ui_frame_commit_end_sync(void);
bool ui_frame_commit_is_batching(void);
/* false => caller applies synchronously (sync scope, no batch or capacity). */
bool ui_frame_commit_defer(ui_frame_commit_fn fn, void *context, uint32_t flags);
void ui_frame_commit_cancel(ui_frame_commit_fn fn, void *context);
void ui_frame_commit_flush(void);
bool ui_frame_commit_pending(void);
void ui_frame_commit_take_stats(ui_frame_commit_stats_t *out);
#endif
