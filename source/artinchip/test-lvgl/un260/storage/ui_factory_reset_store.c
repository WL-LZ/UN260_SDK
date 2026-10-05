#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "ui_factory_reset_store.h"
#include "storage_worker.h"
#include "workspace_store.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef UI_FACTORY_RESET_DIRECTORY
#define UI_FACTORY_RESET_DIRECTORY "/etc/ui_state"
#endif
#define RESET_JOURNAL "factory_reset.pending"
static const char journal_value[] = "UN260_UI_FACTORY_RESET_V1\n";
/* Preferences only. Records, archives, operator identities and imported
 * photographs never enter this list. Each owner supplies its startup default. */
static const char *const preferences[] = {
    "ui_state.cfg", "password.cfg", "password_visibility.cfg",
    "screenshot.cfg", "screen_recording.cfg", "performance_monitor.cfg",
    "performance_profile.cfg", "gestures.cfg", "touch_feedback.cfg",
    "language.cfg", "brightness.cfg", "standby.cfg", "work_mode.cfg",
    "touch_calibration.cfg"
};
static storage_job_id_t job_id;
static bool result_ok, startup_pending;

static bool write_all(int fd, const void *data, size_t size)
{
    const unsigned char *p = data;
    while (size) {
        ssize_t count = write(fd, p, size);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        p += count; size -= (size_t)count;
    }
    return true;
}

static bool journal_read(int dir, bool *exists)
{
    *exists = false;
    int fd = openat(dir, RESET_JOURNAL, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) return errno == ENOENT;
    *exists = true;
    struct stat info;
    char value[sizeof(journal_value)];
    size_t count = 0;
    bool ok = fstat(fd, &info) == 0 && S_ISREG(info.st_mode) &&
              info.st_size == (off_t)(sizeof(journal_value) - 1);
    while (ok && count < sizeof(journal_value) - 1) {
        ssize_t read_count = read(fd, value + count, sizeof(journal_value) - 1 - count);
        if (read_count < 0 && errno == EINTR) continue;
        if (read_count <= 0) { ok = false; break; }
        count += (size_t)read_count;
    }
    if (close(fd)) ok = false;
    return ok && !memcmp(value, journal_value, sizeof(journal_value) - 1);
}

static bool journal_create(int dir)
{
    bool exists;
    if (!journal_read(dir, &exists)) return false;
    if (exists) {
        int fd = openat(dir, RESET_JOURNAL, O_RDONLY | O_NOFOLLOW);
        if (fd < 0) return false;
        bool ok = fsync(fd) == 0;
        if (close(fd)) ok = false;
        return ok && fsync(dir) == 0;
    }
    int fd = openat(dir, RESET_JOURNAL ".tmp",
                    O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
    if (fd < 0) return false;
    bool ok = write_all(fd, journal_value, sizeof(journal_value) - 1);
    if (ok && fsync(fd)) ok = false;
    if (close(fd)) ok = false;
    if (ok && renameat(dir, RESET_JOURNAL ".tmp", dir, RESET_JOURNAL)) ok = false;
    if (!ok) (void)unlinkat(dir, RESET_JOURNAL ".tmp", 0);
    /* No preference is removed until both the file and its directory are
     * durable. An interrupted deletion is replayable and idempotent. */
    if (ok && fsync(dir)) ok = false;
    return ok;
}

static bool remove_file(int dir, const char *name)
{
    if (unlinkat(dir, name, 0) == 0) return true;
    return errno == ENOENT;
}

static bool restore_preferences(int dir)
{
    /* Reuse the workspace codec/atomic writer while preserving operator IDs,
     * their monotonic allocator and identity used by business records. */
    if (!workspace_store_factory_reset_preferences()) return false;
    for (unsigned i = 0; i < sizeof(preferences) / sizeof(preferences[0]); ++i) {
        char temporary[80];
        if (!remove_file(dir, preferences[i])) return false;
        int count = snprintf(temporary, sizeof(temporary), "%s.tmp", preferences[i]);
        if (count < 0 || count >= (int)sizeof(temporary) ||
            !remove_file(dir, temporary)) return false;
    }
    return fsync(dir) == 0;
}

bool ui_factory_reset_store_resume(void)
{
    startup_pending = false;
    int dir = open(UI_FACTORY_RESET_DIRECTORY, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (dir < 0) return errno == ENOENT;
    bool exists;
    bool ok = journal_read(dir, &exists);
    if (ok && exists) {
        ok = restore_preferences(dir);
        if (ok) startup_pending = true;
    }
    if (close(dir)) ok = false;
    return ok;
}

bool ui_factory_reset_store_startup_pending(void) { return startup_pending; }
bool ui_factory_reset_store_finish_startup(void)
{
    if (!startup_pending) return true;
    int dir = open(UI_FACTORY_RESET_DIRECTORY, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (dir < 0) return false;
    bool ok = remove_file(dir, RESET_JOURNAL) && fsync(dir) == 0;
    if (close(dir)) ok = false;
    if (ok) startup_pending = false;
    return ok;
}

static bool run_reset(const void *snapshot, size_t size)
{
    result_ok = false;
    if (size != sizeof(unsigned char) || *(const unsigned char *)snapshot != 1)
        return true;
    if (mkdir(UI_FACTORY_RESET_DIRECTORY, 0755) && errno != EEXIST) return true;
    int dir = open(UI_FACTORY_RESET_DIRECTORY, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (dir < 0) return true;
    result_ok = journal_create(dir) && restore_preferences(dir);
    if (close(dir)) result_ok = false;
    /* Keep the journal until the next startup. If an old cache writes before
     * shutdown, startup replays defaults before enabling its consumers. */
    return true;
}

bool ui_factory_reset_store_begin(void)
{
    const unsigned char acknowledged = 1;
    if (job_id || !storage_worker_init()) return false;
    return storage_worker_submit(run_reset, &acknowledged, sizeof(acknowledged), &job_id);
}

bool ui_factory_reset_store_poll(bool *success)
{
    if (!job_id || storage_worker_status(job_id) != STORAGE_JOB_SUCCEEDED) return false;
    if (success) *success = result_ok;
    (void)storage_worker_release(job_id);
    job_id = 0;
    return true;
}
