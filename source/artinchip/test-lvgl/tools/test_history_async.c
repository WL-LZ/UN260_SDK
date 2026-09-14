#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "un260/lv_system/ui_history_data.h"
#include "un260/counting/counting_history_service.h"

static pthread_t ui_thread;
static pthread_mutex_t gate_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_changed = PTHREAD_COND_INITIALIZER;
static bool held;
static bool entered;
static bool fail_worker_init;
static bool fail_result_alloc;
static atomic_bool fail_submit_alloc;
static unsigned worker_init_calls;
static unsigned read_calls;
static int read_error;
static unsigned result_allocations;
static unsigned result_frees;
static void *result_pointer;

void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size)
{
    if (atomic_exchange(&fail_submit_alloc, false)) return NULL;
    return __real_malloc(size);
}

static bool test_worker_init(void)
{
    worker_init_calls++;
    return !fail_worker_init && storage_worker_init();
}

static FILE *test_fopen(const char *path, const char *mode)
{
    assert(!pthread_equal(pthread_self(), ui_thread));
    pthread_mutex_lock(&gate_lock);
    entered = true;
    pthread_cond_broadcast(&gate_changed);
    while (held) pthread_cond_wait(&gate_changed, &gate_lock);
    pthread_mutex_unlock(&gate_lock);
    read_calls++;
    if (read_error) { errno = read_error; return NULL; }
    return fopen(path, mode);
}

static void *test_malloc(size_t size)
{
    assert(!pthread_equal(pthread_self(), ui_thread));
    assert(size == sizeof(ui_history_store_t));
    if (fail_result_alloc) return NULL;
    result_pointer = malloc(size);
    if (result_pointer) result_allocations++;
    return result_pointer;
}

static void test_free(void *pointer)
{
    assert(pthread_equal(pthread_self(), ui_thread));
    assert(pointer == result_pointer);
    result_frees++;
    free(pointer);
}

#define storage_worker_init test_worker_init
#define fopen test_fopen
#define malloc test_malloc
#define free test_free
#include "un260/lv_system/ui_history_data_fs.c"
#undef storage_worker_init
#undef fopen
#undef malloc
#undef free

void currency_state_get_active_code(char code[4]) { memcpy(code, "USD", 4); }
void machine_time_get(machine_time_value_t *value)
{
    *value = (machine_time_value_t){ 2026, 9, 14, 8, 33, 26 };
}
bool machine_time_is_valid(const machine_time_value_t *value)
{
    return value->year >= 2000 && value->month >= 1 && value->month <= 12 &&
           value->day >= 1 && value->day <= 31 && value->hour < 24 &&
           value->minute < 60 && value->second < 60;
}

static void gate_hold(void)
{
    pthread_mutex_lock(&gate_lock);
    held = true;
    entered = false;
    pthread_mutex_unlock(&gate_lock);
}

static void gate_wait_entered(void)
{
    pthread_mutex_lock(&gate_lock);
    while (!entered) pthread_cond_wait(&gate_changed, &gate_lock);
    pthread_mutex_unlock(&gate_lock);
}

static void gate_release(void)
{
    pthread_mutex_lock(&gate_lock);
    held = false;
    pthread_cond_broadcast(&gate_changed);
    pthread_mutex_unlock(&gate_lock);
}

static void wait_initialized(void)
{
    for (unsigned i = 0; i < 30000 && !ui_history_data_init_poll(); i++) usleep(100);
    assert(ui_history_data_is_initialized());
}

