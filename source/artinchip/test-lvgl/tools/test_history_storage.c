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
static int load_open_error;
static bool fail_load_read;
static FILE *load_file;
static unsigned unrelated_job_runs;

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
    if (load_open_error != 0) {
        errno = load_open_error;
        return NULL;
    }
    load_file = fopen(path, mode);
    return load_file;
}

static int history_test_ferror(FILE *fp)
{
    if (fail_load_read && fp == load_file) {
        errno = EIO;
        return 1;
    }
    return ferror(fp);
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
#define ferror history_test_ferror
#define fflush history_test_fflush
#define fclose history_test_fclose
#define fsync history_test_fsync
#define rename history_test_rename
#define storage_worker_init history_test_worker_init
#include "un260/lv_system/ui_history_data_fs.c"
#undef open
#undef fopen
#undef ferror
#undef fflush
#undef fclose
#undef fsync
#undef rename
#undef storage_worker_init
#include "un260/counting/counting_history_service.h"

static const counting_sim_t *runtime_sim;
static unsigned reset_animation_calls;
static unsigned reset_island_calls;
static const counting_sim_t *history_runtime_test_current(void) { return runtime_sim; }
static uint32_t lv_tick_get(void) { return 0; }
static void ui_count_end_anim_cancel(void) { reset_animation_calls++; }
static void smart_island_notify_count_reset(void) { reset_island_calls++; }
static void uart_debug_printf(const char *format, ...) { (void)format; }
#define counting_data_current history_runtime_test_current
#include "history_runtime_under_test.h"
#undef counting_data_current

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
        assert(reset_island_calls == 0);
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
    {
        ui_history_record_t untouched, before;
        storage_job_id_t old_job = ui_history_last_commit_id();
        memset(&untouched, 0xA5, sizeof(untouched));
        before = untouched;
        counting_data_mark_multi_result(&sim);
        assert(!ui_history_record_build_from_session(&sim, 12, 9999, "", "", "", "", &untouched));
        assert(memcmp(&before, &untouched, sizeof(before)) == 0);
        session.history_record.valid = true;
        session.history_record.end_seen = true;
        session.history_record.pcs = 12;
        session.history_record.amount = 9999;
        assert(counting_history_try_commit(&session, &sim, now) == COUNTING_HISTORY_COMMIT_UNSUPPORTED);
        assert(!session.history_record.valid && counting_history_take_unsupported_notice());
        assert(!counting_history_take_unsupported_notice());
        assert(ui_history_last_commit_id() == old_job);
        assert(ui_history_total_notes_counted_get() == 23 && ui_history_data_get()->record_count == 20);
        assert(counting_history_can_start());
        counting_data_reset_result_scope(&sim);
    }
    assert(app_counting_runtime_reset_session(&session, "safe reset"));
    assert(reset_animation_calls == 1);
    assert(reset_island_calls == 1);
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

static void delete_all_saved(uint32_t now)
{
    if (ui_history_data_get()->record_count == 0) return;
    ui_history_record_set_all_selected(true);
    wait_saved(ui_history_last_commit_id(), now);
    assert(ui_history_record_delete_selected());
    wait_saved(ui_history_last_commit_id(), now);
}

static void test_retention(void)
{
    uint32_t ids[UI_HISTORY_MAX_RECORDS];
    uint8_t slots[UI_HISTORY_MAX_RECORDS];
    ui_history_record_t record;
    const ui_history_store_t *store = ui_history_data_get();
    uint32_t next_id = store->next_record_no;
    uint32_t total = store->total_notes_counted;
    storage_job_id_t job;
    unsigned i;

    delete_all_saved(0);
    assert(store->next_record_no == next_id);
    assert(store->total_notes_counted == total);
    for (i = 0; i < UI_HISTORY_MAX_RECORDS; i++) wait_saved(append(1), 0);
    for (i = 0; i < UI_HISTORY_MAX_RECORDS; i++) {
        ids[i] = store->records[i].record_no;
        slots[i] = store->records[i].slot_no;
    }
    next_id = store->next_record_no;

    /* Remove the newest record and a middle record. A failed deletion must
     * roll back until its original immutable job succeeds. */
    assert(ui_history_record_set_selected(0, true));
    wait_saved(ui_history_last_commit_id(), 0);
    assert(ui_history_record_set_selected(7, true));
    wait_saved(ui_history_last_commit_id(), 0);
    atomic_store(&fault, FAIL_RENAME);
    assert(ui_history_record_delete_selected());
    job = ui_history_last_commit_id();
    assert(storage_worker_wait(job) == STORAGE_JOB_FAILED);
    (void)ui_history_data_poll(0);
    assert(store->record_count == UI_HISTORY_MAX_RECORDS);
    assert(ui_history_record_get_by_no(ids[0], &record));
    assert(store->next_record_no == next_id);
    atomic_store(&fault, FAIL_NONE);
    (void)ui_history_data_poll(1000);
    wait_saved(job, 1000);
    assert(store->record_count == UI_HISTORY_MAX_RECORDS - 2);
    assert(store->next_record_no == next_id);
    assert(!ui_history_record_get_by_no(ids[0], &record));
    assert(!ui_history_record_get_by_no(ids[7], &record));
    for (i = 1; i < UI_HISTORY_MAX_RECORDS; i++) {
        if (i == 7) continue;
        assert(ui_history_record_get_by_no(ids[i], &record));
        assert(record.slot_no == slots[i]);
    }

    /* Fill holes first, regardless of where the persisted ring cursor points. */
    wait_saved(append(1), 1000);
    assert(store->records[0].record_no == next_id);
    assert(store->records[0].slot_no == slots[0] || store->records[0].slot_no == slots[7]);
    wait_saved(append(1), 1000);
    assert(store->record_count == UI_HISTORY_MAX_RECORDS);
    assert(store->records[0].record_no == next_id + 1);
    assert(store->records[0].slot_no == slots[0] || store->records[0].slot_no == slots[7]);
    assert(store->records[0].slot_no != store->records[1].slot_no);
    for (i = 1; i < UI_HISTORY_MAX_RECORDS; i++) {
        if (i == 7) continue;
        assert(ui_history_record_get_by_no(ids[i], &record));
    }

    wait_saved(append(1), 1000);
    assert(!ui_history_record_get_by_no(ids[UI_HISTORY_MAX_RECORDS - 1], &record));
    assert(store->records[0].slot_no == slots[UI_HISTORY_MAX_RECORDS - 1]);
    assert(ui_history_record_get_by_no(next_id, &record));
    assert(ui_history_record_get_by_no(next_id + 1, &record));
    assert(store->next_record_no == next_id + 3);
    assert(storage_worker_shutdown());
    puts("PASS: stable IDs/slots, failed-delete recovery, vacant-slot reuse, oldest-only eviction");
}

static bool unrelated_storage_job(const void *snapshot, size_t size)
{
    assert(size == 1 && *(const char *)snapshot == 'X');
    unrelated_job_runs++;
    return true;
}

static void test_delete_records(void)
{
    uint32_t ids[UI_HISTORY_MAX_RECORDS];
    const ui_history_store_t *store = ui_history_data_get();
    ui_history_store_t *before = malloc(sizeof(*before));
    storage_job_id_t job, previous_job;
    unsigned previous_renames;
    assert(before);
    delete_all_saved(0);
    for (unsigned i = 0; i < UI_HISTORY_MAX_RECORDS; ++i) wait_saved(append(1), 0);
    assert(ui_history_record_set_selected(1, true));
    wait_saved(ui_history_last_commit_id(), 0);
    assert(ui_history_record_set_selected(3, true));
    wait_saved(ui_history_last_commit_id(), 0);
    for (unsigned i = 0; i < 7; ++i) ids[i] = store->records[i * 2].record_no;

    hold_worker(true);
    for (unsigned i = 0; i < STORAGE_WORKER_CAPACITY; ++i)
        assert(ui_history_record_toggle_selected((uint8_t)(13 + i)));
    *before = *store;
    previous_job = ui_history_last_commit_id();
    assert(!ui_history_record_delete_records(ids, 7));
    assert(memcmp(before, store, sizeof(*before)) == 0);
    assert(ui_history_last_commit_id() == previous_job);
    hold_worker(false); wait_saved(previous_job, 0);

    *before = *store;
    previous_job = ui_history_last_commit_id();
    uint32_t duplicate[] = { ids[0], ids[0] };
    uint32_t missing[] = { ids[0], store->next_record_no };
    uint32_t zero[] = { ids[0], 0 };
#define EXPECT_DELETE_REJECTED(values, count) do { \
    assert(!ui_history_record_delete_records(values, count)); \
    assert(memcmp(before, store, sizeof(*before)) == 0); \
    assert(ui_history_last_commit_id() == previous_job); \
} while (0)
    EXPECT_DELETE_REJECTED(NULL, 1);
    EXPECT_DELETE_REJECTED(ids, 0);
    EXPECT_DELETE_REJECTED(ids, UI_HISTORY_MAX_RECORDS + 1);
    EXPECT_DELETE_REJECTED(duplicate, 2);
    EXPECT_DELETE_REJECTED(missing, 2);
    EXPECT_DELETE_REJECTED(zero, 2);
#undef EXPECT_DELETE_REJECTED

    /* Seven IDs exceed the four-job queue if implemented as temporary global
     * selection changes. The batch must instead own exactly one snapshot. */
    previous_renames = index_renames;
    hold_worker(true); atomic_store(&fault, FAIL_RENAME);
    assert(ui_history_record_delete_records(ids, 7));
    job = ui_history_last_commit_id();
    assert(job == previous_job + 1 && g_history_job_count == 1);
    assert(store->record_count == UI_HISTORY_MAX_RECORDS - 7);
    hold_worker(false);
    assert(storage_worker_wait(job) == STORAGE_JOB_FAILED);
    (void)ui_history_data_poll(0);
    assert(memcmp(before, store, sizeof(*before)) == 0);
    assert(!ui_history_record_delete_records(ids, 7));
    assert(memcmp(before, store, sizeof(*before)) == 0);
    assert(index_renames == previous_renames);
    atomic_store(&fault, FAIL_NONE);
    (void)ui_history_data_poll(1000); wait_saved(job, 1000);
    assert(g_history_job_count == 0 && index_renames == previous_renames + 1);
    assert(store->record_count == UI_HISTORY_MAX_RECORDS - 7);
    assert(store->next_record_no == before->next_record_no);
    assert(store->total_notes_counted == before->total_notes_counted);
    for (unsigned i = 0; i < UI_HISTORY_MAX_RECORDS; ++i) {
        ui_history_record_t record;
        bool removed = i <= 12 && i % 2 == 0;
        assert(ui_history_record_get_by_no(before->records[i].record_no, &record) != removed);
        if (!removed) assert(memcmp(&record, &before->records[i], sizeof(record)) == 0);
    }
    size_t remaining = store->record_count;
    for (size_t i = 0; i < remaining; ++i) ids[i] = store->records[i].record_no;
    assert(ui_history_record_delete_records(ids, remaining));
    wait_saved(ui_history_last_commit_id(), 1000);
    assert(store->record_count == 0 && store->next_record_no == before->next_record_no);
    free(before);
    assert(storage_worker_shutdown());
    puts("PASS: atomic stable-ID batch deletion, strict validation, one queued snapshot and failure rollback");
}

