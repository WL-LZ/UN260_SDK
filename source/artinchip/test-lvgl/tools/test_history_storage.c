#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "un260/storage/storage_worker.h"

enum { FAIL_NONE, FAIL_OPEN, FAIL_FLUSH, FAIL_FILE_SYNC, FAIL_CLOSE, FAIL_RENAME, FAIL_DIR_SYNC };
static atomic_int fault;
static int crash_stage;
static pthread_t ui_thread;
static pthread_mutex_t gate_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_changed = PTHREAD_COND_INITIALIZER;
static bool held;
static bool index_fd[4096];
static bool store_dir_fd[4096];
static unsigned index_renames;
static unsigned slot_renames;
static unsigned meta_renames;
static bool fail_worker_init;
static unsigned worker_init_calls;

static bool history_test_worker_init(void)
{
    worker_init_calls++;
    return !fail_worker_init && storage_worker_init();
}

static void hold_worker(bool hold)
{
    pthread_mutex_lock(&gate_lock);
    held = hold;
    pthread_cond_broadcast(&gate_changed);
    pthread_mutex_unlock(&gate_lock);
}

static int history_test_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    int fd;
    bool index = strstr(path, "/index.cfg.tmp") != NULL;
    assert(!pthread_equal(pthread_self(), ui_thread));
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = (mode_t)va_arg(args, int);
        va_end(args);
    }
    if (index) {
        pthread_mutex_lock(&gate_lock);
        while (held) pthread_cond_wait(&gate_changed, &gate_lock);
        pthread_mutex_unlock(&gate_lock);
        if (atomic_load(&fault) == FAIL_OPEN) { errno = EIO; return -1; }
    }
    fd = open(path, flags, mode);
    if (fd >= 0) {
        assert(fd < 4096);
        index_fd[fd] = index;
        store_dir_fd[fd] = strcmp(path, UI_HISTORY_STORE_DIR) == 0;
    }
    return fd;
}

static FILE *history_test_fopen(const char *path, const char *mode)
{
    assert(!pthread_equal(pthread_self(), ui_thread));
    return fopen(path, mode);
}

static int history_test_fflush(FILE *fp)
{
    if (index_fd[fileno(fp)] && atomic_load(&fault) == FAIL_FLUSH) {
        errno = EIO; return EOF;
    }
    return fflush(fp);
}

static int history_test_fclose(FILE *fp)
{
    int fd = fileno(fp);
    bool fail = index_fd[fd] && atomic_load(&fault) == FAIL_CLOSE;
    int result = fclose(fp);
    index_fd[fd] = false;
    if (fail) { errno = EIO; return EOF; }
    return result;
}

static int history_test_fsync(int fd)
{
    assert(!pthread_equal(pthread_self(), ui_thread));
    if (index_fd[fd] && crash_stage == 1) _exit(51);
    if ((index_fd[fd] && atomic_load(&fault) == FAIL_FILE_SYNC) ||
        (store_dir_fd[fd] && atomic_load(&fault) == FAIL_DIR_SYNC)) {
        errno = EIO; return -1;
    }
    return fsync(fd);
}

static int history_test_rename(const char *from, const char *to)
{
    bool index = strstr(to, "/index.cfg") != NULL;
    int result;
    assert(!pthread_equal(pthread_self(), ui_thread));
    if (index && crash_stage == 2) _exit(52);
    if (index && atomic_load(&fault) == FAIL_RENAME) { errno = EIO; return -1; }
    result = rename(from, to);
    if (result == 0) {
        if (index) index_renames++;
        else if (strstr(to, ".rec")) slot_renames++;
        else if (strstr(to, "meta.cfg")) meta_renames++;
    }
    if (index && crash_stage == 3) _exit(53);
    return result;
}

#define open history_test_open
#define fopen history_test_fopen
#define fflush history_test_fflush
#define fclose history_test_fclose
#define fsync history_test_fsync
#define rename history_test_rename
#define storage_worker_init history_test_worker_init
#include "un260/lv_system/ui_history_data_fs.c"
#undef open
#undef fopen
#undef fflush
#undef fclose
#undef fsync
#undef rename
#undef storage_worker_init
#include "un260/counting/counting_history_service.h"

static const counting_sim_t *runtime_sim;
static unsigned reset_animation_calls;
static const counting_sim_t *counting_data_current(void) { return runtime_sim; }
static uint32_t lv_tick_get(void) { return 0; }
static void ui_count_end_anim_cancel(void) { reset_animation_calls++; }
static void uart_debug_printf(const char *format, ...) { (void)format; }
#include "history_runtime_under_test.h"

void currency_state_get_active_code(char code[4]) { memcpy(code, "USD", 4); }
void machine_time_get(machine_time_value_t *value)
{
    *value = (machine_time_value_t){ 2026, 9, 8, 12, 30, 20 };
}
bool machine_time_is_valid(const machine_time_value_t *value)
{
    return value->year >= 2000 && value->month >= 1 && value->month <= 12 &&
           value->day >= 1 && value->day <= 31 && value->hour < 24 &&
           value->minute < 60 && value->second < 60;
}