static void assert_pending(void)
{
    ui_history_record_t record = { .valid = true, .pcs = 2, .amount = 20 };
    ui_history_record_t out;
    uint32_t id = 1;
    const ui_history_store_t *store = ui_history_data_get();
    assert(!ui_history_data_is_initialized());
    assert(!ui_history_data_init_poll());
    assert(!ui_history_data_poll(1234));
    assert(!ui_history_data_is_available());
    assert(!ui_history_data_can_accept());
    assert(ui_history_data_status() == STORAGE_JOB_PENDING);
    assert(store->record_count == 0 && store->total_notes_counted == 0);
    assert(ui_history_total_notes_counted_get() == 0);
    assert(!ui_history_record_append_snapshot(&record, 2));
    assert(!ui_history_record_toggle_selected(0));
    assert(!ui_history_record_set_selected(0, true));
    assert(!ui_history_record_get(0, &out));
    assert(!ui_history_record_get_by_no(1, &out));
    assert(ui_history_record_selected_count_get() == 0);
    assert(ui_history_record_selected_first_index_get() == -1);
    ui_history_total_notes_counted_set(999);
    ui_history_total_notes_counted_clear();
    ui_history_record_clear_selected();
    ui_history_record_set_all_selected(true);
    assert(!ui_history_record_delete_selected());
    assert(!ui_history_record_delete_records(&id, 1));
    assert(ui_history_last_commit_id() == 0);
    assert(store->record_count == 0 && store->total_notes_counted == 0);
    assert(g_history_accepted.record_count == 0 && g_history_durable.record_count == 0);
}

static void assert_terminal(bool available, unsigned records, unsigned total)
{
    assert(ui_history_data_is_initialized());
    assert(ui_history_data_is_available() == available);
    assert(ui_history_data_status() == (available ? STORAGE_JOB_SUCCEEDED : STORAGE_JOB_FAILED));
    assert(ui_history_data_get()->record_count == records);
    assert(ui_history_total_notes_counted_get() == total);
    assert(g_history_load_job == 0 && g_history_load_result == NULL);
    assert(result_allocations == result_frees);
    unsigned reads = read_calls;
    unsigned init_calls = worker_init_calls;
    for (unsigned i = 0; i < 100; i++) {
        ui_history_data_init_async();
        ui_history_data_init();
        assert(ui_history_data_init_poll());
        assert(!ui_history_data_poll(2000 + i));
        assert(ui_history_data_is_initialized());
    }
    assert(read_calls == reads && worker_init_calls == init_calls);
}

static bool unrelated_job(const void *snapshot, size_t size)
{
    assert(size == 1 && *(const char *)snapshot == 7);
    return true;
}

static bool blocked_job(const void *snapshot, size_t size)
{
    assert(size == 1 && *(const char *)snapshot == 7);
    pthread_mutex_lock(&gate_lock);
    entered = true;
    pthread_cond_broadcast(&gate_changed);
    while (held) pthread_cond_wait(&gate_changed, &gate_lock);
    pthread_mutex_unlock(&gate_lock);
    return true;
}