static void test_unavailable_history(void)
{
    counting_sim_t sim = { 0 };
    ui_history_record_t record;
    storage_job_id_t job;
    const char payload = 'X';
    const uint32_t record_no = 1;
    unsigned i;

    assert(!ui_history_data_is_available());
    assert(ui_history_data_status() == STORAGE_JOB_FAILED);
    assert(!ui_history_data_can_accept() && !counting_history_can_start());
    assert(ui_history_record_build_from_session(&sim, 1, 10, "", "", "", "", &record));
    for (i = 0; i < 3; i++) {
        ui_history_data_init();
        assert(!ui_history_record_append_snapshot(&record, 1));
        assert(!ui_history_record_append_from_session(&sim, 1, 10, 1, "", "", "", ""));
        ui_history_total_notes_counted_set(123);
        ui_history_total_notes_counted_clear();
        assert(!ui_history_record_set_selected(0, true));
        assert(!ui_history_record_toggle_selected(0));
        ui_history_record_set_all_selected(true);
        ui_history_record_clear_selected();
        assert(!ui_history_record_delete_selected());
        assert(!ui_history_record_delete_records(&record_no, 1));
        (void)ui_history_data_poll(i * 1000);
        assert(ui_history_data_get()->record_count == 0);
        assert(ui_history_total_notes_counted_get() == 0);
    }
    assert(worker_init_calls == 1 && ui_history_last_commit_id() == 0);
    assert(index_renames == 0 && slot_renames == 0 && meta_renames == 0);
    /* An unreadable history file does not leave a failed job owning the shared
     * worker: unrelated settings/storage work must still be able to finish. */
    assert(storage_worker_submit(unrelated_storage_job, &payload, sizeof(payload), &job));
    assert(storage_worker_wait(job) == STORAGE_JOB_SUCCEEDED);
    assert(storage_worker_release(job));
    assert(unrelated_job_runs == 1);
    assert(storage_worker_shutdown());
    puts("PASS: unreadable history remains protected; unrelated storage lane stays usable");
}

