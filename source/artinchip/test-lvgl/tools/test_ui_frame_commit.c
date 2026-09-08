#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "un260/lv_core/ui_frame_commit.h"
static unsigned calls;
static uint32_t received;
static bool visible = true;
static uint32_t hidden_dirty;
static void project(void *context, uint32_t flags)
{
    (void)context;
    calls++;
    if (!visible) { hidden_dirty |= flags; return; }
    received |= flags;
    /* Real projection functions pass through the same defer entry again. */
    assert(!ui_frame_commit_defer(project, context, flags));
}
int main(void)
{
    assert(!ui_frame_commit_defer(project, 0, 1)); /* create/navigation sync */
    ui_frame_commit_begin_batch();
    for (unsigned i=0; i<1000; ++i)
        assert(ui_frame_commit_defer(project, 0, 1U << (i%10)));
    ui_frame_commit_end_batch();
    assert(calls == 0 && ui_frame_commit_pending());
    ui_frame_commit_flush();
    assert(calls == 1 && received == 1023 && !ui_frame_commit_pending());
    ui_frame_commit_begin_batch();
    ui_frame_commit_begin_batch();
    assert(ui_frame_commit_defer(project, 0, 16));
    ui_frame_commit_end_batch();
    assert(ui_frame_commit_is_batching());
    ui_frame_commit_end_batch();
    visible = false;
    ui_frame_commit_flush();
    assert(hidden_dirty == 16); /* hidden retained page must not lose changes */
    ui_frame_commit_begin_batch();
    for (uintptr_t i=1; i<=16; ++i)
        assert(ui_frame_commit_defer(project, (void*)i, 1));
    assert(!ui_frame_commit_defer(project, (void*)17, 1)); /* sync fallback */
    ui_frame_commit_cancel(project, (void*)4);
    assert(ui_frame_commit_defer(project, (void*)17, 2));
    ui_frame_commit_end_batch();
    ui_frame_commit_flush();
    assert(calls == 18);
    ui_frame_commit_stats_t stats;
    ui_frame_commit_take_stats(&stats);
    assert(stats.merged == 999 && stats.full == 1 && stats.callbacks == calls);
    puts("UI frame commit: merge, nested batches, hidden retention, capacity/cancel PASS");
}