int main(int argc, char **argv)
{
    assert(argc >= 2);
    ui_thread = pthread_self();
    const char *scenario = argv[1];
    unsigned expected = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 0;
    bool available = strcmp(scenario, "bad") != 0 && strcmp(scenario, "read-error") != 0;
    const char unrelated_payload = 7;
    storage_job_id_t other[STORAGE_WORKER_CAPACITY] = { 0 };

    if (strcmp(scenario, "sync") == 0) {
        ui_history_data_init();
        assert_terminal(true, expected, 400 + expected);
    } else if (strcmp(scenario, "queue-full") == 0) {
        assert(storage_worker_init());
        for (unsigned i = 0; i < STORAGE_WORKER_CAPACITY; i++) {
            assert(storage_worker_submit(unrelated_job, &unrelated_payload, 1, &other[i]));
            assert(storage_worker_wait(other[i]) == STORAGE_JOB_SUCCEEDED);
        }
        ui_history_data_init_async();
        assert_terminal(false, 0, 0);
        for (unsigned i = 0; i < STORAGE_WORKER_CAPACITY; i++) assert(storage_worker_release(other[i]));
    } else if (strcmp(scenario, "worker-failure") == 0 ||
               strcmp(scenario, "submit-failure") == 0 ||
               strcmp(scenario, "result-failure") == 0) {
        fail_worker_init = strcmp(scenario, "worker-failure") == 0;
        atomic_store(&fail_submit_alloc, strcmp(scenario, "submit-failure") == 0);
        fail_result_alloc = strcmp(scenario, "result-failure") == 0;
        ui_history_data_init_async();
        wait_initialized();
        assert_terminal(false, 0, 0);
    } else {
        bool behind_job = strcmp(scenario, "queued") == 0;
        bool early_record = strcmp(scenario, "early-record") == 0;
        counting_session_state_t session = { 0 };
        counting_sim_t sim = { 0 };
        if (strcmp(scenario, "read-error") == 0) read_error = EACCES;
        gate_hold();
        if (behind_job) {
            assert(storage_worker_init());
            assert(storage_worker_submit(blocked_job, &unrelated_payload, 1, &other[0]));
            gate_wait_entered();
        }
        ui_history_data_init_async();
        if (!behind_job) gate_wait_entered();
        storage_job_id_t load_id = g_history_load_job;
        for (unsigned i = 0; i < 30; i++) {
            ui_history_data_init_async();
            assert_pending();
            usleep(1000);
        }
        assert(g_history_load_job == load_id);
        if (early_record) {
            session.history_record.valid = true;
            session.history_record.end_seen = true;
            session.history_record.pcs = 2;
            session.history_record.amount = 20;
            assert(counting_history_try_commit(&session, &sim, 1000) == COUNTING_HISTORY_COMMIT_PENDING);
            assert(!session.history_record.valid);
            assert(counting_history_poll_commit(&session, &sim, 1001) == COUNTING_HISTORY_COMMIT_PENDING);
            assert(!counting_history_can_start());
            assert(ui_history_last_commit_id() == 0);
        }
        gate_release();
        /* Even a completed worker must not expose partly/fully loaded state
         * before the UI thread explicitly publishes the private snapshot. */
        assert(storage_worker_wait(load_id) == STORAGE_JOB_SUCCEEDED);
        assert(!ui_history_data_is_initialized());
        assert(ui_history_data_get()->record_count == 0);
        assert(ui_history_total_notes_counted_get() == 0);
        wait_initialized();
        unsigned total = available && strcmp(scenario, "missing") != 0 ? 400 + expected : 0;
        assert_terminal(available, expected, total);
        if (early_record) {
            assert(counting_history_poll_commit(&session, &sim, 2000) == COUNTING_HISTORY_COMMIT_PENDING);
            storage_job_id_t commit = ui_history_last_commit_id();
            assert(commit != 0 && storage_worker_wait(commit) == STORAGE_JOB_SUCCEEDED);
            assert(ui_history_data_poll(2001));
            assert(counting_history_poll_commit(&session, &sim, 2002) == COUNTING_HISTORY_COMMIT_SAVED);
            assert(ui_history_total_notes_counted_get() == total + 2);
            assert(ui_history_data_get()->record_count == (expected < 100 ? expected + 1 : 100));
            assert(ui_history_data_get()->records[0].pcs == 2);
            assert(ui_history_data_get()->records[0].record_no == expected + 1);
        }
        if (behind_job) assert(storage_worker_release(other[0]));
    }
    /* Every terminal result frees the read slot, including corrupt/OOM cases. */
    assert(storage_worker_init());
    for (unsigned i = 0; i < STORAGE_WORKER_CAPACITY; i++) {
        assert(storage_worker_submit(unrelated_job, &unrelated_payload, 1, &other[i]));
        assert(storage_worker_wait(other[i]) == STORAGE_JOB_SUCCEEDED);
    }
    for (unsigned i = 0; i < STORAGE_WORKER_CAPACITY; i++) assert(storage_worker_release(other[i]));
    assert(storage_worker_shutdown());
    printf("PASS: history async %s (%u records), private publication, no pending writes, one load, released ownership\n",
           scenario, expected);
    return 0;
}