static storage_job_id_t append(unsigned pcs)
{
    counting_sim_t sim = { 0 };
    char serial[] = "IMMUTABLE123";
    char *serials[] = { serial };
    sim.sn_capacity = 1;
    sim.sn_str = serials;
    sim.denom_mix[0] = 10;
    sim.denom_number = 1;
    sim.denom[0] = (denom_t){ 10, 1, 10.0f };
    assert(ui_history_record_append_from_session(&sim, pcs, pcs * 10.0f,
        ui_history_total_notes_counted_get() + pcs, "error", "start", "end", "log"));
    memset(serial, 'X', strlen(serial));
    return ui_history_last_commit_id();
}

static void wait_saved(storage_job_id_t id, uint32_t now)
{
    assert(storage_worker_wait(id) == STORAGE_JOB_SUCCEEDED);
    (void)ui_history_data_poll(now);
    assert(ui_history_commit_status(id) == STORAGE_JOB_SUCCEEDED);
}

static void exercise(void)
{
    storage_job_id_t id;
    unsigned before_slots, before_meta, before_index;
    unsigned i;
    uint32_t now = 0;
    counting_session_state_t session = { 0 };
    counting_sim_t sim = { 0 };
    runtime_sim = &sim;
    hold_worker(true);
    id = append(1);
    assert(storage_worker_status(id) == STORAGE_JOB_PENDING);
    assert(!storage_worker_shutdown());
    hold_worker(false);
    wait_saved(id, now);
    assert(strcmp(ui_history_data_get()->records[0].sn_text, "IMMUTABLE123") == 0);
    before_slots = slot_renames;
    wait_saved(append(2), now);
    assert(slot_renames == before_slots + 1); /* Old slot untouched. */
    before_slots = slot_renames;
    before_meta = meta_renames;
    assert(ui_history_record_set_selected(0, true));
    wait_saved(ui_history_last_commit_id(), now);
    assert(slot_renames == before_slots + 1 && meta_renames == before_meta);
    assert(ui_history_record_set_selected(0, false));
    wait_saved(ui_history_last_commit_id(), now);

    for (i = FAIL_OPEN; i <= FAIL_DIR_SYNC; i++) {
        uint8_t old_count = ui_history_data_get()->record_count;
        atomic_store(&fault, (int)i);
        id = append(1);
        assert(storage_worker_wait(id) == STORAGE_JOB_FAILED);
        assert(ui_history_commit_status(id) == STORAGE_JOB_FAILED);
        (void)ui_history_data_poll(now); /* Not yet the retry deadline. */
        assert(ui_history_data_get()->record_count == old_count);
        assert(!ui_history_data_can_accept() && !counting_history_can_start());
        assert(!storage_worker_release(id) && !storage_worker_shutdown());
        before_index = index_renames;
        atomic_store(&fault, FAIL_NONE);
        now += 1000;
        (void)ui_history_data_poll(now);
        wait_saved(id, now);
        assert(ui_history_data_get()->record_count == old_count + 1);
        if (i == FAIL_DIR_SYNC) assert(index_renames == before_index);
    }

    assert(ui_history_record_set_selected(0, true));
    wait_saved(ui_history_last_commit_id(), now);
    {
        uint8_t old_count = ui_history_data_get()->record_count;
        atomic_store(&fault, FAIL_RENAME);
        assert(ui_history_record_delete_selected());
        id = ui_history_last_commit_id();
        assert(storage_worker_wait(id) == STORAGE_JOB_FAILED);
        (void)ui_history_data_poll(now);
        assert(ui_history_data_get()->record_count == old_count);
        atomic_store(&fault, FAIL_NONE);
        now += 1000;
        (void)ui_history_data_poll(now);
        wait_saved(id, now);
        assert(ui_history_data_get()->record_count == old_count - 1);
    }

    hold_worker(true);
    for (i = 0; i < STORAGE_WORKER_CAPACITY; i++) id = append(1);
    {
        ui_history_record_t rec = ui_history_data_get()->records[0];
        uint32_t before = ui_history_total_notes_counted_get();
        assert(!ui_history_record_append_snapshot(&rec, before + 1));
        assert(ui_history_total_notes_counted_get() == before);
    }
    hold_worker(false);
    wait_saved(id, now);

    /* Count snapshots outlive new starts, input resets and queue saturation. */
    hold_worker(true);
    for (i = 0; i < 8; i++) {
        session.history_record.valid = true;
        session.history_record.end_seen = true;
        session.history_record.pcs = 1;
        session.history_record.amount = 10;
        assert(counting_history_try_commit(&session, &sim, now) == COUNTING_HISTORY_COMMIT_PENDING);
        assert(!session.history_record.valid);
        counting_history_session_start(NULL, 0);
        assert(!counting_history_discard_pending(&session));
    }
    session.history_record.valid = true;
    session.history_record.end_seen = true;
    session.history_record.pcs = 1;
    assert(counting_history_try_commit(&session, &sim, now) == COUNTING_HISTORY_COMMIT_PENDING);
    assert(!session.history_record.valid); /* Ninth count has an immutable emergency slot. */
    session.history_record.valid = true;
    session.history_record.end_seen = false;
    session.history_record.pcs = 1;
    session.phase = COUNTING_SESSION_FINISHED_WAIT_START;
    session.analysis_valid_pcs = 27;
    {
        counting_session_state_t before = session;
        assert(!app_counting_runtime_reset_session(&session, "full-spool reset test"));
        assert(memcmp(&before, &session, sizeof(session)) == 0);
        assert(reset_animation_calls == 0);
        assert(!counting_history_prepare_start(&session, &sim, now));
        assert(memcmp(&before, &session, sizeof(session)) == 0);
    }
    assert(!counting_history_can_start());
    hold_worker(false);
    for (i = 0; i < 10000; i++) {
        (void)ui_history_data_poll(++now);
        (void)counting_history_poll_commit(&session, &sim, now);
        /* Model the retained start frame retrying before later frames. */
        if (session.history_record.valid)
            (void)counting_history_prepare_start(&session, &sim, now);
        if (!session.history_record.valid && counting_history_can_start() &&
            ui_history_data_status() == STORAGE_JOB_SUCCEEDED) break;
        usleep(1000);
    }
    assert(i < 10000 && !session.history_record.valid);
    assert(ui_history_total_notes_counted_get() == 23);
    assert(ui_history_data_get()->record_count == 20);
    assert(app_counting_runtime_reset_session(&session, "safe reset"));
    assert(reset_animation_calls == 1);
    assert(storage_worker_shutdown());
    puts("PASS: worker-only I/O, immutable bounded jobs, durable completion, failure rollback/retry, incremental mirrors, counting backpressure");
}

