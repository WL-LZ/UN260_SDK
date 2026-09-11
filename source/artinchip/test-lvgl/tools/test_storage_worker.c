#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/storage/storage_worker.h"

/* Instrument only the actual worker's allocation boundary, not libc/pthreads. */
static bool fail_allocation;
static size_t allocation_calls, allocated_bytes;
static struct { void *ptr; size_t size; } allocations[STORAGE_WORKER_CAPACITY];

static void *worker_test_malloc(size_t size)
{
    allocation_calls++;
    if (fail_allocation) return NULL;
    void *ptr = malloc(size);
    assert(ptr);
    for (unsigned i = 0; i < STORAGE_WORKER_CAPACITY; ++i) {
        if (allocations[i].ptr) continue;
        allocations[i].ptr = ptr;
        allocations[i].size = size;
        allocated_bytes += size;
        return ptr;
    }
    assert(!"Worker allocated beyond its bounded job count");
    return NULL;
}

static void worker_test_free(void *ptr)
{
    if (!ptr) return;
    for (unsigned i = 0; i < STORAGE_WORKER_CAPACITY; ++i) {
        if (allocations[i].ptr != ptr) continue;
        allocated_bytes -= allocations[i].size;
        allocations[i].ptr = NULL;
        allocations[i].size = 0;
        free(ptr);
        return;
    }
    assert(!"Unknown or double-freed worker snapshot");
}

#define malloc worker_test_malloc
#define free worker_test_free
#include "un260/storage/storage_worker.c"
#undef malloc
#undef free

static pthread_mutex_t gate_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_changed = PTHREAD_COND_INITIALIZER;
static bool held, entered;
static atomic_bool fail_run;
static atomic_uint run_count;
static unsigned order[32];

static void hold_worker(bool hold)
{
    pthread_mutex_lock(&gate_lock);
    held = hold;
    if (hold) entered = false;
    pthread_cond_broadcast(&gate_changed);
    pthread_mutex_unlock(&gate_lock);
}

static void wait_entered(void)
{
    pthread_mutex_lock(&gate_lock);
    while (!entered) pthread_cond_wait(&gate_changed, &gate_lock);
    pthread_mutex_unlock(&gate_lock);
}

static bool run_snapshot(const void *snapshot, size_t size)
{
    const unsigned char *bytes = snapshot;
    assert(size > 0 && size <= STORAGE_WORKER_MAX_JOB_BYTES);
    assert((uintptr_t)snapshot % _Alignof(max_align_t) == 0);
    pthread_mutex_lock(&gate_lock);
    entered = true;
    pthread_cond_broadcast(&gate_changed);
    while (held) pthread_cond_wait(&gate_changed, &gate_lock);
    pthread_mutex_unlock(&gate_lock);
    for (size_t i = 0; i < size; ++i) assert(bytes[i] == bytes[0]);
    unsigned index = atomic_load(&run_count);
    assert(index < sizeof(order) / sizeof(order[0]));
    order[index] = bytes[0];
    atomic_store(&run_count, index + 1);
    return !atomic_load(&fail_run);
}