int main(int argc, char **argv)
{
    ui_thread = pthread_self();
    assert(argc >= 2);
    fail_worker_init = strcmp(argv[1], "init-failure") == 0;
    if (strcmp(argv[1], "load-eio") == 0) load_open_error = EIO;
    if (strcmp(argv[1], "load-eacces") == 0) load_open_error = EACCES;
    fail_load_read = strcmp(argv[1], "load-read-error") == 0;
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
    } else if (strncmp(argv[1], "load-", 5) == 0) test_unavailable_history();
    else if (strcmp(argv[1], "exercise") == 0) exercise();
    else if (strcmp(argv[1], "total-arithmetic") == 0) test_total_arithmetic();
    else if (strcmp(argv[1], "retention") == 0) test_retention();
    else if (strcmp(argv[1], "delete-records") == 0) test_delete_records();
    else if (strcmp(argv[1], "delete-all") == 0) {
        uint32_t next_id = ui_history_data_get()->next_record_no;
        uint32_t total = ui_history_total_notes_counted_get();
        delete_all_saved(0);
        assert(ui_history_data_get()->record_count == 0);
        assert(ui_history_data_get()->next_record_no == next_id);
        assert(ui_history_total_notes_counted_get() == total);
        assert(storage_worker_shutdown());
    } else if (strcmp(argv[1], "append-one") == 0) {
        uint32_t next_id = ui_history_data_get()->next_record_no;
        wait_saved(append(1), 0);
        assert(ui_history_data_get()->records[0].record_no == next_id);
        assert(storage_worker_shutdown());
    }
    else if (strcmp(argv[1], "dump") == 0) {
        const ui_history_store_t *store = ui_history_data_get();
        assert(ui_history_data_is_available());
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
