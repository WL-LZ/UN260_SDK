#ifndef STORAGE_WORKER_H
#define STORAGE_WORKER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* One process-wide storage lane. Each accepted job owns an exact-size copy,
 * never borrowed UI pointers. Failed jobs retain their payload and block later
 * jobs until retry. Both job count and per-job allocation are bounded. */
#define STORAGE_WORKER_CAPACITY 4U
/* 100 history records including bounded MULTI groups. Allocation is exact-size,
 * not this maximum; queue capacity remains unchanged. */
#define STORAGE_WORKER_MAX_JOB_BYTES (1536U * 1024U)
typedef uint64_t storage_job_id_t;
typedef enum {
    STORAGE_JOB_UNKNOWN = 0,
    STORAGE_JOB_PENDING,
    STORAGE_JOB_SUCCEEDED,
    STORAGE_JOB_FAILED
} storage_job_status_t;
typedef bool (*storage_job_run_t)(const void *snapshot, size_t size);

bool storage_worker_init(void);
/* Rejecting a full queue, invalid input or allocation failure leaves *id and
 * all previously accepted jobs unchanged. */
bool storage_worker_submit(storage_job_run_t run, const void *snapshot,
                           size_t size, storage_job_id_t *id);
storage_job_status_t storage_worker_status(storage_job_id_t id);
bool storage_worker_retry(storage_job_id_t id);
bool storage_worker_release(storage_job_id_t id);
bool storage_worker_copy_completed(storage_job_id_t id, void *out, size_t size);
bool storage_worker_has_capacity(void);
/* Startup/tests only: never wait in an interactive UI callback. */
storage_job_status_t storage_worker_wait(storage_job_id_t id);
/* Refuses shutdown while any accepted job is unfinished; never drops a job.
 * Successful shutdown releases any completed snapshots not already released. */
bool storage_worker_shutdown(void);

#endif