int main(void)
{
    storage_job_id_t ids[STORAGE_WORKER_CAPACITY], rejected = UINT64_C(987654);
    unsigned char small[17], copied[17];
    size_t before_allocations;
    memset(small, 1, sizeof(small));
    assert(storage_worker_shutdown());
    assert(!storage_worker_submit(run_snapshot, small, sizeof(small), &rejected));
    assert(allocation_calls == 0 && rejected == UINT64_C(987654));
    assert(storage_worker_init() && storage_worker_init());
    assert(allocated_bytes == 0);
    assert(!storage_worker_submit(NULL, small, sizeof(small), &rejected));
    assert(!storage_worker_submit(run_snapshot, NULL, sizeof(small), &rejected));
    assert(!storage_worker_submit(run_snapshot, small, 0, &rejected));
    assert(!storage_worker_submit(run_snapshot, small, STORAGE_WORKER_MAX_JOB_BYTES + 1U, &rejected));
    assert(!storage_worker_submit(run_snapshot, small, sizeof(small), NULL));
    assert(allocation_calls == 0 && rejected == UINT64_C(987654));

    hold_worker(true);
    assert(storage_worker_submit(run_snapshot, small, sizeof(small), &ids[0]));
    wait_entered();
    fail_allocation = true;
    assert(!storage_worker_submit(run_snapshot, small, sizeof(small), &rejected));
    assert(rejected == UINT64_C(987654) && allocated_bytes == sizeof(small));
    assert(storage_worker_status(ids[0]) == STORAGE_JOB_PENDING);
    fail_allocation = false;
    for (unsigned i = 1; i < STORAGE_WORKER_CAPACITY; ++i) {
        memset(small, (int)i + 1, sizeof(small));
        assert(storage_worker_submit(run_snapshot, small, sizeof(small), &ids[i]));
        assert(ids[i] == ids[0] + i); /* Failed allocation consumed no identity. */
    }
    memset(small, 0xEE, sizeof(small));
    assert(allocated_bytes == STORAGE_WORKER_CAPACITY * sizeof(small));
    before_allocations = allocation_calls;
    assert(!storage_worker_has_capacity());
    assert(!storage_worker_submit(run_snapshot, small, sizeof(small), &rejected));
    assert(allocation_calls == before_allocations && rejected == UINT64_C(987654));
    assert(!storage_worker_release(ids[0]));
    assert(!storage_worker_copy_completed(ids[0], copied, sizeof(copied)));
    assert(!storage_worker_shutdown());
    atomic_store(&fail_run, true);
    hold_worker(false);
    assert(storage_worker_wait(ids[0]) == STORAGE_JOB_FAILED);
    assert(atomic_load(&run_count) == 1);
    assert(!storage_worker_release(ids[0]) && !storage_worker_shutdown());
    assert(!storage_worker_copy_completed(ids[0], copied, sizeof(copied)));
    for (unsigned i = 1; i < STORAGE_WORKER_CAPACITY; ++i)
        assert(storage_worker_status(ids[i]) == STORAGE_JOB_PENDING);
    atomic_store(&fail_run, false);
    assert(storage_worker_retry(ids[0]));
    assert(storage_worker_wait(ids[STORAGE_WORKER_CAPACITY - 1]) == STORAGE_JOB_SUCCEEDED);
    assert(allocation_calls == before_allocations);
    assert(atomic_load(&run_count) == STORAGE_WORKER_CAPACITY + 1U);
    assert(order[0] == 1);
    for (unsigned i = 0; i < STORAGE_WORKER_CAPACITY; ++i) {
        assert(order[i + 1] == i + 1);
        assert(storage_worker_copy_completed(ids[i], copied, sizeof(copied)));
        for (size_t j = 0; j < sizeof(copied); ++j) assert(copied[j] == i + 1);
        assert(!storage_worker_copy_completed(ids[i], copied, sizeof(copied) - 1));
        assert(!storage_worker_retry(ids[i]));
    }
    assert(!storage_worker_has_capacity()); /* Completed, unreleased still owns its slot. */
    for (unsigned i = 0; i < STORAGE_WORKER_CAPACITY; ++i) {
        assert(storage_worker_release(ids[i]));
        assert(!storage_worker_release(ids[i]));
        assert(storage_worker_status(ids[i]) == STORAGE_JOB_UNKNOWN);
    }
    assert(allocated_bytes == 0 && storage_worker_has_capacity());

    unsigned char *maximum = malloc(STORAGE_WORKER_MAX_JOB_BYTES);
    assert(maximum);
    memset(maximum, 9, STORAGE_WORKER_MAX_JOB_BYTES);
    hold_worker(true);
    for (unsigned i = 0; i < STORAGE_WORKER_CAPACITY; ++i)
        assert(storage_worker_submit(run_snapshot, maximum, STORAGE_WORKER_MAX_JOB_BYTES, &ids[i]));
    wait_entered();
    assert(allocated_bytes == STORAGE_WORKER_CAPACITY * STORAGE_WORKER_MAX_JOB_BYTES);
    hold_worker(false);
    assert(storage_worker_wait(ids[STORAGE_WORKER_CAPACITY - 1]) == STORAGE_JOB_SUCCEEDED);
    assert(storage_worker_shutdown()); /* Release completed snapshots even without release(). */
    assert(allocated_bytes == 0);
    for (unsigned i = 0; i < STORAGE_WORKER_CAPACITY; ++i)
        assert(storage_worker_status(ids[i]) == STORAGE_JOB_UNKNOWN);
    assert(storage_worker_init());
    memset(small, 10, sizeof(small));
    assert(storage_worker_submit(run_snapshot, small, sizeof(small), &rejected));
    assert(rejected == ids[STORAGE_WORKER_CAPACITY - 1] + 1);
    assert(storage_worker_wait(rejected) == STORAGE_JOB_SUCCEEDED);
    assert(storage_worker_shutdown() && storage_worker_shutdown());
    assert(allocated_bytes == 0);
    free(maximum);
    printf("PASS: exact-size owned snapshots, OOM/invalid/full rejection, immutable FIFO retry, release/shutdown/restart; job metadata=%zu bytes, maximum payload budget=%u bytes\n",
           sizeof(storage_job_t), STORAGE_WORKER_CAPACITY * STORAGE_WORKER_MAX_JOB_BYTES);
    return 0;
}
