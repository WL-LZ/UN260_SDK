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

    /* Initial projection must not enter the surrounding runtime batch or
     * flush/drop existing work belonging to any page. */
    visible = true;
    unsigned prior_calls = calls;
    ui_frame_commit_begin_batch();
    assert(ui_frame_commit_defer(project, 0, 32));
    ui_frame_commit_begin_sync();
    assert(!ui_frame_commit_is_batching());
    assert(!ui_frame_commit_defer(project, (void*)1, 64));
    ui_frame_commit_begin_sync();
    ui_frame_commit_begin_batch();
    assert(!ui_frame_commit_defer(project, (void*)2, 128));
    ui_frame_commit_end_sync();
    assert(!ui_frame_commit_is_batching());
    ui_frame_commit_end_batch();
    assert(ui_frame_commit_pending() && calls == prior_calls);
    ui_frame_commit_end_sync();
    assert(ui_frame_commit_is_batching());
    assert(ui_frame_commit_pending() && calls == prior_calls);
    assert(ui_frame_commit_defer(project, 0, 64));
    ui_frame_commit_end_batch();
    received = 0;
    ui_frame_commit_flush();
    assert(calls == prior_calls + 1 && received == (32U | 64U));
    ui_frame_commit_end_sync(); /* unmatched end must not underflow */
    ui_frame_commit_begin_batch();
    assert(ui_frame_commit_is_batching());
    ui_frame_commit_end_batch();
    puts("UI frame commit: merge, nesting, hidden retention, capacity/cancel and synchronous lifecycle PASS");
}