static void test_total_arithmetic(void)
{
    counting_session_state_t session = { 0 };
    counting_sim_t sim = { 0 };
    ui_history_total_notes_counted_set(100);
    wait_saved(ui_history_last_commit_id(), 0);
    session.history_record.valid = true;
    session.history_record.end_seen = true;
    session.history_record.pcs = 3;
    session.history_record.total_after = 999; /* Stale session absolute total is not added. */
    assert(counting_history_try_commit(&session, &sim, 0) == COUNTING_HISTORY_COMMIT_PENDING);
    wait_saved(ui_history_last_commit_id(), 0);
    assert(counting_history_poll_commit(&session, &sim, 0) == COUNTING_HISTORY_COMMIT_SAVED);
    assert(ui_history_total_notes_counted_get() == 103);
    assert(counting_history_poll_commit(&session, &sim, 0) == COUNTING_HISTORY_COMMIT_NOT_READY);
    assert(ui_history_total_notes_counted_get() == 103);
    ui_history_total_notes_counted_clear();
    wait_saved(ui_history_last_commit_id(), 0);
    session.history_record.valid = true;
    session.history_record.end_seen = true;
    session.history_record.pcs = 4;
    assert(counting_history_try_commit(&session, &sim, 0) == COUNTING_HISTORY_COMMIT_PENDING);
    wait_saved(ui_history_last_commit_id(), 0);
    assert(counting_history_poll_commit(&session, &sim, 0) == COUNTING_HISTORY_COMMIT_SAVED);
    assert(ui_history_total_notes_counted_get() == 4);
    assert(storage_worker_shutdown());
    puts("PASS: absolute total set/clear and exactly-once count increments");
}

int main(int argc, char **argv)
{
    ui_thread = pthread_self();
    assert(argc >= 2);
    fail_worker_init = strcmp(argv[1], "init-failure") == 0;
    ui_history_data_init();
    if (fail_worker_init) {
        unsigned i;
        fail_worker_init = false;
        for (i = 0; i < 20; i++) {
            (void)ui_history_data_get();
            (void)ui_history_total_notes_counted_get();
            ui_history_total_notes_counted_set(123);
            ui_history_data_init();
        }
        assert(worker_init_calls == 1);
        assert(ui_history_data_status() == STORAGE_JOB_FAILED);
        assert(!ui_history_data_can_accept() && !counting_history_can_start());
        puts("PASS: failed boot load never retries/waits in getters or overwrites history");
    } else if (strcmp(argv[1], "exercise") == 0) exercise();
    else if (strcmp(argv[1], "total-arithmetic") == 0) test_total_arithmetic();
    else if (strcmp(argv[1], "dump") == 0) {
        const ui_history_store_t *store = ui_history_data_get();
        printf("%u %u %u\n", store->record_count, store->total_notes_counted, store->next_record_no);
        assert(storage_worker_shutdown());
    } else if (strcmp(argv[1], "crash") == 0) {
        assert(argc == 3);
        crash_stage = atoi(argv[2]);
        (void)storage_worker_wait(append(1));
        assert(!"Crash injection was not reached");
    } else assert(!"Unknown test command");
    return 0;
}
