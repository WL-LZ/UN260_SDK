#include "ui_frame_commit.h"
#include <stddef.h>
#include <string.h>
#define UI_FRAME_COMMIT_CAPACITY 16U
typedef struct { ui_frame_commit_fn fn; void *context; uint32_t flags; } entry_t;
static entry_t pending[UI_FRAME_COMMIT_CAPACITY];
static unsigned batch_depth;
static unsigned sync_depth;
static bool flushing;
static ui_frame_commit_stats_t stats;
void ui_frame_commit_begin_batch(void) { ++batch_depth; }
void ui_frame_commit_end_batch(void) { if (batch_depth) --batch_depth; }
void ui_frame_commit_begin_sync(void) { ++sync_depth; }
void ui_frame_commit_end_sync(void) { if (sync_depth) --sync_depth; }
bool ui_frame_commit_is_batching(void)
{ return batch_depth != 0 && sync_depth == 0 && !flushing; }
bool ui_frame_commit_defer(ui_frame_commit_fn fn, void *context, uint32_t flags)
{
    entry_t *empty = NULL;
    if (!fn || !flags || !ui_frame_commit_is_batching()) return false;
    ++stats.requests;
    for (unsigned i = 0; i < UI_FRAME_COMMIT_CAPACITY; ++i) {
        if (pending[i].fn == fn && pending[i].context == context) {
            pending[i].flags |= flags;
            ++stats.merged;
            return true;
        }
        if (!pending[i].fn && !empty) empty = &pending[i];
    }
    if (!empty) { ++stats.full; return false; }
    *empty = (entry_t){fn, context, flags};
    return true;
}
void ui_frame_commit_cancel(ui_frame_commit_fn fn, void *context)
{
    for (unsigned i = 0; i < UI_FRAME_COMMIT_CAPACITY; ++i)
        if (pending[i].fn == fn && pending[i].context == context)
            pending[i] = (entry_t){0};
}
void ui_frame_commit_flush(void)
{
    if (flushing) return;
    flushing = true;
    for (unsigned i = 0; i < UI_FRAME_COMMIT_CAPACITY; ++i) {
        entry_t e = pending[i];
        pending[i] = (entry_t){0};
        if (e.fn) { ++stats.callbacks; e.fn(e.context, e.flags); }
    }
    flushing = false;
}
bool ui_frame_commit_pending(void)
{
    for (unsigned i = 0; i < UI_FRAME_COMMIT_CAPACITY; ++i)
        if (pending[i].fn) return true;
    return false;
}
void ui_frame_commit_take_stats(ui_frame_commit_stats_t *out)
{
    if (out) *out = stats;
    memset(&stats, 0, sizeof(stats));
}
