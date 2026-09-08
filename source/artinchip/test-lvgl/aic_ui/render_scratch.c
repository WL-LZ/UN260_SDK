#include "render_scratch.h"

#include <limits.h>
#include <stdlib.h>

#include "image_memory.h"

static void *g_retained;
static uint32_t g_capacity;
static uint32_t g_idle_since_ms;
static render_scratch_lease_t *g_owner;
static bool g_trim_on_release;
static uint32_t g_temporary_bytes;
static uint32_t g_total_bytes;
static render_scratch_stats_t g_stats;

static void scratch_free(void *data, uint32_t bytes)
{
    free(data);
    image_mem_release(IMAGE_MEM_CPU, bytes);
    g_total_bytes -= bytes;
}

static void scratch_note_allocation(uint32_t bytes)
{
    g_total_bytes += bytes;
    if (g_total_bytes > g_stats.peak_bytes) g_stats.peak_bytes = g_total_bytes;
}

void render_scratch_trim(void)
{
    if (g_owner != NULL) {
        g_trim_on_release = true;
        return;
    }
    if (g_retained != NULL) {
        scratch_free(g_retained, g_capacity);
        g_retained = NULL;
        g_capacity = 0;
        g_stats.trims++;
    }
    g_trim_on_release = false;
}

static void scratch_reclaim(uint32_t bytes)
{
    (void)bytes;
    render_scratch_trim();
}

void render_scratch_poll(uint32_t now_ms)
{
    if (g_retained != NULL && g_owner == NULL &&
        (uint32_t)(now_ms - g_idle_since_ms) >= RENDER_SCRATCH_IDLE_MS) {
        render_scratch_trim();
    }
}

bool render_scratch_acquire(render_scratch_lease_t *lease, uint32_t bytes,
                            uint32_t now_ms)
{
    void *allocation;
    uint32_t capacity;
    bool retain;

    if (lease == NULL || lease->data != NULL || bytes == 0 ||
        bytes > UINT32_MAX - 4095U) {
        g_stats.failures++;
        return false;
    }
    capacity = (bytes + 4095U) & ~4095U;
    render_scratch_poll(now_ms);
    image_mem_register(IMAGE_MEM_CPU, scratch_reclaim);

    if (g_owner == NULL && g_temporary_bytes == 0 &&
        g_retained != NULL && g_capacity >= capacity) {
        *lease = (render_scratch_lease_t){g_retained, g_capacity, true};
        g_owner = lease;
        g_stats.hits++;
        return true;
    }
    g_stats.misses++;
    retain = g_owner == NULL && g_temporary_bytes == 0 &&
             capacity <= RENDER_SCRATCH_RETAIN_LIMIT;

    /* Replacement reserves the complete new allocation while the idle old
     * buffer is still accounted, including the allocation-in-flight peak.
     * Pressure may evict only that idle buffer before one budget retry. */
    if (!image_mem_acquire(IMAGE_MEM_CPU, capacity)) {
        if (g_owner == NULL && g_retained != NULL) {
            render_scratch_trim();
            if (!image_mem_acquire(IMAGE_MEM_CPU, capacity)) {
                g_stats.failures++;
                return false;
            }
        } else {
            g_stats.failures++;
            return false;
        }
    }
    allocation = malloc(capacity);
    if (allocation == NULL) {
        image_mem_release(IMAGE_MEM_CPU, capacity);
        g_stats.failures++;
        return false;
    }
    scratch_note_allocation(capacity);

    if (retain) {
        /* No realloc: malloc failure leaves an existing idle buffer usable,
         * and a nested request can never replace the outer borrower's data. */
        render_scratch_trim();
        g_retained = allocation;
        g_capacity = capacity;
        g_owner = lease;
    } else {
        g_temporary_bytes += capacity;
        g_stats.temporary_allocations++;
    }
    *lease = (render_scratch_lease_t){allocation, capacity, retain};
    return true;
}

void render_scratch_release(render_scratch_lease_t *lease, uint32_t now_ms)
{
    if (lease == NULL || lease->data == NULL) return;
    if (lease->pooled) {
        /* A copied/mismatched handle must not release another borrower's pool. */
        if (g_owner != lease || lease->data != g_retained) return;
        g_owner = NULL;
        g_idle_since_ms = now_ms;
        if (g_trim_on_release) render_scratch_trim();
    } else {
        scratch_free(lease->data, lease->capacity);
        g_temporary_bytes -= lease->capacity;
    }
    *lease = (render_scratch_lease_t){0};
}

void render_scratch_take_stats(render_scratch_stats_t *out)
{
    if (out == NULL) return;
    *out = g_stats;
    out->retained_bytes = g_capacity;
    out->in_use_bytes = g_temporary_bytes + (g_owner != NULL ? g_capacity : 0U);
    out->total_bytes = g_total_bytes;
    g_stats = (render_scratch_stats_t){.peak_bytes = g_total_bytes};
}
