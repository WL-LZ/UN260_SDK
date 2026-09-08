#ifndef UN260_RENDER_SCRATCH_H
#define UN260_RENDER_SCRATCH_H

#include <stdbool.h>
#include <stdint.h>

#define RENDER_SCRATCH_RETAIN_LIMIT (2U * 1024U * 1024U)
#define RENDER_SCRATCH_IDLE_MS 5000U

/* UI-thread only. Initialize each lease to zero and do not copy a live lease.
 * data is uninitialized scratch, never an image cache. capacity may exceed the
 * request. Release on every path; a second release of the same lease is safe. */
typedef struct {
    void *data;
    uint32_t capacity;
    bool pooled;
} render_scratch_lease_t;

typedef struct {
    uint32_t hits;
    uint32_t misses;
    uint32_t temporary_allocations;
    uint32_t failures;
    uint32_t trims;
    uint32_t retained_bytes;
    uint32_t in_use_bytes;
    uint32_t total_bytes;
    uint32_t peak_bytes;
} render_scratch_stats_t;

/* All live and idle bytes remain claimed against IMAGE_MEM_CPU's existing
 * 4 MiB cap. At most one <=2 MiB allocation is retained. Nested borrowers and
 * larger requests receive independent, budgeted allocations freed on release.
 * Pass the same wrapping monotonic millisecond clock to all timed calls. */
bool render_scratch_acquire(render_scratch_lease_t *lease, uint32_t bytes,
                            uint32_t now_ms);
void render_scratch_release(render_scratch_lease_t *lease, uint32_t now_ms);
void render_scratch_poll(uint32_t now_ms);

/* Pressure/shutdown hook. Never frees a live lease; a busy retained buffer is
 * marked for eviction when its owner releases it. Temporaries remain private. */
void render_scratch_trim(void);

/* Interval counters plus current allocation gauges. Does not release memory. */
void render_scratch_take_stats(render_scratch_stats_t *out);

#endif
