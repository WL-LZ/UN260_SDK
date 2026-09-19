#include "work_mode_store.h"
#include "storage_worker.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef WORK_MODE_STORE_DIRECTORY
#define WORK_MODE_STORE_DIRECTORY "/etc/ui_state"
#endif
#define STORE_PATH WORK_MODE_STORE_DIRECTORY "/work_mode.cfg"
#define TEMP_PATH STORE_PATH ".tmp"
#define RECORD_BYTES 8U

typedef struct {
    work_mode_record_t record;
    bool load;
} store_job_t;

/* Worker result is published only after storage_worker_status acquires its
 * completion mutex. One owned job at a time; no borrowed UI pointers. */
static storage_job_id_t job_id;
static store_job_t result;
static bool result_ok;

static bool valid(const work_mode_record_t *record)
{
    return record && record->preferred <= 1 &&
           (!record->restore_pending || record->preferred_valid);
}

static bool read_record(work_mode_record_t *record)
{
    unsigned char bytes[RECORD_BYTES];
    memset(record, 0, sizeof(*record));
    FILE *file = fopen(STORE_PATH, "rb");
    if (!file) return errno == ENOENT;
    size_t count = fread(bytes, 1, sizeof(bytes), file);
    bool ok = count == sizeof(bytes) && fgetc(file) == EOF && !ferror(file);
    if (fclose(file)) ok = false;
    if (!ok || memcmp(bytes, "WKM1", 4) || bytes[4] > 1 ||
        bytes[5] > 1 || bytes[6] > 1 ||
        bytes[7] != (unsigned char)(0xA5U ^ bytes[4] ^ bytes[5] ^ bytes[6])) return false;
    *record = (work_mode_record_t){bytes[4] != 0, bytes[5], bytes[6] != 0};
    return valid(record);
}

static bool write_record(const work_mode_record_t *record)
{
    unsigned char bytes[RECORD_BYTES] = {'W', 'K', 'M', '1',
        record->preferred_valid, record->preferred, record->restore_pending, 0};
    bytes[7] = (unsigned char)(0xA5U ^ bytes[4] ^ bytes[5] ^ bytes[6]);
    if (mkdir(WORK_MODE_STORE_DIRECTORY, 0755) && errno != EEXIST) return false;
    int fd = open(TEMP_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
    if (fd < 0) return false;
    size_t offset = 0;
    bool ok = true;
    while (offset < sizeof(bytes)) {
        ssize_t count = write(fd, bytes + offset, sizeof(bytes) - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = false; break; }
        offset += (size_t)count;
    }
    if (ok && fsync(fd)) ok = false;
    if (close(fd)) ok = false;
    if (ok && rename(TEMP_PATH, STORE_PATH)) ok = false;
    if (ok) {
        int dir = open(WORK_MODE_STORE_DIRECTORY, O_RDONLY | O_DIRECTORY);
        if (dir < 0) ok = false;
        else {
            if (fsync(dir)) ok = false;
            if (close(dir)) ok = false;
        }
    }
    if (!ok) unlink(TEMP_PATH);
    return ok;
}

static bool run_job(const void *snapshot, size_t size)
{
    if (size != sizeof(store_job_t)) return true;
    result = *(const store_job_t *)snapshot;
    result_ok = result.load ? read_record(&result.record) : write_record(&result.record);
    /* Report filesystem failure separately, so this setting cannot pin the
     * shared FIFO and prevent unrelated history from being saved. */
    return true;
}

static bool submit(const store_job_t *job)
{
    if (job_id || !storage_worker_init()) return false;
    return storage_worker_submit(run_job, job, sizeof(*job), &job_id);
}

bool work_mode_store_begin_load(void)
{
    const store_job_t job = {.load = true};
    return submit(&job);
}

bool work_mode_store_begin_save(const work_mode_record_t *record)
{
    if (!valid(record)) return false;
    const store_job_t job = {.record = *record, .load = false};
    return submit(&job);
}

bool work_mode_store_busy(void) { return job_id != 0; }

bool work_mode_store_poll(work_mode_record_t *record, bool *was_load, bool *ok)
{
    if (!job_id || storage_worker_status(job_id) != STORAGE_JOB_SUCCEEDED) return false;
    if (record) *record = result.record;
    if (was_load) *was_load = result.load;
    if (ok) *ok = result_ok;
    (void)storage_worker_release(job_id);
    job_id = 0;
    return true;
}
