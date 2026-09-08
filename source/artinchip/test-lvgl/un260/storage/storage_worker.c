#include "storage_worker.h"

#include <pthread.h>
#include <string.h>

typedef struct {
    storage_job_id_t id;
    storage_job_status_t status;
    bool running;
    storage_job_run_t run;
    size_t size;
    union {
        uint64_t alignment;
        unsigned char bytes[STORAGE_WORKER_MAX_JOB_BYTES];
    } payload;
} storage_job_t;

static storage_job_t g_jobs[STORAGE_WORKER_CAPACITY];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_changed = PTHREAD_COND_INITIALIZER;
static pthread_t g_thread;
static storage_job_id_t g_next_id = 1;
static bool g_started;
static bool g_stopping;

static storage_job_t *find_job(storage_job_id_t id)
{
    unsigned i;
    for (i = 0; i < STORAGE_WORKER_CAPACITY; i++) {
        if (g_jobs[i].status != STORAGE_JOB_UNKNOWN && g_jobs[i].id == id)
            return &g_jobs[i];
    }
    return NULL;
}

static void *storage_thread(void *unused)
{
    (void)unused;
    pthread_mutex_lock(&g_lock);
    for (;;) {
        storage_job_t *job = NULL;
        unsigned i;
        bool succeeded;
        for (i = 0; i < STORAGE_WORKER_CAPACITY; i++) {
            storage_job_t *candidate = &g_jobs[i];
            if ((candidate->status == STORAGE_JOB_PENDING ||
                 candidate->status == STORAGE_JOB_FAILED) &&
                (job == NULL || candidate->id < job->id)) job = candidate;
        }
        if (g_stopping) break;
        if (job == NULL || job->status == STORAGE_JOB_FAILED) {
            pthread_cond_wait(&g_changed, &g_lock);
            continue;
        }
        job->running = true;
        pthread_mutex_unlock(&g_lock);
        /* No UI/LVGL calls, and no service mutex held across filesystem I/O. */
        succeeded = job->run(job->payload.bytes, job->size);
        pthread_mutex_lock(&g_lock);
        job->running = false;
        job->status = succeeded ? STORAGE_JOB_SUCCEEDED : STORAGE_JOB_FAILED;
        pthread_cond_broadcast(&g_changed);
    }
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

bool storage_worker_init(void)
{
    bool ok = true;
    pthread_mutex_lock(&g_lock);
    if (!g_started) {
        g_stopping = false;
        if (pthread_create(&g_thread, NULL, storage_thread, NULL) != 0) ok = false;
        else g_started = true;
    }
    pthread_mutex_unlock(&g_lock);
    return ok;
}

bool storage_worker_submit(storage_job_run_t run, const void *snapshot,
                           size_t size, storage_job_id_t *id)
{
    unsigned i;
    bool accepted = false;
    if (run == NULL || snapshot == NULL || id == NULL || size == 0 ||
        size > STORAGE_WORKER_MAX_JOB_BYTES) return false;
    pthread_mutex_lock(&g_lock);
    if (g_started && !g_stopping && g_next_id != 0) {
        for (i = 0; i < STORAGE_WORKER_CAPACITY; i++) {
            storage_job_t *job = &g_jobs[i];
            if (job->status != STORAGE_JOB_UNKNOWN) continue;
            memcpy(job->payload.bytes, snapshot, size);
            job->size = size;
            job->run = run;
            job->id = g_next_id++;
            job->status = STORAGE_JOB_PENDING;
            *id = job->id;
            accepted = true;
            pthread_cond_broadcast(&g_changed);
            break;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return accepted;
}

storage_job_status_t storage_worker_status(storage_job_id_t id)
{
    storage_job_t *job;
    storage_job_status_t result;
    pthread_mutex_lock(&g_lock);
    job = find_job(id);
    result = job != NULL ? job->status : STORAGE_JOB_UNKNOWN;
    pthread_mutex_unlock(&g_lock);
    return result;
}

storage_job_status_t storage_worker_wait(storage_job_id_t id)
{
    storage_job_t *job;
    storage_job_status_t result;
    pthread_mutex_lock(&g_lock);
    while ((job = find_job(id)) != NULL && job->status == STORAGE_JOB_PENDING)
        pthread_cond_wait(&g_changed, &g_lock);
    result = job != NULL ? job->status : STORAGE_JOB_UNKNOWN;
    pthread_mutex_unlock(&g_lock);
    return result;
}

bool storage_worker_retry(storage_job_id_t id)
{
    storage_job_t *job;
    bool retried = false;
    pthread_mutex_lock(&g_lock);
    job = find_job(id);
    if (job != NULL && job->status == STORAGE_JOB_FAILED) {
        job->status = STORAGE_JOB_PENDING;
        retried = true;
        pthread_cond_broadcast(&g_changed);
    }
    pthread_mutex_unlock(&g_lock);
    return retried;
}

bool storage_worker_release(storage_job_id_t id)
{
    storage_job_t *job;
    bool released = false;
    pthread_mutex_lock(&g_lock);
    job = find_job(id);
    if (job != NULL && job->status == STORAGE_JOB_SUCCEEDED) {
        job->status = STORAGE_JOB_UNKNOWN;
        released = true;
    }
    pthread_mutex_unlock(&g_lock);
    return released;
}

bool storage_worker_copy_completed(storage_job_id_t id, void *out, size_t size)
{
    storage_job_t *job;
    bool copied = false;
    if (out == NULL) return false;
    pthread_mutex_lock(&g_lock);
    job = find_job(id);
    if (job != NULL && job->status == STORAGE_JOB_SUCCEEDED && job->size == size) {
        memcpy(out, job->payload.bytes, size);
        copied = true;
    }
    pthread_mutex_unlock(&g_lock);
    return copied;
}

bool storage_worker_has_capacity(void)
{
    unsigned i;
    bool available = false;
    pthread_mutex_lock(&g_lock);
    if (g_started && !g_stopping) {
        for (i = 0; i < STORAGE_WORKER_CAPACITY; i++)
            if (g_jobs[i].status == STORAGE_JOB_UNKNOWN) available = true;
    }
    pthread_mutex_unlock(&g_lock);
    return available;
}

bool storage_worker_shutdown(void)
{
    unsigned i;
    pthread_mutex_lock(&g_lock);
    if (!g_started) {
        pthread_mutex_unlock(&g_lock);
        return true;
    }
    for (i = 0; i < STORAGE_WORKER_CAPACITY; i++) {
        if (g_jobs[i].status == STORAGE_JOB_PENDING || g_jobs[i].status == STORAGE_JOB_FAILED) {
            pthread_mutex_unlock(&g_lock);
            return false;
        }
    }
    g_stopping = true;
    pthread_cond_broadcast(&g_changed);
    pthread_mutex_unlock(&g_lock);
    pthread_join(g_thread, NULL);
    pthread_mutex_lock(&g_lock);
    g_started = false;
    pthread_mutex_unlock(&g_lock);
    return true;
}
